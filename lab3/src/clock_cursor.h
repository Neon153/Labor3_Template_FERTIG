/**
 * @file The cursor is an input with the Joystick, allowing to set the time:
 *  - when moving the Joystick, a cursor (two lines) should appear
 *    (see clock_gui.c)
 *  Using the joystick, one shall edit (change) the hour and minute:
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
#ifndef __CLOCK_CURSOR_H__
#define __CLOCK_CURSOR_H__

#include <pico/types.h>
#include "clock.h"

#define JOYSTICK_GPIO_X       26
#define JOYSTICK_GPIO_Y       27
#define JOYSTICK_GPIO_BUTTON  20


/**
 * Initialize the cursor component:
 *  - Set up ADC
 *  - Set up first pointer position
 *  - Set up the fade-out time
 *  - Store the areas where hours and minutes are selected to be edited
 * 
 * @return true   In case initialization was successful.
 */
bool clock_cursor_init(void);

/**
 * Check whether the cursor has moved (and is visible) or will fade,
 * aka after some time was visible but now isn't anymore.
 * 
 * @param tick_us The current clock tick in microsecons after boot.
 * 
 * @return Whether there is change and requires GUI update.
 */
bool clock_cursor_update(uint64_t tick_us);

/**
 * Especially, when clock_cursor_update() returns true, this function provides
 * the cursor's current cursor state:  whether it's editing,
 * whether it's visible (or faded) and the current position.
 * 
 * @param pos[out]     The current position of the cursor
 * @param visible[out] Whether it's visible or faded
 */
void clock_cursor_get_state(pos_t * pos, bool * visible);

#endif /* __CLOCK_CURSOR_H__ */