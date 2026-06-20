/**
 * @file Implements reading the on-board QMI8658 IMU and classifying the
 *       board tilt (Req 1.5), using the provided qmi8658 driver library.
 *
 * IMPORTANT DESIGN NOTE
 * ---------------------
 * The qmi8658 driver uses BLOCKING I2C calls (i2c_read_blocking /
 * i2c_write_blocking) that have NO timeout. If the sensor is missing, slow or
 * the (breadboard) bus glitches, such a call can stall for a long time. We
 * must NEVER let that stall the control task, because the free-running clock
 * has to keep ticking even when peripherals fail ([Req 1.2]).
 *
 * Therefore all I2C traffic happens in a dedicated, LOW-priority task here.
 * That task reads the accelerometer periodically and stores the resulting
 * tilt in 'current_orientation'. The control task only calls
 * clock_imu_get_orientation(), which just returns that cached value and never
 * touches I2C -- so it can never block.
 *
 * With the default driver settings the accelerometer is read in milli-g (mg):
 * the axis aligned with gravity reads ~1000.
 */
#include <pico/stdlib.h>
#include <hardware/i2c.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "qmi8658/qmi8658.h"   // provides IMU_I2C_PORT (== i2c1)
#include "clock.h"
#include "clock_imu.h"

/* QMI8658 I2C pins on the Waveshare RP2350-LCD-1.28 (I2C1). */
#define IMU_I2C_SDA_PIN   6
#define IMU_I2C_SCL_PIN   7
#define IMU_I2C_BAUDRATE  (400 * 1000)

/* Tilt threshold in milli-g. ~350 mg corresponds to roughly 20 degrees. */
#define IMU_TILT_THRESHOLD_MG  350.0f

/* How often the IMU task reads the sensor. */
#define IMU_READ_INTERVAL_MS   50

static qmi8658_dev_t imu_dev;

/* Cached tilt. Written by the IMU task, read by the control task. A single
 * aligned word read/write is atomic on RV32, so 'volatile' is sufficient. */
static volatile imu_orientation_t current_orientation = IMU_NEUTRAL;

static TaskHandle_t imu_task_handle = NULL;


static void clock_imu_task(void *params)
{
    (void)params;

    /* Configure the I2C1 bus the driver expects. Done HERE (after the
     * scheduler has started) so that a missing/slow IMU can never block the
     * boot sequence or the free-running clock. */
    i2c_init(IMU_I2C_PORT, IMU_I2C_BAUDRATE);
    gpio_set_function(IMU_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(IMU_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(IMU_I2C_SDA_PIN);
    gpio_pull_up(IMU_I2C_SCL_PIN);

    vTaskDelay(pdMS_TO_TICKS(100));   // let the sensor power up

    bool ok = (qmi8658_init(&imu_dev) == 0);
    if (!ok)
        printf("clock_imu: QMI8658 init failed -- screen switching disabled\n");

    const TickType_t xDelay = pdMS_TO_TICKS(IMU_READ_INTERVAL_MS);
    while (true)
    {
        if (ok)
        {
            float ax, ay, az;
            if (qmi8658_read_accel(&imu_dev, &ax, &ay, &az) == 0)
            {
                /*
                 * Values are in milli-g. Depending on how the board is mounted
                 * the relevant left/right axis may be ay instead of ax, and the
                 * sign may be flipped. Uncomment to calibrate on the console:
                 *
                 *   printf("acc x=%.0f y=%.0f z=%.0f mg\n", ax, ay, az);
                 */
                float tilt = ax;

                if (tilt > IMU_TILT_THRESHOLD_MG)
                    current_orientation = IMU_TILT_RIGHT;
                else if (tilt < -IMU_TILT_THRESHOLD_MG)
                    current_orientation = IMU_TILT_LEFT;
                else
                    current_orientation = IMU_NEUTRAL;
            }
        }
        vTaskDelay(xDelay);
    }
}

bool clock_imu_init(void)
{
    /* Only CREATE the task here -- no I2C traffic in the boot path. */
    int ret = xTaskCreate(clock_imu_task, "clock_imu",
                          4 * configMINIMAL_STACK_SIZE, NULL,
                          tskIDLE_PRIORITY + 1UL, &imu_task_handle);
    if (ret != pdPASS)
    {
        printf("clock_imu: failed to create task\n");
        return false;
    }
    return true;
}

imu_orientation_t clock_imu_get_orientation(void)
{
    /* Non-blocking: just return the most recent cached value. */
    return current_orientation;
}
