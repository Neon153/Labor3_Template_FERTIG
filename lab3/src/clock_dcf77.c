/**
 * @file Implements the sampling and parsing of the DCF77 signal.
 *  - Setup the GPIO pin 13 as input -- pull-ups/pull-downs are disabled,
 *    the external 10k pull-down on the OpAmp output defines the idle level.
 *  - A high-priority FreeRTOS task samples the signal every 10ms, detects the
 *    positive/negative edges, measures T_low and T_pulse and turns them into
 *    "events" (see Figure 4 in the lab sheet).
 *  - The events are decoded into a 59-bit array; on a valid minute mark the
 *    frame is parsed and the free-running clock is updated.
 *
 * NOTE on signal polarity: our antenna INVERTS the signal. The physical DCF77
 * line is High (H) when idle and drops to Low (L) for each second pulse. After
 * inversion the GPIO therefore reads:
 *     gpio == 1  while the DCF77 line is L  (pulse active / "low phase")
 *     gpio == 0  while the DCF77 line is H  (idle)
 * Throughout this file we work with the de-inverted level "signal_low":
 *     signal_low = (gpio_get(DCF77_IO) != 0)
 */
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "clock.h"
#include "clock_dcf77.h"
#include "clock_time.h"

/********************** Timing windows (milliseconds) **********************/
/* Bit value, measured as T_low on the POSITIVE edge (end of the low phase). */
#define DCF77_ZERO_MS        100    /* short pulse  -> logical 0 */
#define DCF77_ONE_MS         200    /* long  pulse  -> logical 1 */
#define DCF77_BIT_TOL_MS      30    /* tolerance per lab sheet (+/- 30 ms)   */

/* Second/minute mark, measured as T_pulse on the NEGATIVE edge
 * (distance between two consecutive negative edges). */
#define DCF77_SECOND_MS     1000
#define DCF77_SECOND_TOL_MS   30    /* 1 s +/- 30 ms  */
#define DCF77_MINUTE_MS     2000
#define DCF77_MINUTE_TOL_MS  100    /* 2 s +/- 100 ms */

/********************** LED configuration **********************
 * !!! VERIFY THESE PIN NUMBERS AGAINST board_setup.png / your Lab 2 wiring !!!
 * They were placeholders in the template; set them to your actual LED GPIOs.
 * Set DCF77_LED_ACTIVE_HIGH to 0 if your RGB-LED is common-anode (active low).
 */
#define DCF77_LED_MONO_PIN    1
#define DCF77_LED_R_PIN       5
#define DCF77_LED_G_PIN       3
#define DCF77_LED_B_PIN       4
#define DCF77_LED_ACTIVE_HIGH 1

/* BCD weights: bits are transmitted LSB first. */
static const int bcd_1s_weights[4]  = {1, 2, 4, 8};
static const int bcd_10s_weights[4] = {10, 20, 40, 80};

static TaskHandle_t clock_dcf77_handle_sample = NULL;

typedef struct
{
    uint8_t bits[59];
    int bit_index;   // number of bits collected in the current minute (0..59)
} DCF77_Bitarray;

static DCF77_Bitarray dcf77_bitarray;

/* True once we received our first complete, valid minute -> the error/no-info
 * RED LED may stay off (Req 1.2). */
static bool dcf77_minute_received = false;

/* Tracks the monochrome LED state (toggled every second, Req 1.2). */
static bool dcf77_mono_state = false;


/********************** LED helpers **********************/

static inline void dcf77_led_write(int pin, bool on)
{
#if DCF77_LED_ACTIVE_HIGH
    gpio_put(pin, on);
#else
    gpio_put(pin, !on);
#endif
}

static void dcf77_leds_init(void)
{
    const int pins[] = {DCF77_LED_MONO_PIN, DCF77_LED_R_PIN, DCF77_LED_G_PIN, DCF77_LED_B_PIN};
    for (unsigned i = 0; i < ARRAY_LEN(pins); ++i)
    {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        dcf77_led_write(pins[i], false);
    }
}

/* Drive the RGB-LED. Used for debugging the reception (Req 1.2):
 *   RED   -> no information / error
 *   BLUE  -> a ZERO bit was decoded
 *   GREEN -> a ONE  bit was decoded
 */
