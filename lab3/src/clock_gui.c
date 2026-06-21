/**
 * @file Implements the main GUI of this clock.
 *
 * Based on simple lib/gui, which is based on lib/lcd
 * Major performance uptake is the introduction of a
 * backup image to restore, aka draw once, restore and
 * draw on-top.
 */
#include "lcd/LCD_1in28.h"
#include "hardware/dma.h"
#include "lvgl/lvgl.h"
#include "string.h"
#include "math.h"

#include "clock.h"
#include "clock_cursor.h"
#include "clock_gui.h"
#include "clock_time.h"

#define CURSOR_LEN 6

static lv_display_t * display;

/* Current tilt orientation requested by the IMU (Req 1.5). Written from the
 * control task via clock_gui_set_orientation(), read in clock_gui_update().
 * Both run in the same (control) task, so no extra mutex is required. */
static imu_orientation_t gui_orientation = IMU_NEUTRAL;

void clock_gui_set_orientation(imu_orientation_t orientation)
{
    gui_orientation = orientation;
}


static lv_obj_t *cursor_h_Screen1 = NULL;
static lv_obj_t *cursor_h_Screen2 = NULL;
static lv_obj_t *cursor_v_Screen1 = NULL;
static lv_obj_t *cursor_v_Screen2 = NULL;

static lv_point_precise_t cursor_points_h[2] = {{120 - CURSOR_LEN, 120}, {120 + CURSOR_LEN, 120}};
static lv_point_precise_t cursor_points_v[2] = {{120, 120 - CURSOR_LEN}, {120, 120 + CURSOR_LEN}};

static lv_obj_t *line_hour = NULL;
static lv_obj_t *line_min = NULL;
static lv_obj_t *line_sec = NULL;

static lv_point_precise_t line_hour_pos[2] = {{120, 120}, {120, 0}};
static lv_point_precise_t line_min_pos[2] =  {{120, 120}, {240, 120}};
static lv_point_precise_t line_sec_pos[2] =  {{120, 120}, {0, 120}};

static lv_obj_t *ui_Screen1 = NULL;
static lv_obj_t *ui_Screen1Day = NULL;
static lv_obj_t *ui_Screen1Month = NULL;
static lv_obj_t *ui_Screen1Year = NULL;
static lv_obj_t *ui_Screen1Weekday = NULL;   // weekday text (Req 2.1)

static lv_obj_t *ui_Screen2 = NULL;
static lv_obj_t *ui_Screen2Day = NULL;
static lv_obj_t *ui_Screen2Month = NULL;
static lv_obj_t *ui_Screen2Year = NULL;
static lv_obj_t *ui_Screen2Weekday = NULL;   // weekday text (Req 2.1)
static lv_obj_t *ui_Screen2_Hour = NULL;
static lv_obj_t *ui_Screen2_Minute = NULL;
static lv_obj_t *ui_Screen2_Seconds = NULL;
static lv_obj_t *ui_Screen2_Label1 = NULL;
static lv_obj_t *ui_Screen2_Label2 = NULL;

#ifdef WANT_COMPUTED_POSITIONS
// 60 end positions on the outskirts of the display
static pos_t clock_pos_sec[60];
static pos_t clock_pos_min[60];
static pos_t clock_pos_hrs[96];

