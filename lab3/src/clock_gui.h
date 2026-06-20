#ifndef __CLOCK_GUI_H__
#define __CLOCK_GUI_H__

#include "lvgl/lvgl.h"
#include "clock_imu.h"

LV_IMAGE_DECLARE(clock_image);

void clock_gui_init(void);
void clock_gui_update(uint64_t tick_us);

/* Forward the current IMU tilt orientation to the GUI (Req 1.5).
 * The screen switch itself is applied inside clock_gui_update(), which runs
 * in the control task -- the only task allowed to call into LVGL. */
void clock_gui_set_orientation(imu_orientation_t orientation);

#endif /* __CLOCK_GUI_H__ */