/**
 * @file Reads the on-board IMU (QMI8658) and reports the tilt orientation
 *       of the board, used to switch between Analog and Digital display
 *       (Req 1.5).
 */
#ifndef __CLOCK_IMU_H__
#define __CLOCK_IMU_H__

#include <stdbool.h>

/* Tilt orientation derived from the accelerometer. */
typedef enum {
    IMU_NEUTRAL = 0,   // board roughly level -> keep current screen
    IMU_TILT_LEFT,     // tilted left  -> Analog display  (Screen 1)
    IMU_TILT_RIGHT     // tilted right -> Digital display (Screen 2)
} imu_orientation_t;

/**
 * Initialize the IMU. Must be called once before clock_imu_get_orientation().
 *
 * @return true on success.
 */
bool clock_imu_init(void);

/**
 * Read the accelerometer and classify the current tilt orientation.
 *
 * @return the current imu_orientation_t.
 */
imu_orientation_t clock_imu_get_orientation(void);

#endif /* __CLOCK_IMU_H__ */