static void clock_gui_calc_pos(void)
{
    for (int r = 0; r < 60; r++)
    {
        int x = 120 + round(110.0 * cos((360.0 / 60.0 * r) * M_PI / 180.0));
        int y = 120 + round(110.0 * sin((360.0 / 60.0 * r) * M_PI / 180.0));
        clock_pos_sec[r].x = x;
        clock_pos_sec[r].y = y;
    }

    for (int r = 0; r < 60; r++)
    {
        int x = 120 + round(90.0 * cos((360.0 / 60.0 * r) * M_PI / 180.0));
        int y = 120 + round(90.0 * sin((360.0 / 60.0 * r) * M_PI / 180.0));
        clock_pos_min[r].x = x;
        clock_pos_min[r].y = y;
    }

    // Even for hours, we split into 96 separate positions
    for (int r = 0; r < 96; r++)
    {
        int x = 120 + round(70.0 * cos((360.0 / 96.0 * r) * M_PI / 180.0));
        int y = 120 + round(70.0 * sin((360.0 / 96.0 * r) * M_PI / 180.0));
        clock_pos_hrs[r].x = x;
        clock_pos_hrs[r].y = y;
    }
}
#else
static const pos_t clock_pos_sec[] = 
  { {230, 120}, {229, 131}, {228, 143}, {225, 154}, {220, 165}, {215, 175}, {209, 185}, {202, 194}, {194, 202}, {185, 209}, 
    {175, 215}, {165, 220}, {154, 225}, {143, 228}, {131, 229}, {120, 230}, {109, 229}, { 97, 228}, { 86, 225}, { 75, 220}, 
    { 65, 215}, { 55, 209}, { 46, 202}, { 38, 194}, { 31, 185}, { 25, 175}, { 20, 165}, { 15, 154}, { 12, 143}, { 11, 131}, 
    { 10, 120}, { 11, 109}, { 12,  97}, { 15,  86}, { 20,  75}, { 25,  65}, { 31,  55}, { 38,  46}, { 46,  38}, { 55,  31}, 
    { 65,  25}, { 75,  20}, { 86,  15}, { 97,  12}, {109,  11}, {120,  10}, {131,  11}, {143,  12}, {154,  15}, {165,  20}, 
    {175,  25}, {185,  31}, {194,  38}, {202,  46}, {209,  55}, {215,  65}, {220,  75}, {225,  86}, {228,  97}, {229, 109}};
static const pos_t clock_pos_min[] = 
  { {210, 120}, {210, 129}, {208, 139}, {206, 148}, {202, 157}, {198, 165}, {193, 173}, {187, 180}, {180, 187}, {173, 193}, 
    {165, 198}, {157, 202}, {148, 206}, {139, 208}, {129, 210}, {120, 210}, {111, 210}, {101, 208}, { 92, 206}, { 83, 202}, 
    { 75, 198}, { 67, 193}, { 60, 187}, { 53, 180}, { 47, 173}, { 42, 165}, { 38, 157}, { 34, 148}, { 32, 139}, { 30, 129}, 
    { 30, 120}, { 30, 111}, { 32, 101}, { 34,  92}, { 38,  83}, { 42,  75}, { 47,  67}, { 53,  60}, { 60,  53}, { 67,  47}, 
    { 75,  42}, { 83,  38}, { 92,  34}, {101,  32}, {111,  30}, {120,  30}, {129,  30}, {139,  32}, {148,  34}, {157,  38}, 
    {165,  42}, {173,  47}, {180,  53}, {187,  60}, {193,  67}, {198,  75}, {202,  83}, {206,  92}, {208, 101}, {210, 111}};
static const pos_t clock_pos_hrs[] = 
  { {190, 120}, {190, 125}, {189, 129}, {189, 134}, {188, 138}, {186, 143}, {185, 147}, {183, 151}, {181, 155}, {178, 159}, 
    {176, 163}, {173, 166}, {169, 169}, {166, 173}, {163, 176}, {159, 178}, {155, 181}, {151, 183}, {147, 185}, {143, 186}, 
    {138, 188}, {134, 189}, {129, 189}, {125, 190}, {120, 190}, {115, 190}, {111, 189}, {106, 189}, {102, 188}, { 97, 186}, 
    { 93, 185}, { 89, 183}, { 85, 181}, { 81, 178}, { 77, 176}, { 74, 173}, { 71, 169}, { 67, 166}, { 64, 163}, { 62, 159}, 
    { 59, 155}, { 57, 151}, { 55, 147}, { 54, 143}, { 52, 138}, { 51, 134}, { 51, 129}, { 50, 125}, { 50, 120}, { 50, 115}, 
    { 51, 111}, { 51, 106}, { 52, 102}, { 54,  97}, { 55,  93}, { 57,  89}, { 59,  85}, { 62,  81}, { 64,  77}, { 67,  74}, 
    { 71,  71}, { 74,  67}, { 77,  64}, { 81,  62}, { 85,  59}, { 89,  57}, { 93,  55}, { 97,  54}, {102,  52}, {106,  51}, 
    {111,  51}, {115,  50}, {120,  50}, {125,  50}, {129,  51}, {134,  51}, {138,  52}, {143,  54}, {147,  55}, {151,  57}, 
    {155,  59}, {159,  62}, {163,  64}, {166,  67}, {169,  71}, {173,  74}, {176,  77}, {178,  81}, {181,  85}, {183,  89}, 
    {185,  93}, {186,  97}, {188, 102}, {189, 106}, {189, 111}, {190, 115}};
