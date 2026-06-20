#ifndef CLOCK_DCF77_H
#define CLOCK_DCF77_H

#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#define DCF77_ASSUME_START_YEAR 2000
#define DCF77_IO 13

/* Decoded DCF77 frame. This already holds every field the parser produces;
 * the raw bit array and the edge-timing state live privately in clock_dcf77.c. */
typedef struct
{
    bool A1_switch_dst;      // Announcement of DST change in the next hour
    bool dst;                // Daily-Savings time (MEZ) is in effect; MEZ = Winter time, MESZ = Summer time
    bool A2_insert_leap_sec; // Announcement of insertion of a leap second in the next hour
    int minute;              // The minute (0-59)
    int hour;                // The hour (0-23)
    int day;                 // The day of the month (1-31)
    int weekday;             // The day of the week (1-7, where 1 = Monday, 7 = Sunday)
    int month;               // The month (1-12)
    int year;                // The year (e.g., 2026; starting from 2000)

    bool valid;              // Indicates whether the decoded time is valid
} clock_dcf77_time_t;

typedef enum {
    NO_DCF77_EVENT = 0,
    VALID_ZERO,
    VALID_ONE,
    VALID_SECOND,
    VALID_MINUTE,
    INVALID
} clock_dcf77_event_t;

bool clock_dcf77_init(void);

#endif /* CLOCK_DCF77_H */