static void dcf77_set_rgb(bool r, bool g, bool b)
{
    dcf77_led_write(DCF77_LED_R_PIN, r);
    dcf77_led_write(DCF77_LED_G_PIN, g);
    dcf77_led_write(DCF77_LED_B_PIN, b);
}

static void dcf77_toggle_mono(void)
{
    dcf77_mono_state = !dcf77_mono_state;
    dcf77_led_write(DCF77_LED_MONO_PIN, dcf77_mono_state);
}


/********************** Bit array handling **********************/

static void dcf77_init(DCF77_Bitarray *bitarray)
{
    assert(NULL != bitarray);

    bitarray->bit_index = 0;
    for (int i = 0; i < 59; ++i)
        bitarray->bits[i] = 0;
}

static void dcf77_reset(DCF77_Bitarray *bitarray)
{
    assert(NULL != bitarray);
    dcf77_init(bitarray);
}


/********************** Decoding **********************/

/* Decode a BCD field that is transmitted LSB first.
 *
 * @param bits    the bit array (index == DCF77 bit number)
 * @param start   index of the least significant 1s-bit
 * @param n_ones  number of 1s-digit bits (weights 1,2,4,8)
 * @param n_tens  number of 10s-digit bits (weights 10,20,40,80)
 */
static int dcf77_decode_bcd(const uint8_t *bits, int start, int n_ones, int n_tens)
{
    int val = 0;
    for (int i = 0; i < n_ones; ++i)
        val += bits[start + i] * bcd_1s_weights[i];
    for (int i = 0; i < n_tens; ++i)
        val += bits[start + n_ones + i] * bcd_10s_weights[i];
    return val;
}

/* Even-parity check over an inclusive range that INCLUDES the parity bit:
 * the total number of ones must be even. */
static bool dcf77_even_parity_ok(const uint8_t *bits, int from, int to_incl)
{
    int ones = 0;
    for (int i = from; i <= to_incl; ++i)
        ones += (bits[i] != 0);
    return (ones % 2) == 0;
}

/**
 * Parse the 59-bit frame and, if valid, return the decoded time/date.
 *
 * @param bitarray [in]    the 59 bits gathered
 * @param time     [out]   the decoded time and date with further info
 *
 * @return false           parsing failed (parity / structural / range error)
 * @return true            parsing succeeded and time is valid
 */
static bool dcf77_parse_frame(const DCF77_Bitarray *bitarray, clock_dcf77_time_t *time)
{
    assert(bitarray != NULL);
    assert(time != NULL);

    const uint8_t *b = bitarray->bits;

    /* --- Structural checks --- */
    if (b[20] != 1)            // Start-bit S must always be 1
        return false;
    if (b[17] == b[18])        // time-zone bits: must be 01 or 10, never 00/11
        return false;

    time->dst                = (b[17] == 1 && b[18] == 0); // 10 = MESZ (summer)
    time->A1_switch_dst      = (b[16] != 0);
    time->A2_insert_leap_sec = (b[19] != 0);

    /* --- Parity checks (even parity, P1/P2/P3) --- */
    if (!dcf77_even_parity_ok(b, 21, 28)) return false;   // P1: minutes
    if (!dcf77_even_parity_ok(b, 29, 35)) return false;   // P2: hours
    if (!dcf77_even_parity_ok(b, 36, 58)) return false;   // P3: date

    /* --- BCD decoding (see table on lab sheet page 2) --- */
    int minute  = dcf77_decode_bcd(b, 21, 4, 3);  // bits 21..27
    int hour    = dcf77_decode_bcd(b, 29, 4, 2);  // bits 29..34
    int day     = dcf77_decode_bcd(b, 36, 4, 2);  // bits 36..41
    int wday    = dcf77_decode_bcd(b, 42, 3, 0);  // bits 42..44 (plain 3-bit)
    int month   = dcf77_decode_bcd(b, 45, 4, 1);  // bits 45..49
    int year    = dcf77_decode_bcd(b, 50, 4, 4);  // bits 50..57 (00..99)

    /* --- Range checks (Req 1.3) --- */
    if (hour  < 0 || hour  > 23) return false;
    if (minute < 0 || minute > 59) return false;
    if (day   < 1 || day   > 31) return false;
    if (month < 1 || month > 12) return false;
    if (wday  < 1 || wday  > 7)  return false;

    time->minute  = minute;
    time->hour    = hour;
    time->day     = day;
    time->weekday = wday;
    time->month   = month;
    time->year    = DCF77_ASSUME_START_YEAR + year;
    time->valid   = true;

    return true;
}

