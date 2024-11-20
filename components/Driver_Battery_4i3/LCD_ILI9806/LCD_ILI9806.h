#ifndef _LCD_ILI9806_H_
#define _LCD_ILI9806_H_

#include <stdint.h>
#include <stdbool.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define LVGL_CONFIG_ENABLE_SPIRAM_LCD 1
#if (LVGL_CONFIG_ENABLE_SPIRAM_LCD == 1 & CONFIG_SPIRAM)
#define LCD_PIXEL_CLOCK_HZ     (20* 1000 * 1000)
#else
#define LCD_PIXEL_CLOCK_HZ     (20* 1000 * 1000)
#endif

#define LCD_BK_LIGHT_ON_LEVEL  1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
#define PIN_NUM_DATA0          35
#define PIN_NUM_DATA1          36
#define PIN_NUM_DATA2          37
#define PIN_NUM_DATA3          38
#define PIN_NUM_DATA4          39
#define PIN_NUM_DATA5          40
#define PIN_NUM_DATA6          41
#define PIN_NUM_DATA7          42
#define PIN_NUM_PCLK           1    // WR // NWE
#define PIN_NUM_CS             0    // LCD_CS
#define PIN_NUM_DC             48   // LCD_DCX
#define PIN_NUM_RST            -1
#define PIN_NUM_BK_LIGHT       45
#define PIN_RD                  2

/** ILI9806 can select different resolution */
#define ILI9806_RESOLUTION_VER 800  
// The pixel number in horizontal and vertical
#define LCD_H_RES              480
#define LCD_V_RES              320
// Bit number used to represent command and parameter
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

#define LVGL_TICK_PERIOD_MS    1

// Supported alignment: 16, 32, 64. A higher alignment can enables higher burst transfer size, thus a higher i80 bus throughput.
#define PSRAM_DATA_ALIGNMENT   64

#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE   (32 * 1024)
#define LVGL_TASK_PRIORITY     2

#define LCD_BACKLIGHT_TIMER         LEDC_TIMER_1
#define LCD_BACKLIGHT_MODE          LEDC_LOW_SPEED_MODE
#define LCD_BACKLIGHT_CHANNEL       LEDC_CHANNEL_1
#define LCD_BACKLIGHT_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LCD_BACKLIGHT_DUTY_0        (0) // Set duty to 50%. ((2 ** 13) - 1) * 25% = 2048
#define LCD_BACKLIGHT_DUTY_1        (2048) // Set duty to 50%. ((2 ** 13) - 1) * 25% = 2048
#define LCD_BACKLIGHT_DUTY_2        (4095) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define LCD_BACKLIGHT_DUTY_3        (6143) // Set duty to 50%. ((2 ** 13) - 1) * 75% = 6143
#define LCD_BACKLIGHT_DUTY_4        (8191) // Set duty to 100%. ((2 ** 13) - 1) * 100% = 8191
#define LCD_BACKLIGHT_FREQUENCY     (5000) // Frequency in Hertz. Set frequency at 5 kHz

void init_lcd(void);
bool lvgl_lock(int timeout_ms);
void lvgl_unlock(void);
void LCD_SetBrightness(uint8_t bStep);

#endif //_DRIVER_ILI9806