static void clock_gui_calc_pos(void) {}
#endif

static void clock_gui_text_init(lv_obj_t * obj, int x, int width, int max_length){
    lv_obj_set_x(obj, x);
    lv_obj_set_y(obj, 140);
    lv_obj_set_width(obj, width);
    lv_obj_set_height(obj, 30);
    lv_obj_set_style_pad_left(obj, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(obj, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(obj, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(obj, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_textarea_set_max_length(obj, max_length);
    lv_textarea_set_placeholder_text(obj, "...");
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_decor(obj, LV_TEXT_DECOR_NONE, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_18, LV_PART_MAIN| LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
    lv_obj_set_style_bg_opa(obj, 192, LV_PART_MAIN| LV_STATE_DEFAULT);
}

/* Create a centered weekday label on the given screen (Req 2.1). The y offset
 * is relative to the top of the screen. A semi-transparent white box keeps it
 * readable over both the analog clock face and the digital gradient. */
static lv_obj_t * clock_gui_weekday_init(lv_obj_t * parent, int y)
{
    lv_obj_t * lbl = lv_label_create(parent);
    lv_obj_set_width(lbl, LV_SIZE_CONTENT);
    lv_obj_set_height(lbl, LV_SIZE_CONTENT);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_MID);
    lv_obj_set_y(lbl, y);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(lbl, "---");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lbl, 192, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(lbl, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(lbl, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    return lbl;
}

static void clock_gui_Screen1_init(void)
{
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_image_src(ui_Screen1, &clock_image, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Screen1Day = lv_textarea_create(ui_Screen1);
    clock_gui_text_init(ui_Screen1Day, 45, 30, 6);

    ui_Screen1Month = lv_textarea_create(ui_Screen1);
    clock_gui_text_init(ui_Screen1Month, 75, 60, 4);
    
    ui_Screen1Year = lv_textarea_create(ui_Screen1);
    clock_gui_text_init(ui_Screen1Year, 135, 60, 4);

    line_hour = lv_line_create(ui_Screen1);
    static lv_style_t line_h_style;
    lv_style_init(&line_h_style);
    lv_style_set_line_color(&line_h_style, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_line_width(&line_h_style, 4);
    lv_obj_add_style(line_hour, &line_h_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    line_min = lv_line_create(ui_Screen1);
    static lv_style_t line_m_style;
    lv_style_init(&line_m_style);
    lv_style_set_line_color(&line_m_style, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_line_width(&line_m_style, 4);
    lv_obj_add_style(line_min, &line_m_style, LV_PART_MAIN | LV_STATE_DEFAULT);

    line_sec = lv_line_create(ui_Screen1);
    static lv_style_t line_s_style;
    lv_style_init(&line_s_style);
    lv_style_set_line_color(&line_s_style, lv_palette_main(LV_PALETTE_BLUE_GREY));
    lv_style_set_line_width(&line_s_style, 4);
    lv_obj_add_style(line_sec, &line_s_style, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Weekday in the upper half of the analog display (Req 2.1). Created last
     * so it stays on top of the clock hands. */
    ui_Screen1Weekday = clock_gui_weekday_init(ui_Screen1, 48);
}

static void clock_gui_Screen2_init(void)
{
    ui_Screen2 = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Screen2, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(ui_Screen2, lv_color_hex(0x262626), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Screen2, lv_color_hex(0x7EBBFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_Screen2, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Just the same as with gui_Screen1_screen_init(),
    // but with a different background, and the addition of the Hour and Minute textareas
    ui_Screen2Day = lv_textarea_create(ui_Screen2);
    clock_gui_text_init(ui_Screen2Day, 45, 30, 6);

    ui_Screen2Month = lv_textarea_create(ui_Screen2);
    clock_gui_text_init(ui_Screen2Month, 75, 60, 4);

    ui_Screen2Year = lv_textarea_create(ui_Screen2);
    clock_gui_text_init(ui_Screen2Year, 135, 60, 4);

    ui_Screen2_Hour = lv_textarea_create(ui_Screen2);
    lv_obj_set_width(ui_Screen2_Hour, 60);
    lv_obj_set_height(ui_Screen2_Hour, 50);
    lv_obj_set_x(ui_Screen2_Hour, 15);
    lv_obj_set_y(ui_Screen2_Hour, 85);
    lv_textarea_set_max_length(ui_Screen2_Hour, 3);
    lv_textarea_set_placeholder_text(ui_Screen2_Hour, ".H.");
    lv_obj_set_scrollbar_mode(ui_Screen2_Hour, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_text_color(ui_Screen2_Hour, lv_color_hex(0xAFAFAF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Screen2_Hour, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_Screen2_Hour, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Screen2_Hour, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Screen2_Hour, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen2_Hour, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Screen2_Label1 = lv_label_create(ui_Screen2);
    lv_obj_set_width(ui_Screen2_Label1, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Screen2_Label1, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_Screen2_Label1, -40);
    lv_obj_set_y(ui_Screen2_Label1, -10);
    lv_obj_set_align(ui_Screen2_Label1, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_Screen2_Label1, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(ui_Screen2_Label1, ":");
    lv_obj_set_style_text_font(ui_Screen2_Label1, &lv_font_montserrat_38, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Screen2_Minute = lv_textarea_create(ui_Screen2);
    lv_obj_set_width(ui_Screen2_Minute, 60);
    lv_obj_set_height(ui_Screen2_Minute, 50);
    lv_obj_set_x(ui_Screen2_Minute, 88);
    lv_obj_set_y(ui_Screen2_Minute, 85);
    lv_textarea_set_max_length(ui_Screen2_Minute, 3);
    lv_textarea_set_placeholder_text(ui_Screen2_Minute, ".M.");
    lv_obj_set_scrollbar_mode(ui_Screen2_Minute, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_text_color(ui_Screen2_Minute, lv_color_hex(0xAFAFAF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Screen2_Minute, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_Screen2_Minute, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Screen2_Minute, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Screen2_Minute, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen2_Minute, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Screen2_Label2 = lv_label_create(ui_Screen2);
    lv_obj_set_width(ui_Screen2_Label2, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Screen2_Label2, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_Screen2_Label2, 37);
    lv_obj_set_y(ui_Screen2_Label2, -10);
    lv_obj_set_align(ui_Screen2_Label2, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_Screen2_Label2, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(ui_Screen2_Label2, ":");
    lv_obj_set_style_text_font(ui_Screen2_Label2, &lv_font_montserrat_38, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Screen2_Seconds = lv_textarea_create(ui_Screen2);
    lv_obj_set_width(ui_Screen2_Seconds, 60);
    lv_obj_set_height(ui_Screen2_Seconds, 50);
    lv_obj_set_x(ui_Screen2_Seconds, 166);
    lv_obj_set_y(ui_Screen2_Seconds, 85);
    lv_textarea_set_max_length(ui_Screen2_Seconds, 3);
    lv_textarea_set_placeholder_text(ui_Screen2_Seconds, ".S.");
    lv_obj_set_scrollbar_mode(ui_Screen2_Seconds, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_text_color(ui_Screen2_Seconds, lv_color_hex(0xAFAFAF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Screen2_Seconds, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_Screen2_Seconds, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Screen2_Seconds, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Screen2_Seconds, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen2_Seconds, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Weekday below the date on the digital display (Req 2.1). */
    ui_Screen2Weekday = clock_gui_weekday_init(ui_Screen2, 180);
}


/*
 * For LVGL this callback provides a sense of current time, required to
 * support in-time redraw of animations (every 33ms).
 */
static uint32_t my_tick_cb(void) {
    return to_ms_since_boot(get_absolute_time());
}

static void my_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    LCD_1IN28_DisplayWindows(area->x1, area->y1, area->x2+1, area->y2+1, (uint16_t*)px_map);
    lv_display_flush_ready(display);
}


static void clock_gui_lvgl_init(void)
{
    /* Initialize LVGL, this has to be the first function call prior to any other LVGL function */
    lv_init();
    /* The proper way to allow LVGL's animations and screen-update information is to provide
     * a call-back function (cb) which returns time in milliseconds */
    lv_tick_set_cb(my_tick_cb);

    /* Initialize the Display with the correct resolution information in Pixels */
    display = lv_display_create(LCD_1IN28_WIDTH, LCD_1IN28_HEIGHT);
    /* Set the Dots-per-Inch (DPI), sqrt(240^2 + 240^2)/1.28 inch; not really necessary */
    lv_display_set_dpi(display, 265);

    /* Setting display's color format to LV_COLOR_FORMAT_RGB565_SWAPPED, and UNCOMMENTING in lv_conf.h LV_COLOR_16_SWAP */
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565_SWAPPED);
    
    /* Set a call-back to flush the render buffer to the display, this is called every 33ms */
    lv_display_set_flush_cb(display, my_flush_cb);

    static uint16_t buf1[LCD_1IN28_WIDTH * LCD_1IN28_HEIGHT];

    /* Attach one or two buffers (each with either the full, halve or smaller screen size)
     * for full (divisor==1) or partial rendering (all other divisors) of the display in parallel */
    lv_display_set_buffers(display, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_FULL);
}

static void clock_gui_cursor_init(void) {
    cursor_h_Screen1 = lv_line_create(ui_Screen1);
    cursor_h_Screen2 = lv_line_create(ui_Screen2);
    cursor_v_Screen1 = lv_line_create(ui_Screen1);
    cursor_v_Screen2 = lv_line_create(ui_Screen2);
    static lv_style_t line_style;
    lv_style_init(&line_style);
    lv_style_set_line_color(&line_style, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_line_width(&line_style, 2);
    lv_obj_add_style(cursor_h_Screen1, &line_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(cursor_h_Screen2, &line_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(cursor_v_Screen1, &line_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(cursor_v_Screen2, &line_style, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_line_set_points(cursor_h_Screen1, cursor_points_h, 2);
    lv_line_set_points(cursor_h_Screen2, cursor_points_h, 2);
    lv_line_set_points(cursor_v_Screen1, cursor_points_v, 2);
    lv_line_set_points(cursor_v_Screen2, cursor_points_v, 2);

}

void clock_gui_init(void)
{
    LCD_1IN28_Init(HORIZONTAL);
    clock_gui_calc_pos();
    clock_gui_lvgl_init();
    clock_gui_Screen1_init();
    clock_gui_Screen2_init();
    clock_gui_cursor_init();

    lv_screen_load(ui_Screen1);
}

void clock_gui_update(uint64_t tick_us)
{
    int hours_arr;
    char string[16];
    int string_len;

    const lv_obj_t * screen_active = lv_screen_active();
    int hours, minutes, seconds;
    int day, month, year;
    clock_time_get_local_date(&hours, &minutes, &seconds, &day, &month, &year);

    int weekday;
    clock_time_get_weekday(&weekday);

    // First update the cursor
    pos_t pos;
    bool visible;
    clock_cursor_get_state(&pos, &visible);

    lv_obj_t * cursor_h = (screen_active == ui_Screen1 ? cursor_h_Screen1 : cursor_h_Screen2);
    lv_obj_t * cursor_v = (screen_active == ui_Screen1 ? cursor_v_Screen1 : cursor_v_Screen2);
    if (visible)
    {
        lv_point_precise_t tmp_h[2] = {{MAX(0, pos.x - CURSOR_LEN), pos.y},
                                       {MIN(LCD_1IN28_WIDTH, pos.x + CURSOR_LEN), pos.y}};
        memcpy(cursor_points_h, tmp_h, sizeof(cursor_points_h));
        lv_line_set_points(cursor_h, cursor_points_h, 2);

        lv_point_precise_t tmp_v[2] = {{pos.x, MAX(0, pos.y - CURSOR_LEN)},
                                       {pos.x, MIN(LCD_1IN28_HEIGHT, pos.y + CURSOR_LEN)}};
        memcpy(cursor_points_v, tmp_v, sizeof(cursor_points_v));
        lv_line_set_points(cursor_v, cursor_points_v, 2);
    } else {
        static const lv_point_precise_t const_tmp_h[] = {{0, 3}, {6, 3}};
        lv_line_set_points(cursor_h, const_tmp_h, 2);

        static const lv_point_precise_t const_tmp_v[] = {{3, 0}, {3, 6}};
        lv_line_set_points(cursor_v, const_tmp_v, 2);
    }
    
    string_len = snprintf(string, sizeof(string), "%02d", day);
    lv_textarea_set_text(ui_Screen1Day, string);
    lv_textarea_set_text(ui_Screen2Day, string);

    const char * month_strs[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    string_len = snprintf(string, sizeof(string), "%3s.", month_strs[month - 1]);
    lv_textarea_set_text(ui_Screen1Month, string);
    lv_textarea_set_text(ui_Screen2Month, string);

    string_len = snprintf(string, sizeof(string), "%4d", year);
    lv_textarea_set_text(ui_Screen1Year, string);
    lv_textarea_set_text(ui_Screen2Year, string);

    /* Weekday on both screens (Req 2.1). DCF77: 1 = Monday ... 7 = Sunday. */
    static const char * weekday_strs[] = {
        "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
    };
    const char * wd = (weekday >= 1 && weekday <= 7) ? weekday_strs[weekday - 1] : "---";
    lv_label_set_text(ui_Screen1Weekday, wd);
    lv_label_set_text(ui_Screen2Weekday, wd);

    // Now draw the hands of the clock -- but only on Screen1
    if (screen_active == ui_Screen1)
    {
        hours_arr = hours * 8 + (minutes * 16) / 120;
        lv_point_precise_t tmp_1 = {clock_pos_hrs[(hours_arr + 96 - 24) % 96].x, clock_pos_hrs[(hours_arr + 96 - 24) % 96].y};
        line_hour_pos[1] = tmp_1;
        lv_line_set_points(line_hour, line_hour_pos, 2);

        lv_point_precise_t tmp_2 = {clock_pos_min[(minutes + 60 - 15) % 60].x, clock_pos_min[(minutes + 60 - 15) % 60].y};
        line_min_pos[1] = tmp_2;
        lv_line_set_points(line_min, line_min_pos, 2);

        lv_point_precise_t tmp_3 = {clock_pos_sec[(seconds + 60 - 15) % 60].x, clock_pos_sec[(seconds + 60 - 15) % 60].y};
        line_sec_pos[1] = tmp_3;
        lv_line_set_points(line_sec, line_sec_pos, 2);
    }

    // Change the Time -- but only in ui_Screen2!
    if (screen_active == ui_Screen2)
    {
        string_len = snprintf(string, sizeof(string), "%02d", hours);
        lv_textarea_set_text(ui_Screen2_Hour, string);

        string_len = snprintf(string, sizeof(string), "%02d", minutes);
        lv_textarea_set_text(ui_Screen2_Minute, string);

        string_len = snprintf(string, sizeof(string), "%02d", seconds);
        lv_textarea_set_text(ui_Screen2_Seconds, string);
    }

    /* Switch the active screen according to the IMU tilt (Req 1.5):
     *   tilt left  -> Analog  display (Screen 1)
     *   tilt right -> Digital display (Screen 2)
     *   neutral    -> keep the currently selected screen
     * This replaces the periodic auto-toggle of the template. */
    if (gui_orientation == IMU_TILT_LEFT && screen_active != ui_Screen1)
        lv_screen_load(ui_Screen1);
    else if (gui_orientation == IMU_TILT_RIGHT && screen_active != ui_Screen2)
        lv_screen_load(ui_Screen2);

}