/**
 * Analyse a single DCF77 event and build up the bit array.
 *
 *  - On a positive edge we know the BIT value (VALID_ZERO / VALID_ONE) and
 *    store it (one bit per second).
 *  - On a negative edge we only learn whether a normal second (VALID_SECOND)
 *    or the 2-second minute mark (VALID_MINUTE) elapsed.
 *  - On the minute mark the previous 59 bits (indices 0..58) are complete:
 *    parse them and, if valid, update the clock. Then restart the collection.
 *  - Any INVALID timing means we lost sync -> restart and show the error LED.
 */
static inline void dcf77_feed_event(DCF77_Bitarray *bitarray, clock_dcf77_event_t event)
{
    assert(bitarray != NULL);

    switch (event)
    {
    case VALID_ZERO:
    case VALID_ONE:
    {
        uint8_t bit = (event == VALID_ONE) ? 1 : 0;
        if (bitarray->bit_index < 59)
            bitarray->bits[bitarray->bit_index] = bit;
        bitarray->bit_index++;
        // Debug LED: BLUE for a 0, GREEN for a 1 (also confirms good antenna
        // orientation, with RED off, before the first full minute arrives).
        dcf77_set_rgb(false, (bit == 1), (bit == 0));
        break;
    }

    case VALID_SECOND:
        // A normal second elapsed; the bit value already arrived on the
        // preceding positive edge, so nothing to collect here.
        break;

    case VALID_MINUTE:
    {
        // Minute mark: bits 0..58 of the finished minute are complete.
        if (bitarray->bit_index == 59)
        {
            clock_dcf77_time_t t;
            memset(&t, 0, sizeof(t));
            if (dcf77_parse_frame(bitarray, &t))
            {
                clock_time_set_from_dcf77(t.hour, t.minute, t.day, t.month, t.year);
                clock_time_set_weekday(t.weekday);
                dcf77_minute_received = true;
                dcf77_set_rgb(false, false, false);   // valid -> RED off (Req 1.2/1.3)
            }
            else
            {
                dcf77_set_rgb(true, false, false);    // parity/range error -> RED on
            }
        }
        // Start collecting the new minute (bit 0 starts now).
        dcf77_reset(bitarray);
        break;
    }

    case INVALID:
    default:
        // Out-of-tolerance timing -> lost synchronisation. Restart and signal
        // the error; the free-running clock keeps going untouched.
        dcf77_reset(bitarray);
        dcf77_set_rgb(true, false, false);
        break;
    }
}


/********************** Sampling task **********************/

