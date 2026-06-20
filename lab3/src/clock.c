/**
 * @file Implements the main loop initializing components
 *
 * Please note, for handling the button, You may need to add another file,
 * like clock_button.c and clock_button.h
 */
#include "pico/stdlib.h"
#include "Config/DEV_Config.h"
#include "hardware/ticks.h"

#include "clock.h"
#include "clock_cursor.h"
#include "clock_gui.h"
#include "clock_time.h"
#include "clock_dcf77.h"
#include "clock_imu.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

void control_fn(void *)
{
    const TickType_t xDelay = pdMS_TO_TICKS(CLOCK_CONTROL_UPDATE_DELAY_MS);
    static uint64_t tick_us = 0;

    while (true)
    {
        bool update_gui = false;

        tick_us = time_us_64();

        /* Read the IMU and switch the active screen when the board is tilted
         * (Req 1.5). The actual lv_screen_load() happens inside the GUI module
         * (only the control task may call into LVGL); here we just forward the
         * orientation and request a GUI update when it changed. */
        static imu_orientation_t last_orient = IMU_NEUTRAL;
        imu_orientation_t orient = clock_imu_get_orientation();
        if (orient != last_orient)
        {
            last_orient = orient;
            clock_gui_set_orientation(orient);
            update_gui = true;
        }

        // Check the cursor for movement / and whether it's supposed to hide (after a few seconds)
        if (clock_cursor_update(tick_us))
            update_gui = true;

        // Update the seconds, minutes and hours internally (DCF77 is separate)
        if (clock_time_inc_second(tick_us))
            update_gui = true;

        if (update_gui)
            clock_gui_update(tick_us);

        uint32_t time_till_next = lv_timer_handler();
        vTaskDelay(xDelay);
    }
}

int main()
{
    int ret;
    static TaskHandle_t handle_control;

    if (0 != (ret = DEV_Module_Init()))
        ERROR(ret, "DEV_Module_Init");

    /* Initialize the IMU before the GUI/tasks use it (Req 1.5). */
    clock_imu_init();
    clock_time_init();
    
    clock_time_set_from_dcf77(13, 0, 20, 6, 2026);  // 12:34, 20.06.2026
    clock_time_set_weekday(6);
    
    clock_gui_init();
    clock_dcf77_init();
    clock_cursor_init();

    // Sample Task is the highest-priority Task and has configMAX_PRIORITIES-2 (one less than timer task)
    ret = xTaskCreate(control_fn, "control_fn", 5*configMINIMAL_STACK_SIZE, NULL, CLOCK_CONTROL_TASK_PRIORITY, &handle_control);
    if (ret != pdPASS)
        ERROR(ret, "xTaskCreate for control_fn returned != pdPASS");
    assert(handle_control != NULL);
    // vTaskCoreAffinitySet(handle_control, (0x1 << 1));

    vTaskStartScheduler();

    // This is not really necessary, will never reach, dead-code-elimination
    DEV_Module_Exit();
}
