#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <stdlib.h>
#include <stdio.h>

/**************************** Configuration ****************************/

#include "FreeRTOS.h"
/* This is FreeRTOS dependent */
#define CLOCK_DCF77_SAMPLE_TASK_PRIORITY    (tskIDLE_PRIORITY + 3UL)
#define CLOCK_CONTROL_TASK_PRIORITY         (tskIDLE_PRIORITY + 2UL)

#define CLOCK_NEED_MUTEX                // We need Mutexes, since GUI can query other state

#define CLOCK_CONTROL_UPDATE_DELAY_MS        7
#define CLOCK_DCF77_SAMPLE_INTERVAL_DELAY_MS 10


/**************************** No need to change below ****************************/

#define ERROR(err, str)                        \
    do                                         \
    {                                          \
        fprintf(stderr, "ERROR (%s:%d): %s\n", \
                __FILE__, __LINE__, (str));    \
        exit(err);                             \
    } while (0)

#ifndef MIN
#  define MIN(a,b)     ((a) < (b)) ? (a) : (b)
#endif
#ifndef MAX
#  define MAX(a,b)     ((a) > (b)) ? (a) : (b)
#endif

/* Macro to get length of declared array at compile-time */
#define ARRAY_LEN(array) (sizeof(array)/sizeof(array[0]))


/* The type for a (x, y) position, each between 0 and screen-width/-height */
typedef struct {
    int x;
    int y;
} pos_t;

/* The type for a box definition upper-left and lower-right corner */
typedef struct {
    pos_t upper_left;
    pos_t lower_right;
} box_t;

#endif /* __CLOCK_H__ */