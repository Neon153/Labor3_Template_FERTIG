/**
 * @file Time and date handling
 */
#include <pico/stdlib.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "clock.h"
#include "clock_time.h"

/********************** Type definitions **********************/
typedef struct timezone_def {
    char name_short[8];
    char name[32];
    int hour_offset_UTC;
} timezone_def_t;


/********************** Local variables **********************/
static int seconds = CLOCK_TIME_START_SECOND;
static int minutes = CLOCK_TIME_START_MINUTE;
static int hours = CLOCK_TIME_START_HOUR;

static int days = CLOCK_TIME_START_DAY;
static int months = CLOCK_TIME_START_MONTH;
static int years = CLOCK_TIME_START_YEAR;
static int weekday = 1; // 1 = Monday ... 7 = Sunday

static int timezone_offset_hours = CLOCK_TIME_START_OFFSET_HOUR;
static uint64_t last_tick_us = 0;

static timezone_def_t timezone_def[] = {
    {"UTC",   "Coordinated Universal Time", 0},
    {"WET",   "Western European Time", 0},
    {"WEST",  "Western European Summer Time", 1},
    {"EST",   "Eastern Standard Time", -5}
};

#ifdef CLOCK_NEED_MUTEX
static SemaphoreHandle_t clock_semaphore_time = NULL;
#define LOCK   do { xSemaphoreTake(clock_semaphore_time, portMAX_DELAY); } while(0)
#define UNLOCK do { xSemaphoreGive(clock_semaphore_time);  } while(0)
#else
#define LOCK
#define UNLOCK
#endif

bool clock_time_init(void)
{
    clock_semaphore_time = xSemaphoreCreateMutex();
    assert (NULL != clock_semaphore_time);

    return true;
}

bool clock_time_inc_second(uint64_t tick_us)
{
    if ((tick_us - last_tick_us) < (1000*1000))
        return false;

    LOCK;
    last_tick_us += (1000*1000);
    if (seconds++ == 59)
    {
        if (minutes++ == 59)
        {
            if (hours++ == 23)
                hours = 0;
            minutes = 0;
        }
        seconds = 0;
    }
    UNLOCK;

    return true;
}

void clock_time_set_utc(int hour, int minute, int second) {
    assert (hour >= 0 && hour < 24);
    assert (minute >= 0 && minute < 60);
    assert (second >= 0 && second < 60);

    LOCK;
    hours = hour;
    minutes = minute;
    seconds = second;
    UNLOCK;
}

void clock_time_get_utc(int * hour, int * minute, int * second) {
    assert (hour != NULL);
    assert (minute != NULL);
    assert (second != NULL);

    LOCK;
    *hour = hours;
    *minute = minutes;
    *second = seconds;
    UNLOCK;
}

void clock_time_get_local(int * hour, int * minute, int * second) {
    assert (hour != NULL);
    assert (minute != NULL);
    assert (second != NULL);

    LOCK;
    *hour = (hours + timezone_offset_hours) % 24;
    *minute = minutes;
    *second = seconds;
    UNLOCK;
}

void clock_time_get_local_date(int * hour, int * minute, int * second, int * day, int * month, int * year) {
    assert (hour != NULL);
    assert (minute != NULL);
    assert (second != NULL);
    assert (day != NULL);
    assert (month != NULL);
    assert (year != NULL);

    LOCK;
    *hour = (hours + timezone_offset_hours) % 24;
    *minute = minutes;
    *second = seconds;
    *day = days;
    *month = months;
    *year = years;
    UNLOCK;

}

void clock_time_set_from_dcf77(int current_hour, int current_minute, int current_day, int current_month, int current_year) {    
    assert (current_hour >= 0 && current_hour < 24);
    assert (current_minute >= 0 && current_minute < 60);
    assert (current_day >= 1 && current_day <= 31);
    assert (current_month >= 1 && current_month <= 12);
    assert (current_year >= 1970); // We only support years from 1970 onwards

    LOCK;
    hours = current_hour;
    minutes = current_minute;
    days = current_day;
    months = current_month;
    years = current_year;

    seconds = 0;
    last_tick_us = time_us_64();
    UNLOCK;
}

void clock_time_set_date(int current_day, int current_month, int current_year) {
    assert (current_day >= 1 && current_day <= 31);
    assert (current_month >= 1 && current_month <= 12);
    assert (current_year >= 1970); // We only support years from 1970 onwards

    LOCK;
    days = current_day;
    months = current_month;
    years = current_year;
    UNLOCK;
}

void clock_time_get_date(int * day, int * month, int * year) {
    assert (day != NULL);
    assert (month != NULL);
    assert (year != NULL);

    LOCK;
    *day = days;
    *month = months;
    *year = years;
    UNLOCK;
}

void clock_time_set_weekday(int current_weekday) {
    assert (current_weekday >= 1 && current_weekday <= 7);

    LOCK;
    weekday = current_weekday;
    UNLOCK;
}

void clock_time_get_weekday(int * out_weekday) {
    assert (out_weekday != NULL);

    LOCK;
    *out_weekday = weekday;
    UNLOCK;
}

void clock_time_set_timezone(timezones_t tz) {
    assert (tz >= 0 && tz < TIMEZONE_COUNT);

    LOCK;
    timezone_offset_hours = timezone_def[tz].hour_offset_UTC;
    UNLOCK;
}


void clock_time_change_hour_utc(int change_value) {
    assert (change_value >= 0 && change_value < 24);

    LOCK;
    hours += change_value;
    UNLOCK;
}

void clock_time_change_minute_utc(int change_value) {
    assert (change_value >= 0 && change_value < 60);

    LOCK;
    minutes += change_value;
    UNLOCK;
}