static void DCF77_sample_fn(void *params)
{
    (void)params;
    const TickType_t xDelay = pdMS_TO_TICKS(CLOCK_DCF77_SAMPLE_INTERVAL_DELAY_MS);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    dcf77_leds_init();
    dcf77_set_rgb(true,  false, false); vTaskDelay(pdMS_TO_TICKS(800));  // sollte ROT
    dcf77_set_rgb(false, true,  false); vTaskDelay(pdMS_TO_TICKS(800));  // sollte GRÜN
    dcf77_set_rgb(false, false, true ); vTaskDelay(pdMS_TO_TICKS(800));  // sollte BLAU
    dcf77_set_rgb(false, false, false);
    dcf77_set_rgb(true, false, false);   // start: RED on = no information yet

    // Initialer, roher GPIO-Status
    bool initial_raw_low = (gpio_get(DCF77_IO) != 0);
    
    // Entprell-Statusvariablen (Sliding Window)
    bool debounced_low   = initial_raw_low;
    bool last_low        = debounced_low;
    
    // Schieberegister für die letzten 8 Samples (0xFF wenn Low/Puls, 0x00 wenn High/Idle)
    uint8_t shift_reg    = initial_raw_low ? 0xFF : 0x00; 

    uint32_t t_neg_edge  = to_ms_since_boot(get_absolute_time()); // last negative edge
    uint32_t t_low_start = t_neg_edge;                            // start of current low phase
    uint32_t last_mono_ms = t_neg_edge;                           // mono-LED heartbeat timer

    while (true)
    {
        uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());
        
        // 1. Rohes Signal einlesen
        bool raw_low = (gpio_get(DCF77_IO) != 0);

        // 2. Sliding Window (Schieberegister aktualisieren)
        shift_reg = (shift_reg << 1) | (raw_low ? 1 : 0);

        // 3. Signal entprellen: Wir schauen uns nur die letzten 3 Bits an (Bitmaske 0x07).
        // Erst wenn 3 Samples in Folge (30ms) denselben Zustand haben, übernehmen wir ihn.
        if ((shift_reg & 0x07) == 0x07) 
        {
            debounced_low = true;  // Signal ist stabil LOW (Pulsphase)
        } 
        else if ((shift_reg & 0x07) == 0x00) 
        {
            debounced_low = false; // Signal ist stabil HIGH (Idlephase)
        }
        // In allen anderen Fällen flackert es gerade -> debounced_low behält den alten Zustand!

        clock_dcf77_event_t event = NO_DCF77_EVENT;

        // 4. Flankenauswertung nun mit dem SAUBEREN (debounced) Signal!
        if (!last_low && debounced_low)
        {
            /* Negative edge (H -> L): a new second pulse begins.
             * T_pulse = time since the previous negative edge. */
            uint32_t t_pulse = current_time_ms - t_neg_edge;
            
            t_neg_edge  = current_time_ms;
            t_low_start = current_time_ms;

            if (t_pulse >= 700 && t_pulse < 1500)
                event = VALID_SECOND;      // ~1 s
            else if (t_pulse >= 1500 && t_pulse < 2500)
                event = VALID_MINUTE;      // ~2 s (Minutenmarke)
            else
                event = INVALID; 
        }
        else if (last_low && !debounced_low)
        {
            /* Positive edge (L -> H): the low pulse ended.
             * T_low decides the bit value. */
            uint32_t t_low = current_time_ms - t_low_start;

            if (t_low >= 30 && t_low < 120) 
            {
                event = VALID_ZERO;        // ~100 ms = 0 (Löst die blaue LED aus)
            } 
            else if (t_low >= 120 && t_low <= 350) 
            {
                event = VALID_ONE;         // ~200 ms = 1 (Löst die grüne LED aus)
            } 
            else 
            {
                event = INVALID;           // Schrott-Pulse
            }
            //printf("Pulsweite: %lu ms | Sekundenabstand: %lu ms\n", t_low, t_pulse);
        }
        
        // Zustand für den nächsten Durchlauf merken
        last_low = debounced_low;

        if (event != NO_DCF77_EVENT)
            dcf77_feed_event(&dcf77_bitarray, event);

        // Monochrome LED toggles every second as a "task alive" heartbeat
        // (Req 1.2). Time-based, so it blinks even without a DCF77 signal.
        if (current_time_ms - last_mono_ms >= 1000)
        {
            last_mono_ms = current_time_ms;
            dcf77_toggle_mono();
        }

        vTaskDelayUntil(&xLastWakeTime, xDelay);
    }
}
bool clock_dcf77_init(void)
{
    int ret;

    dcf77_init(&dcf77_bitarray);

    // Initialize DCF77 IO as input. The external 10k pull-down defines the
    // idle level, so the internal pulls are disabled.
    gpio_init(DCF77_IO);
    gpio_set_function(DCF77_IO, GPIO_FUNC_SIO);
    gpio_set_dir(DCF77_IO, GPIO_IN);
    gpio_disable_pulls(DCF77_IO);

    // Sample Task is the highest-priority Task and has configMAX_PRIORITIES-2 (one less than timer task)
    ret = xTaskCreate(DCF77_sample_fn, "DCF77_sample", 2*configMINIMAL_STACK_SIZE, NULL, CLOCK_DCF77_SAMPLE_TASK_PRIORITY, &clock_dcf77_handle_sample);
    if (ret != pdPASS)
        ERROR(ret, "xTaskCreate for DCF77_sample_fn returned != pdPASS");
    assert(clock_dcf77_handle_sample != NULL);
    // vTaskCoreAffinitySet(clock_dcf77_handle_sample, (0x1 << 1));

    return true;
}
