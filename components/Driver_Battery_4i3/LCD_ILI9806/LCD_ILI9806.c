#include "LCD_ILI9806.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "driver/ledc.h"
#include "Key.h"
#include "TimerApp.h"

lv_indev_t * joy_indev;

#define TAG "LCD_ILI9806"
#if (LVGL_CONFIG_ENABLE_SPIRAM_LCD == 1 & CONFIG_SPIRAM)
#define MEMORY_LCD_LVGL LCD_V_RES
#else
#define MEMORY_LCD_LVGL 100
#endif

static SemaphoreHandle_t lvgl_mux = NULL;

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

static void increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

bool lvgl_lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

static void lvgl_port_task(void *arg)
{
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            lvgl_unlock();
        }
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

static void ILI9806G_LCD105FPC_Config(const esp_lcd_panel_io_handle_t io)
{
    esp_lcd_panel_io_tx_param(io, 0xff, (uint8_t[]) {
        0xff,
        0x98, 
        0x06,
    }, 3);

    esp_lcd_panel_io_tx_param(io, 0xbc, (uint8_t[]) {
        0x01,
        0x0F,
        0x61,
        0xff,
        0x01,
        0x01,
        0x0B,
        0x10,
        0x37,
        0x63,
        0xFF,
        0xFF,
        0x01,
        0x01,
        0x00,
        0x00,
        0xff,
        0x52,
        0x01,
        0x00,
        0x40,
    }, 21);
    // GIP 2
    esp_lcd_panel_io_tx_param(io, 0xbd, (uint8_t[]) {
        0x01,
        0x23,
        0x45,
        0x67,
        0x01,
        0x23,
        0x45,
        0x67,
    }, 8);
    // GIP 3
    esp_lcd_panel_io_tx_param(io, 0xbe, (uint8_t[]) {
        0x00,
        0x01,
        0xab,
        0x60,
        0x22,
        0x22,
        0x22,
        0x22,
        0x22,
    }, 9);
    // Vcom
    esp_lcd_panel_io_tx_param(io, 0xc7, (uint8_t[]) {
        0x36,
    }, 1);
    // EN_volt_reg
    esp_lcd_panel_io_tx_param(io, 0xed, (uint8_t[]) {
        0x7f,
        0x0f,
    }, 2);
    // Power Control 1
    esp_lcd_panel_io_tx_param(io, 0xc0, (uint8_t[]) {
        0x0f,
        0x0b,
        0x0a,
    }, 3);
    esp_lcd_panel_io_tx_param(io, 0xfc, (uint8_t[]) {
        0x08,
    }, 1);
    // Engineering Setting
    esp_lcd_panel_io_tx_param(io, 0xdf, (uint8_t[]) {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x20,
    }, 6);
    // DVDD Voltage Setting
    esp_lcd_panel_io_tx_param(io, 0xf3, (uint8_t[]) {
        0x74,
    }, 1);
    // Display Inversion Control
    esp_lcd_panel_io_tx_param(io, 0xb4, (uint8_t[]) {
        0x00,
        0x00,
        0x00,
    }, 3);
    // Panel Resolution Selection Set.
    switch (ILI9806_RESOLUTION_VER) {
    case 864:
        esp_lcd_panel_io_tx_param(io, 0xf7, (uint8_t[]) {
            0x80,
        }, 1); // set to 480x864
        break;
    case 854:
        esp_lcd_panel_io_tx_param(io, 0xf7, (uint8_t[]) {
            0x81,
        }, 1); // set to 480x854
        break;
    case 800:
        esp_lcd_panel_io_tx_param(io, 0xf7, (uint8_t[]) {
            0x82,
        }, 1); // set to 480x800
        break;
    case 640:
        esp_lcd_panel_io_tx_param(io, 0xf7, (uint8_t[]) {
            0x83,
        }, 1); // set to 480x640
        break;
    case 720:
        esp_lcd_panel_io_tx_param(io, 0xf7, (uint8_t[]) {
            0x84,
        }, 1); // set to 480x720
        break;
    default:
        ESP_LOGE(TAG, "Unsupported resolution, use default [480x865]");
        break;
    }
    // Frame Rate
    esp_lcd_panel_io_tx_param(io, 0xb1, (uint8_t[]) {
        0x00,
        0x13,
        0x13,
    }, 3);
    // Panel Timing Control 2
    esp_lcd_panel_io_tx_param(io, 0xf2, (uint8_t[]) {
        0x80,
        0x04,
        0x40,
        0x28,
    }, 4);
    // Power Control 2
    esp_lcd_panel_io_tx_param(io, 0xc1, (uint8_t[]) {
        0x17,
        0x88,
        0x88,
        0x20,
    }, 4);
    // Positive Gamma Control
    esp_lcd_panel_io_tx_param(io, 0xe0, (uint8_t[]) {
        0x00,
        0x0a,
        0x12,
        0x10,
        0x0e,
        0x20,
        0xcc,
        0x07,
        0x06,
        0x0b,
        0x0e,
        0x0f,
        0x0d,
        0x15,
        0x10,
        0x00,
    }, 16);
    // Negative Gamma Correction
    esp_lcd_panel_io_tx_param(io, 0xe1, (uint8_t[]) {
        0x00,
        0x0d,
        0x13,
        0x0d,
        0x0e,
        0x1b,
        0x71,
        0x06,
        0x06,
        0x0a,
        0x0f,
        0x0e,
        0x0f,
        0x15,
        0x0c,
        0x00,
    }, 16);
    
    esp_lcd_panel_io_tx_param(io, 0x2a, (uint8_t[]) {
        0x00,
        0x00,
        0x01,
        0xdf,
    }, 4);

    esp_lcd_panel_io_tx_param(io, 0x2b, (uint8_t[]) {
        0x00,
        0x00,
        0x03,
        0x1f,
    }, 4);

    esp_lcd_panel_io_tx_param(io, 0x36, (uint8_t[]) {
        0x00,
    }, 1);
    
    esp_lcd_panel_io_tx_param(io, 0x3a, (uint8_t[]) {
        0x55,
    }, 1);
    //Tearing Effect ON
    esp_lcd_panel_io_tx_param(io, 0x35, (uint8_t[]) {
        0x00,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void ILI9806G_LCD_115FPC_B_Config(const esp_lcd_panel_io_handle_t io)
{
    esp_lcd_panel_io_tx_param(io, 0xff, (uint8_t[]) {
        0xff,
        0x98, 
        0x06,
    }, 3);

    esp_lcd_panel_io_tx_param(io, 0xba, (uint8_t[]) {
        0x60,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0xbc, (uint8_t[]) {
        0x01,
        0x0e,
        0x61,
        0xfb,
        0x10,
        0x10,
        0x0B,
        0x0f,
        0x2e,
        0x63,
        0xFF,
        0xFF,
        0x0e,
        0x0e,
        0x00,
        0x03,
        0x66,
        0x63,
        0x01,
        0x00,
        0x00,
    }, 21);
    // GIP 2
    esp_lcd_panel_io_tx_param(io, 0xbd, (uint8_t[]) {
        0x01,
        0x23,
        0x45,
        0x67,
        0x01,
        0x23,
        0x45,
        0x67,
    }, 8);
    // GIP 3
    esp_lcd_panel_io_tx_param(io, 0xbe, (uint8_t[]) {
        0x00,
        0x21,
        0xab,
        0x60,
        0x22,
        0x22,
        0x22,
        0x22,
        0x22,
    }, 9);
    // Vcom
    esp_lcd_panel_io_tx_param(io, 0xc7, (uint8_t[]) {
        0x6f,
    }, 1);
    // EN_volt_reg
    esp_lcd_panel_io_tx_param(io, 0xed, (uint8_t[]) {
        0x7f,
        0x0f,
        0x00,
    }, 3);
    // Power Control 1
    esp_lcd_panel_io_tx_param(io, 0xc0, (uint8_t[]) {
        0x37,
        0x0b,
        0x0a,
    }, 3);
    esp_lcd_panel_io_tx_param(io, 0xfc, (uint8_t[]) {
        0x0a,
    }, 1);
    // Engineering Setting
    esp_lcd_panel_io_tx_param(io, 0xdf, (uint8_t[]) {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x20,
    }, 6);
    // DVDD Voltage Setting
    esp_lcd_panel_io_tx_param(io, 0xf3, (uint8_t[]) {
        0x74,
    }, 1);
    // Display Inversion Control
    esp_lcd_panel_io_tx_param(io, 0xb4, (uint8_t[]) {
        0x00,
        0x00,
        0x00,
    }, 3);
    // Panel Resolution Selection Set.
    switch (ILI9806_RESOLUTION_VER) {
    case 864:
        esp_lcd_panel_io_tx_param(io, 0xf7, (uint8_t[]) {
            0x80,
        }, 1); // set to 480x864
        break;
    case 854:
        esp_lcd_panel_io_tx_param(io, 0xf7, (uint8_t[]) {
            0x81,
        }, 1); // set to 480x854
        break;
    case 800:
        esp_lcd_panel_io_tx_param(io, 0xf7, (uint8_t[]) {
            0x82,
        }, 1); // set to 480x800
        break;
    case 640:
        esp_lcd_panel_io_tx_param(io, 0xf7, (uint8_t[]) {
            0x83,
        }, 1); // set to 480x640
        break;
    case 720:
        esp_lcd_panel_io_tx_param(io, 0xf7, (uint8_t[]) {
            0x84,
        }, 1); // set to 480x720
        break;
    default:
        ESP_LOGE(TAG, "Unsupported resolution, use default [480x865]");
        break;
    }

    // Frame Rate
    esp_lcd_panel_io_tx_param(io, 0xb1, (uint8_t[]) {
        0x00,
        0x12,
        0x13,
    }, 3);
    // Panel Timing Control 2
    esp_lcd_panel_io_tx_param(io, 0xf2, (uint8_t[]) {
        0x80,
        0x5b,
        0x40,
        0x28,
    }, 4);
    // Power Control 2
    esp_lcd_panel_io_tx_param(io, 0xc1, (uint8_t[]) {
        0x17,
        0x7d,
        0x7a,
        0x20,
    }, 4);
    // Positive Gamma Control
    esp_lcd_panel_io_tx_param(io, 0xe0, (uint8_t[]) {
        0x00,
        0x11,
        0x1c,
        0x0e,
        0x0f,
        0x0c,
        0xc7,
        0x06,
        0x06,
        0x0a,
        0x10,
        0x12,
        0x0a,
        0x10,
        0x02,
        0x00,
    }, 16);
    // Negative Gamma Correction
    esp_lcd_panel_io_tx_param(io, 0xe1, (uint8_t[]) {
        0x00,
        0x12,
        0x18,
        0x0c,
        0x0f,
        0x0a,
        0x77,
        0x06,
        0x07,
        0x0a,
        0x0e,
        0x0b,
        0x10,
        0x1d,
        0x17,
        0x00,
    }, 16);
    
    esp_lcd_panel_io_tx_param(io, 0x2a, (uint8_t[]) {
        0x00,
        0x00,
        0x01,
        0xdf,
    }, 4);

    esp_lcd_panel_io_tx_param(io, 0x2b, (uint8_t[]) {
        0x00,
        0x00,
        0x03,
        0x1f,
    }, 4);

    esp_lcd_panel_io_tx_param(io, 0x36, (uint8_t[]) {
        0x00,
    }, 1);
    
    esp_lcd_panel_io_tx_param(io, 0x3a, (uint8_t[]) {
        0x55,
    }, 1);
    //Tearing Effect ON
    esp_lcd_panel_io_tx_param(io, 0x35, (uint8_t[]) {
        0x00,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void ST7796S_193FPC_LCD_Config(const esp_lcd_panel_io_handle_t io)
{
    // Sleep Out
    esp_lcd_panel_io_tx_param(io, 0x11, (uint8_t[]) {
        0x00
    }, 0);
    vTaskDelay(pdMS_TO_TICKS(120));

    esp_lcd_panel_io_tx_param(io, 0xf0, (uint8_t[]) {
        0xc3,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0xf0, (uint8_t[]) {
        0x96,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0x36, (uint8_t[]) {
        0x48,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0x3A, (uint8_t[]) {
        0x55,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0xB4, (uint8_t[]) {
        0x01,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0xB6, (uint8_t[]) {
        0x20,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0xB7, (uint8_t[]) {
        0xC6,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0xe8, (uint8_t[]) {
        0x40,
        0x8a,
        0x00,
        0x00,
        0x29,
        0x19,
        0xa5,
        0x33,
    }, 8);

    esp_lcd_panel_io_tx_param(io, 0xc2, (uint8_t[]) {
        0xA7,
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0xc5, (uint8_t[]) {
        0x1c,
    }, 1);

    //Positive Voltage Gamma Control
    esp_lcd_panel_io_tx_param(io, 0xe0, (uint8_t[]) {
        0xf0,
        0x00,
        0x02,
        0x0a,
        0x0d,
        0x1d,
        0x35,
        0x55,
        0x45,
        0x3c,
        0x17,
        0x17,
        0x18,
        0x1b,
    }, 14);

    //Negative Voltage Gamma Control
    esp_lcd_panel_io_tx_param(io, 0xe1, (uint8_t[]) {
        0xf0,
        0x00,
        0x02,
        0x07,
        0x06,
        0x04,
        0x2e,
        0x44,
        0x45,
        0x0b,
        0x17,
        0x16,
        0x18,
        0x1b,
    }, 14);

    esp_lcd_panel_io_tx_param(io, 0xf0, (uint8_t[]) {
        0x3C,
    }, 1);

    /*Memory data access control*/
    esp_lcd_panel_io_tx_param(io, 0x36, (uint8_t[]) {
        0b10001000, // MY  MX = 1  MV  ML  RGB  MH  0  0 //
    }, 1);

    esp_lcd_panel_io_tx_param(io, 0x21, (uint8_t[]) {
        0x69,
    }, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void Init_LCDBackLight(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LCD_BACKLIGHT_MODE,
        .timer_num        = LCD_BACKLIGHT_TIMER,
        .duty_resolution  = LCD_BACKLIGHT_RES,
        .freq_hz          = LCD_BACKLIGHT_FREQUENCY,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LCD_BACKLIGHT_MODE,
        .channel        = LCD_BACKLIGHT_CHANNEL,
        .timer_sel      = LCD_BACKLIGHT_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_NUM_BK_LIGHT,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

static void joy_read_cb(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
	status_button_t key = KEY_NONE;
	key = Proc_GetStatusKey();
	if(key != KEY_NONE)
	{
		switch(key)
		{
		case KEY_ENTER:
			data->key = LV_KEY_ENTER;
			data->state = LV_INDEV_STATE_PRESSED;
            // ESP_LOGW(__func__, "Key: LV_KEY_ENTER");
			break;
		case KEY_LEFT:
			data->key = LV_KEY_LEFT;
			data->state = LV_INDEV_STATE_PRESSED;
            // ESP_LOGW(__func__, "Key: LV_KEY_LEFT");
			break;
		case KEY_RIGHT:
			data->key = LV_KEY_RIGHT;
			data->state = LV_INDEV_STATE_PRESSED;
            // ESP_LOGW(__func__, "Key: LV_KEY_RIGHT");
			break;
		case KEY_UP:
			data->key = LV_KEY_UP;
			data->state = LV_INDEV_STATE_PRESSED;
            // ESP_LOGW(__func__, "Key: LV_KEY_UP");
			break;
		case KEY_DN:
			data->key = LV_KEY_DOWN;
			data->state = LV_INDEV_STATE_PRESSED;
            // ESP_LOGW(__func__, "Key: LV_KEY_DOWN");
			break;
        case KEY_HOME:
			data->key = LV_KEY_HOME;
			data->state = LV_INDEV_STATE_PRESSED;
            // ESP_LOGW(__func__, "Key: LV_KEY_HOME");
			break;
        case KEY_BACK:
			data->key = LV_KEY_ESC;
			data->state = LV_INDEV_STATE_PRESSED;
            // ESP_LOGW(__func__, "Key: LV_KEY_ESC");
			break;
		default:
			break;
		}
	} else {
		data->state = LV_INDEV_STATE_RELEASED;
	}
}

void indev_init(void)
{
	static lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);
	indev_drv.read_cb = joy_read_cb;
	indev_drv.type = LV_INDEV_TYPE_KEYPAD;
	joy_indev = lv_indev_drv_register(&indev_drv);
}

void init_lcd(void)
{
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions
    int _timeout = 0;

    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT
    };
    Init_LCDBackLight();

    gpio_config_t rd_pin_config = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << PIN_RD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&rd_pin_config);

    ESP_LOGI(TAG, "Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = PIN_NUM_DC,
        .wr_gpio_num = PIN_NUM_PCLK,
        .data_gpio_nums = {
            PIN_NUM_DATA0,
            PIN_NUM_DATA1,
            PIN_NUM_DATA2,
            PIN_NUM_DATA3,
            PIN_NUM_DATA4,
            PIN_NUM_DATA5,
            PIN_NUM_DATA6,
            PIN_NUM_DATA7,
        },
        .bus_width = 8,
        .max_transfer_bytes = LCD_H_RES * MEMORY_LCD_LVGL * sizeof(uint16_t),
        .psram_trans_align = PSRAM_DATA_ALIGNMENT,
        .sram_trans_align = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .flags = {
            .swap_color_bytes = !LV_COLOR_16_SWAP, // Swap can be done in LvGL (default) or DMA
        },
        .on_color_trans_done = notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_LOGI(TAG, "Install LCD driver of ili9806");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        // .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    // ILI9806G_LCD105FPC_Config(io_handle);
    // ILI9806G_LCD_115FPC_B_Config(io_handle);
    ST7796S_193FPC_LCD_Config(io_handle);

    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = NULL;
    lv_color_t *buf2 = NULL;
    
    #if (LVGL_CONFIG_ENABLE_SPIRAM_LCD == 1 & CONFIG_SPIRAM)
    ESP_LOGE(TAG, "Heap size: %" PRIu32 " KB\n", esp_get_free_heap_size()/1024);
    buf1 = heap_caps_aligned_alloc(PSRAM_DATA_ALIGNMENT, LCD_H_RES * MEMORY_LCD_LVGL * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    // buf1 = heap_caps_malloc(LCD_H_RES * MEMORY_LCD_LVGL * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf1);
    ESP_LOGE(TAG, "Heap size: %" PRIu32 " KB\n", esp_get_free_heap_size()/1024);
    buf2 = heap_caps_aligned_alloc(PSRAM_DATA_ALIGNMENT, LCD_H_RES * MEMORY_LCD_LVGL * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf2);
    ESP_LOGE(TAG, "Heap size: %" PRIu32 " KB\n", esp_get_free_heap_size()/1024);
    ESP_LOGW(TAG, "size SPIRAM buf = %d", (LCD_H_RES * MEMORY_LCD_LVGL * sizeof(lv_color_t)));
    ESP_LOGW(TAG, "size buf1 = %d", (LCD_H_RES * MEMORY_LCD_LVGL));
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LCD_H_RES * MEMORY_LCD_LVGL);
    #else
    ESP_LOGE(TAG, "Heap size: %d KB\n", (int)(heap_caps_get_free_size(MALLOC_CAP_DMA)/1024));
    buf1 = heap_caps_malloc(LCD_H_RES * MEMORY_LCD_LVGL * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(buf1);
    // buf2 = heap_caps_malloc(LCD_H_RES * MEMORY_LCD_LVGL * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    // assert(buf2);
    ESP_LOGI(TAG, "size DMA buf = %d", (LCD_H_RES * MEMORY_LCD_LVGL * sizeof(lv_color_t)));
    // initialize LVGL draw buffers
    ESP_LOGE(TAG, "Heap size: %d KB\n", (int)(heap_caps_get_free_size(MALLOC_CAP_DMA)/1024));
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LCD_H_RES * MEMORY_LCD_LVGL);
    #endif

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    disp_drv.rotated = LV_DISP_ROT_90;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    
    indev_init();

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);
    xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);    
}

void LCD_SetBrightness(uint8_t bStep)
{
    uint32_t backlight_duty = LCD_BACKLIGHT_DUTY_4;
    if (bStep == 0) backlight_duty = LCD_BACKLIGHT_DUTY_0;
    if (bStep == 1) backlight_duty = LCD_BACKLIGHT_DUTY_1;
    if (bStep == 2) backlight_duty = LCD_BACKLIGHT_DUTY_2;
    if (bStep == 3) backlight_duty = LCD_BACKLIGHT_DUTY_3;
    if (bStep >= 4) backlight_duty = LCD_BACKLIGHT_DUTY_4;
    ESP_ERROR_CHECK(ledc_set_duty(LCD_BACKLIGHT_MODE, LCD_BACKLIGHT_CHANNEL, backlight_duty));
    ESP_ERROR_CHECK(ledc_update_duty(LCD_BACKLIGHT_MODE, LCD_BACKLIGHT_CHANNEL));
}