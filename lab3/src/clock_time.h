#ifndef __CLOCK_TIME_H__
#define __CLOCK_TIME_H__

#include <pico/types.h>

#define CLOCK_TIME_START_SECOND 0
#define CLOCK_TIME_START_MINUTE 0
#define CLOCK_TIME_START_HOUR   0

#define CLOCK_TIME_START_DAY    1
#define CLOCK_TIME_START_MONTH  1
#define CLOCK_TIME_START_YEAR   2026

#define CLOCK_TIME_START_OFFSET_HOUR 0

typedef enum timezones {
    TIMEZONE_UTC = 0,
    TIMEZONE_WET,
    TIMEZONE_WEST,
    TIMEZONE_EST,
    TIMEZONE_COUNT
} timezones_t;

/**
 * @brief Initialization of component. Needs to be called prior
 * to any other clock_time_* functions.
 */
bool clock_time_init(void);

/**
 * @brief Functions to set and retrieve the time:
 * The internal Time is always in UTC (or Greenwich Mean Time)
 * The local time is derived from one of the TIMEZONE_COUNT timezones
 * defined in the timezone_def_t type.
 * 
 * Please note, we only adapt for HOURS -- there's also time-zones, like
 * the Nepal Time NPT, which is UTC+5:45 or Newfoundland time NST as UTC-3:30
 * Also, we do not (yet) account for days, which might wrap if we change
 * the local timezone.
 */
void clock_time_set_utc(int hour, int minute, int second);
void clock_time_get_utc(int * hour, int * minute, int * second);
void clock_time_get_local(int * hour, int * minute, int * second);
void clock_time_get_local_date(int * hour, int * minute, int * second, int * day, int * month, int * year);

void clock_time_set_from_dcf77(int hour, int minute, int day, int month, int year);
void clock_time_set_date(int day, int month, int year);
void clock_time_get_date(int * day, int * month, int * year);

/* Weekday handling (1 = Monday, ..., 7 = Sunday), set from the DCF77 frame,
 * used for the weekday display (Req 2.1). */
void clock_time_set_weekday(int weekday);
void clock_time_get_weekday(int * weekday);

bool clock_time_inc_second(uint64_t tick_us);

void clock_time_change_hour_utc(int change_value);
void clock_time_change_minute_utc(int change_value);

void clock_time_set_timezone(timezones_t tz);

#endif /* __CLOCK_TIME_H__ */