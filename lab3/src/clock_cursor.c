/**
 * @file Implements the cursor allowing to set the time:
 *  - when moving the Joystick, a cursor (two lines) should appear
 *    (see clock_gui.c)
 *  Using the joystack, one shall edit (change) the hour and minute:
 *  - when not in editing mode:
 *    - Pulling the joystick position of the cursor should change:
 *       pull up: decrease y  /  pull down: increase y
 *       pull left: decrease x / pull right: increase x
 *    - when clicking on hour / minute: Select Edit mode for hour or minute
 *  - when in editing mode:
 *    - Pulling the joystick should change the hour / minute:
 *       pull up: increase hour or minute / pull down: decrease hour or minute
 *    - When clicking: Get out of Editing mode
 */
#include <stdio.h>
#include <hardware/gpio.h>
#include <hardware/adc.h>
#include <pico/time.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "clock.h"
#include "clock_time.h"
#include "clock_cursor.h"

#define CURSOR_UPDATE_TIME_US        (30 * 1000) // 30 ms
#define CURSOR_VISIBLE_TIME_US (5 * 1000 * 1000) // 5 seconds
#define CURSOR_BUTTON_PRESS_TIME_US (200 * 1000) // 200 ms

#ifdef CLOCK_NEED_MUTEX
static SemaphoreHandle_t clock_semaphore_cursor = NULL;
#define LOCK   do { xSemaphoreTake(clock_semaphore_cursor, portMAX_DELAY); } while(0)
#define UNLOCK do { xSemaphoreGive(clock_semaphore_cursor);  } while(0)
#else
#define LOCK
#define UNLOCK
#endif

/*
 * The cursors current state (position, mode of edit, visibility and
 * when that cursor was last visible.
 */
static pos_t cursor_pos = {120, 120};
static bool cursor_visible = true;
static uint64_t cursor_visible_us = 0;

// Static variables to store the button state (whether pressed and time of press)
static bool last_button = false;
static uint64_t last_button_us = 0;

bool clock_cursor_init()
{
    adc_init();
    adc_gpio_init(JOYSTICK_GPIO_X);
    adc_gpio_init(JOYSTICK_GPIO_Y);

    gpio_init(JOYSTICK_GPIO_BUTTON);           // The GPIO connecting button of this joystick
    gpio_set_dir(JOYSTICK_GPIO_BUTTON, false); // Set as input
    gpio_pull_up(JOYSTICK_GPIO_BUTTON);        // Enable Pull-UP Resistor (inverts the button)
#ifdef CLOCK_NEED_MUTEX
    clock_semaphore_cursor = xSemaphoreCreateMutex();
    assert (clock_semaphore_cursor != NULL);
#endif

    return true;
}

/**
 * Confert adc readout to a sensical pointer change value;
 */
static int16_t adc_to_pos_value(uint16_t adc)
{
    if (adc > (2048 + 1024))
    {
        return 10;
    }
    else if (adc > (2048 + 512))
    {
        return 3;
    }
    else if (adc < 512)
    {
        return -10;
    }
    else if (adc < (2048 - 512))
    {
        return -3;
    }
    return 0;
}

/*
 * Confert adc readout to a sensical pointer change value;
 */
static int16_t adc_to_timechange_value(uint16_t adc)
{
    if (adc > (2048 + 1024))
    {
        return 1;
    }
    else if (adc < 1024)
    {
        return -1;
    }
    return 0;
}


bool clock_cursor_update(uint64_t tick_us)
{
    static uint64_t last_update_time_us = 0;
    static bool update;
    
    update = false;
    if (tick_us - last_update_time_us < CURSOR_UPDATE_TIME_US) {
        return update;
    }
    last_update_time_us = tick_us;

    // Update state
    bool button = gpio_get(JOYSTICK_GPIO_BUTTON);
    adc_select_input(0);
    uint16_t adc_x = adc_read();
    adc_select_input(1);
    uint16_t adc_y = adc_read();
    int16_t diff_x = adc_to_pos_value(adc_x);
    int16_t diff_y = adc_to_pos_value(adc_y);

    // Only if diffs, there is a cursor update
    if (diff_x != 0 || diff_y != 0) {
        cursor_visible_us = tick_us;
        update = true;
    }

    LOCK;
    if (button && !last_button && tick_us - last_button_us > CURSOR_BUTTON_PRESS_TIME_US)
    {
        last_button_us = tick_us;
        cursor_visible_us = tick_us;
        update = true;
    }

    /*
     * Bugfix (Req 1.4): the cursor is VISIBLE while it was active within the
     * last CURSOR_VISIBLE_TIME_US, otherwise it fades out. The original code
     * had this condition inverted. Additionally we must report a GUI update
     * when the visibility actually CHANGES (e.g. it just faded out), otherwise
     * the GUI would never redraw to hide the cursor.
     */
    bool now_visible = (tick_us - cursor_visible_us) < CURSOR_VISIBLE_TIME_US;
    if (now_visible != cursor_visible)
        update = true;
    cursor_visible = now_visible;

    last_button = button;
    cursor_pos.x += diff_x;
    cursor_pos.y += diff_y;
    UNLOCK;
    return update;
}

void clock_cursor_get_state(pos_t *pos, bool *visible)
{
    assert (NULL != pos);
    assert (NULL != visible);

    LOCK;
    pos->x = cursor_pos.x;
    pos->y = cursor_pos.y;
    *visible = cursor_visible;
    UNLOCK;
}

bool clock_cursor_button_pressed(void)
{
    /* The joystick button has a pull-up and shorts to GND when pressed, so a
     * LOW level means "pressed". We report a single edge per press and debounce
     * with the same press-time threshold the cursor uses. */
    static bool last_pressed = false;
    static uint64_t last_edge_us = 0;

    bool pressed = (gpio_get(JOYSTICK_GPIO_BUTTON) == 0);
    uint64_t now = time_us_64();
    bool edge = false;

    if (pressed && !last_pressed && (now - last_edge_us) > CURSOR_BUTTON_PRESS_TIME_US)
    {
        edge = true;
        last_edge_us = now;
    }
    last_pressed = pressed;
    return edge;
}
