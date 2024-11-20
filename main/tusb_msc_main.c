/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* DESCRIPTION:
 * This example contains code to make ESP32-S3 based device recognizable by USB-hosts as a USB Mass Storage Device.
 * It either allows the embedded application i.e. example to access the partition or Host PC accesses the partition over USB MSC.
 * They can't be allowed to access the partition at the same time.
 * For different scenarios and behaviour, Refer to README of this example.
 */

#include <errno.h>
#include <dirent.h>
#include "esp_console.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include "Driver_Battery_4i3.h"
#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "TimerApp.h"
#include "test_benchmark_fatfs.h"

#include "mbedtls/aes.h"
#include "mbedtls/dhm.h"
#include "mbedtls/md5.h"
#include "mbedtls/cmac.h"
#include "mbedtls/base64.h"

#include "esp_crc.h"
#include "stdint.h"

static const char *TAG = "example_main";

#define IMG_WIDTH 153
#define IMG_HEIGHT 138
#define IMG_1_NO_SELECT "//data/OBD2 diagnostics.bin"
#define IMG_1_SELECT "//data/OBD2 diagnostics_Line.bin"
#define IMG_2_NO_SELECT "//data/Previous vehicle.bin"
#define IMG_2_SELECT "//data/Previous vehicle_Line.bin"
#define IMG_3_NO_SELECT "//data/Settings.bin"
#define IMG_3_SELECT "//data/Settings_Line.bin"
#define IMG_4_NO_SELECT "//data/Tool Library.bin"
#define IMG_4_SELECT "//data/Tool Library_Line.bin"
#define IMG_5_NO_SELECT "//data/TPMS Diagnostics.bin"
#define IMG_5_SELECT "//data/TPMS Diagnostics_Line.bin"
#define IMG_6_NO_SELECT "//data/TPMS service.bin"
#define IMG_6_SELECT "//data/TPMS service_Line.bin"

#define MENU_LIST 20
#define SCROLL_STEP 100
#define SCROLL_Y_MIN 0

lv_obj_t *GUI_Screen;
lv_group_t *group_screen;
extern lv_indev_t *joy_indev;
lv_obj_t *Body;
lv_obj_t *button_1;
lv_obj_t *button_2;
lv_obj_t *button_3;
lv_obj_t *button_4;
lv_obj_t *button_4;
lv_obj_t *button_5;
lv_obj_t *button_6;
int Select = 1;
lv_obj_t *menu_list[MENU_LIST];
lv_obj_t *button_virtual;
static int16_t label_scroll_y = 0;
static int16_t max_scroll_y = 0;
static TaskHandle_t task_handle = NULL;
static TaskHandle_t task_ui_handle = NULL;

void ui_menu_setting(void);
void ui_test_text_long_img(void);
void Task_debug_log(void);

/* TinyUSB descriptors
 ********************************************************************* */
#define EPNUM_MSC 1
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum
{
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum
{
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN = 0x80,

    EDPT_MSC_OUT = 0x01,
    EDPT_MSC_IN = 0x81,
};

// static uint8_t const desc_configuration[] = {
//     // Config number, interface count, string index, total length, attribute, power in mA
//     TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

//     // Interface number, string index, EP Out & EP In address, EP size
//     TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
// };

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, // This is Espressif VID. This needs to be changed according to Users / Customers
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01};

static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "TinyUSB",                  // 1: Manufacturer
    "TinyUSB Device",           // 2: Product
    "123456",                   // 3: Serials
    "Example MSC",              // 4. MSC
};
/*********************************************************************** TinyUSB descriptors*/

#define BASE_PATH "/data" // base path to mount the partition

#define PROMPT_STR CONFIG_IDF_TARGET
static int console_unmount(int argc, char **argv);
static int console_read(int argc, char **argv);
static int console_write(int argc, char **argv);
static int console_size(int argc, char **argv);
static int console_status(int argc, char **argv);
static int console_exit(int argc, char **argv);
const esp_console_cmd_t cmds[] = {
    {
        .command = "read",
        .help = "read BASE_PATH/README.MD and print its contents",
        .hint = NULL,
        .func = &console_read,
    },
    {
        .command = "write",
        .help = "create file BASE_PATH/README.MD if it does not exist",
        .hint = NULL,
        .func = &console_write,
    },
    {
        .command = "size",
        .help = "show storage size and sector size",
        .hint = NULL,
        .func = &console_size,
    },
    {
        .command = "expose",
        .help = "Expose Storage to Host",
        .hint = NULL,
        .func = &console_unmount,
    },
    {
        .command = "status",
        .help = "Status of storage exposure over USB",
        .hint = NULL,
        .func = &console_status,
    },
    {
        .command = "exit",
        .help = "exit from application",
        .hint = NULL,
        .func = &console_exit,
    }};

// mount the partition and show all the files in BASE_PATH
static void _mount(void)
{
    ESP_LOGI(TAG, "Mount storage...");
    tinyusb_msc_storage_mount(BASE_PATH);

    // List all the files in this directory
    ESP_LOGI(TAG, "\nls command output:");
    struct dirent *d;
    DIR *dh = opendir(BASE_PATH);
    if (!dh)
    {
        if (errno == ENOENT)
        {
            // If the directory is not found
            ESP_LOGE(TAG, "Directory doesn't exist %s", BASE_PATH);
        }
        else
        {
            // If the directory is not readable then throw error and exit
            ESP_LOGE(TAG, "Unable to read directory %s", BASE_PATH);
        }
        return;
    }
    // While the next entry is not readable we will print directory files
    while ((d = readdir(dh)) != NULL)
    {
        printf("%s\n", d->d_name);
    }
    return;
}

// unmount storage
static int console_unmount(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host())
    {
        ESP_LOGE(TAG, "storage is already exposed");
        return -1;
    }
    ESP_LOGI(TAG, "Unmount storage...");
    ESP_ERROR_CHECK(tinyusb_msc_storage_unmount());
    return 0;
}

// read BASE_PATH/README.MD and print its contents
static int console_read(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host())
    {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't read from storage.");
        return -1;
    }
    ESP_LOGD(TAG, "read from storage:");
    const char *filename = BASE_PATH "/README.MD";
    FILE *ptr = fopen(filename, "r");
    if (ptr == NULL)
    {
        ESP_LOGE(TAG, "Filename not present - %s", filename);
        return -1;
    }
    char buf[1024];
    while (fgets(buf, 1000, ptr) != NULL)
    {
        printf("%s", buf);
    }
    fclose(ptr);
    return 0;
}

// create file BASE_PATH/README.MD if it does not exist
static int console_write(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host())
    {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't write to storage.");
        return -1;
    }
    ESP_LOGD(TAG, "write to storage:");
    const char *filename = BASE_PATH "/README.MD";
    FILE *fd = fopen(filename, "r");
    if (!fd)
    {
        ESP_LOGW(TAG, "README.MD doesn't exist yet, creating");
        fd = fopen(filename, "w");
        fprintf(fd, "Mass Storage Devices are one of the most common USB devices. It use Mass Storage Class (MSC) that allow access to their internal data storage.\n");
        fprintf(fd, "In this example, ESP chip will be recognised by host (PC) as Mass Storage Device.\n");
        fprintf(fd, "Upon connection to USB host (PC), the example application will initialize the storage module and then the storage will be seen as removable device on PC.\n");
        fclose(fd);
    }
    return 0;
}

// Show storage size and sector size
static int console_size(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host())
    {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't access storage");
        return -1;
    }
    uint32_t sec_count = tinyusb_msc_storage_get_sector_count();
    uint32_t sec_size = tinyusb_msc_storage_get_sector_size();
    printf("Storage Capacity %lluMB\n", ((uint64_t)sec_count) * sec_size / (1024 * 1024));
    return 0;
}

// exit from application
static int console_status(int argc, char **argv)
{
    printf("storage exposed over USB: %s\n", tinyusb_msc_storage_in_use_by_usb_host() ? "Yes" : "No");
    return 0;
}

// exit from application
static int console_exit(int argc, char **argv)
{
    tinyusb_msc_storage_deinit();
    printf("Application Exiting\n");
    exit(0);
    return 0;
}
static esp_err_t storage_init_sdmmc(sdmmc_card_t **card)
{
    esp_err_t ret = ESP_OK;
    bool host_init = false;
    sdmmc_card_t *sd_card;

    ESP_LOGI(TAG, "Initializing SDCard");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    host.max_freq_khz = SDMMC_FREQ_52M;
    host.flags &= ~SDMMC_HOST_FLAG_DDR;
    if (host.flags & SDMMC_HOST_FLAG_DDR)
    {
        ESP_LOGW(__func__, "card and host support DDR mode");
    }
    else
    {
        ESP_LOGW(__func__, "card and host not support DDR mode");
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // For SD Card, set bus width to use
    slot_config.width = 4;

    // On chips where the GPIOs used for SD card can be configured, set the user defined values
    slot_config.clk = 44;
    slot_config.cmd = 43;
    slot_config.d0 = 47;
    slot_config.d1 = 21;
    slot_config.d2 = 14;
    slot_config.d3 = 13;

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    // not using ff_memalloc here, as allocation in internal RAM is preferred
    sd_card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    ESP_GOTO_ON_FALSE(sd_card, ESP_ERR_NO_MEM, clean, TAG, "could not allocate new sdmmc_card_t");

    ESP_GOTO_ON_ERROR((*host.init)(), clean, TAG, "Host Config Init fail");
    host_init = true;

    ESP_GOTO_ON_ERROR(sdmmc_host_init_slot(host.slot, (const sdmmc_slot_config_t *)&slot_config),
                      clean, TAG, "Host init slot fail");

    while (sdmmc_card_init(&host, sd_card))
    {
        ESP_LOGE(TAG, "The detection pin of the slot is disconnected(Insert uSD card). Retrying...");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, sd_card);
    *card = sd_card;

    return ESP_OK;

clean:
    if (host_init)
    {
        if (host.flags & SDMMC_HOST_FLAG_DEINIT_ARG)
        {
            host.deinit_p(host.slot);
        }
        else
        {
            (*host.deinit)();
        }
    }
    if (sd_card)
    {
        free(sd_card);
        sd_card = NULL;
    }
    return ret;
}

void Init_MSC(void)
{
    ESP_LOGI(TAG, "Initializing storage...");
    static sdmmc_card_t *card = NULL;
    ESP_ERROR_CHECK(storage_init_sdmmc(&card));

    const tinyusb_msc_sdmmc_config_t config_sdmmc = {
        .card = card};
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_sdmmc(&config_sdmmc));

    // mounted in the app by default
    _mount();
}

void ui_event_screen(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_SCREEN_LOADED)
    {
        lv_indev_set_group(joy_indev, group_screen);
        lv_group_add_obj(group_screen, GUI_Screen);
    }
}

static lv_obj_t *ui_create_panel(lv_obj_t *obj,
                                 lv_coord_t width,
                                 lv_coord_t height,
                                 lv_coord_t x,
                                 lv_coord_t y,
                                 bool flex_enable,
                                 lv_flex_flow_t flow,
                                 lv_flex_align_t main_flex,
                                 lv_coord_t pad_left,
                                 lv_coord_t pad_top,
                                 lv_coord_t space_row,
                                 lv_coord_t space_col,
                                 bool scoll_enable)
{
    lv_obj_t *panel = lv_obj_create(obj);
    lv_obj_set_width(panel, width);
    lv_obj_set_height(panel, height);
    lv_obj_set_x(panel, x);
    lv_obj_set_y(panel, y);
    lv_obj_set_align(panel, LV_ALIGN_TOP_MID);
    if (flex_enable)
    {
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(panel, flow);
        lv_obj_set_flex_align(panel, main_flex, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_BETWEEN);
    }
    if (scoll_enable == true)
    {
        lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_scroll_dir(panel, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x305CFF), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
    }
    else
    {
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_set_style_radius(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(panel, pad_left, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(panel, pad_top, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(panel, space_row, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(panel, space_col, LV_PART_MAIN | LV_STATE_DEFAULT);

    return panel;
}

static lv_obj_t *ui_create_image(lv_obj_t *obj, const void *src)
{
    lv_obj_t *img = lv_img_create(obj);
    lv_img_set_src(img, src);
    lv_obj_set_width(img, LV_SIZE_CONTENT);
    lv_obj_set_height(img, LV_SIZE_CONTENT);
    lv_obj_set_align(img, LV_ALIGN_CENTER);
    lv_obj_add_flag(img, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    return img;
}

static lv_obj_t *ui_create_panel_text(lv_obj_t *obj, char *text, lv_coord_t width, lv_text_align_t align_text, const lv_font_t *value, lv_color_t color_txt)
{

    lv_obj_t *panel_text;
    panel_text = lv_label_create(obj);
    lv_obj_set_width(panel_text, width);            /// 1
    lv_obj_set_height(panel_text, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(panel_text, LV_ALIGN_TOP_MID);
    lv_label_set_text(panel_text, text);
    lv_obj_set_style_text_align(panel_text, align_text, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(panel_text, color_txt, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(panel_text, 225, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(panel_text, value, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(panel_text, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_pad_left(panel_text, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(panel_text, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(panel_text, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(panel_text, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    // lv_obj_set_style_border_side(panel_text, LV_BORDER_SIDE_FULL, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_border_color(panel_text, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_border_opa(panel_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_border_width(panel_text, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    return panel_text;
}

static lv_obj_t *ui_create_button(lv_obj_t *obj,
                                  char *textButton,
                                  const lv_font_t *text_font,
                                  lv_color_t color_bg,
                                  lv_align_t align_text,
                                  lv_coord_t x,
                                  lv_coord_t y,
                                  lv_coord_t width,
                                  lv_coord_t height,
                                  bool enable_outline,
                                  lv_event_cb_t event_cb)
{
    lv_obj_t *button = lv_btn_create(obj);

    lv_obj_set_width(button, width);
    lv_obj_set_height(button, height);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_align(button, LV_ALIGN_TOP_MID);
    lv_obj_add_flag(button, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);    /// Flags
    lv_obj_set_style_radius(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, color_bg, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(button, lv_color_hex(0x50A2FF), LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_bg_opa(button, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (enable_outline == false)
    {
        lv_obj_set_style_border_side(button, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else
    {
        lv_obj_set_style_border_side(button, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(button, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_obj_set_style_border_color(button, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(button, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(button, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(button, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(button, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_pad(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(button, event_cb, LV_EVENT_ALL, NULL);

    if (textButton != NULL)
    {
        lv_obj_t *text = lv_label_create(button);
        lv_obj_set_width(text, LV_SIZE_CONTENT);  /// 1
        lv_obj_set_height(text, LV_SIZE_CONTENT); /// 1
        lv_obj_set_align(text, align_text);
        lv_label_set_text(text, textButton);
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(text, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(text, text_font, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    return button;
}

void ui_create_header(lv_obj_t *obj)
{
    lv_obj_t *obj_IconBLE;
    lv_obj_t *obj_IconWifi;
    lv_obj_t *obj_header = lv_obj_create(obj);
    lv_obj_set_width(obj_header, LCD_H_RES);
    lv_obj_set_height(obj_header, 30);
    lv_obj_set_x(obj_header, 0);
    lv_obj_set_y(obj_header, 0);
    lv_obj_set_align(obj_header, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(obj_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(obj_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(obj_header, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_radius(obj_header, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(obj_header, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(obj_header, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj_header, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(obj_header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);

    obj_IconWifi = lv_label_create(obj_header);
    lv_obj_set_width(obj_IconWifi, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(obj_IconWifi, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(obj_IconWifi, LV_ALIGN_CENTER);
    lv_label_set_text(obj_IconWifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(obj_IconWifi, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj_IconWifi, lv_color_hex(0x317DCC), LV_PART_MAIN | LV_STATE_DEFAULT);

    obj_IconBLE = lv_label_create(obj_header);
    lv_obj_set_width(obj_IconBLE, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(obj_IconBLE, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(obj_IconBLE, LV_ALIGN_CENTER);
    lv_label_set_text(obj_IconBLE, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_font(obj_IconBLE, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj_IconBLE, lv_color_hex(0xA09A9A), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void ui_create_title(lv_obj_t *obj, char *text, const lv_font_t *value, lv_color_t color_txt)
{
    lv_obj_t *Title = lv_obj_create(obj);
    lv_obj_set_width(Title, LCD_H_RES);
    lv_obj_set_height(Title, 30);
    lv_obj_set_x(Title, 0);
    lv_obj_set_y(Title, 0);
    lv_obj_set_align(Title, LV_ALIGN_TOP_MID);
    lv_obj_clear_flag(Title, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_radius(Title, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(Title, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(Title, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(Title, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(Title, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *txt_Title = lv_label_create(Title);
    lv_obj_set_width(txt_Title, LCD_H_RES);        /// 1
    lv_obj_set_height(txt_Title, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(txt_Title, LV_ALIGN_CENTER);
    lv_label_set_long_mode(txt_Title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(txt_Title, text);
    lv_obj_set_style_text_color(txt_Title, color_txt, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(txt_Title, 225, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(txt_Title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(txt_Title, value, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void lvgl_ui_test(void)
{
    lv_obj_t *panel_text;
    panel_text = lv_label_create(GUI_Screen);
    lv_obj_set_width(panel_text, 800);              /// 1
    lv_obj_set_height(panel_text, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(panel_text, LV_ALIGN_TOP_MID);
    lv_label_set_text(panel_text, "Battery In Vehicle Test conditions:\n\
- Parking Brake is ON.\n\
- Transmission in PARK or NEUTRAL.\n\
- Turn the ignition in the Key ON, Engine OFF position.\n\
- Place the headlights and blower motor in the OFF position.\n\
- For maintenance-type batteries: Check the electrolyte level. Inspect the battery for damage or corrosion.");
    lv_obj_set_style_text_align(panel_text, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(panel_text, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(panel_text, 225, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(panel_text, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(panel_text, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_pad_left(panel_text, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(panel_text, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(panel_text, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(panel_text, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *img1 = lv_img_create(GUI_Screen);
    lv_img_set_src(img1, IMG_1_NO_SELECT);
    lv_obj_set_width(img1, LV_SIZE_CONTENT);
    lv_obj_set_height(img1, LV_SIZE_CONTENT);
    lv_obj_set_align(img1, LV_ALIGN_CENTER);
    lv_obj_add_flag(img1, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(img1, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    // lv_obj_t *img2 = lv_img_create(GUI_Screen);
    // lv_img_set_src(img2, IMG_1_NO_SELECT);
    // lv_obj_set_width(img2, LV_SIZE_CONTENT);
    // lv_obj_set_height(img2, LV_SIZE_CONTENT);
    // lv_obj_set_align(img2, LV_ALIGN_CENTER);
    // lv_obj_add_flag(img2, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    // lv_obj_clear_flag(img2, LV_OBJ_FLAG_SCROLLABLE); /// Flags
}

void ui_focus_obj(lv_obj_t *obj)
{
    if (obj == NULL)
    {
        ESP_LOGE(__func__, "obj NULL");
        return;
    }
    if (lv_obj_get_state(obj) & LV_STATE_DISABLED)
        return;
    lv_group_focus_obj(obj);
    // ESP_LOGW(__func__, "lv_obj_get_state = %X", (lv_obj_get_state(obj)&&LV_STATE_FOCUS_KEY));
    if (lv_obj_get_state(obj) && LV_STATE_FOCUS_KEY)
    {
        lv_obj_clear_state(obj, LV_STATE_FOCUS_KEY);
    }
}

void ui_create_screen(void)
{
    GUI_Screen = lv_obj_create(NULL);
    lv_obj_clear_flag(GUI_Screen, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_add_event_cb(GUI_Screen, ui_event_screen, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(GUI_Screen, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(GUI_Screen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_scr_load_anim(GUI_Screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}

static void Proc_HomeScreenKeyInit(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *button = lv_event_get_target(e);
    if (event_code == LV_EVENT_KEY)
    {
        lv_obj_t *obj = lv_event_get_target(e);
        if (lv_indev_get_key(joy_indev) == LV_KEY_UP || lv_indev_get_key(joy_indev) == LV_KEY_DOWN)
        {
            for (int ind = 1; ind <= 3; ind++)
            {
                if (Select == ind)
                {
                    Select = 3 + Select;
                }
                else if (Select == 3 + ind)
                {
                    Select = Select - 3;
                }
            }
            switch (Select)
            {
            case 1:
                ui_focus_obj(button_1);
                break;
            case 2:
                ui_focus_obj(button_2);
                break;
            case 3:
                ui_focus_obj(button_3);
                break;
            case 4:
                ui_focus_obj(button_4);
                break;
            case 5:
                ui_focus_obj(button_5);
                break;
            case 6:
                ui_focus_obj(button_6);
                break;
            default:
                break;
            }
        }
        if (lv_indev_get_key(joy_indev) == LV_KEY_RIGHT)
        {
            if (Select == 6)
            {
                Select = 1;
            }
            else
            {
                Select++;
            }
            ESP_LOGW(__func__, "Select = %d", Select);
            switch (Select)
            {
            case 1:
                ui_focus_obj(button_1);
                break;
            case 2:
                ui_focus_obj(button_2);
                break;
            case 3:
                ui_focus_obj(button_3);
                break;
            case 4:
                ui_focus_obj(button_4);
                break;
            case 5:
                ui_focus_obj(button_5);
                break;
            case 6:
                ui_focus_obj(button_6);
                break;
            default:
                break;
            }
        }
        else if (lv_indev_get_key(joy_indev) == LV_KEY_LEFT)
        {
            if (Select == 1)
            {
                Select = 6;
            }
            else
            {
                Select--;
            }

            ESP_LOGW(__func__, "Select = %d", Select);
            switch (Select)
            {
            case 1:
                ui_focus_obj(button_1);
                break;
            case 2:
                ui_focus_obj(button_2);
                break;
            case 3:
                ui_focus_obj(button_3);
                break;
            case 4:
                ui_focus_obj(button_4);
                break;
            case 5:
                ui_focus_obj(button_5);
                break;
            case 6:
                ui_focus_obj(button_6);
                break;
            default:
                break;
            }
        }
        else if (lv_indev_get_key(joy_indev) == LV_KEY_ENTER)
        {
            switch (Select)
            {
            case 1:
                ui_test_text_long_img();
                break;
            case 2:

                break;
            case 3:

                break;
            case 4:

                break;
            case 5:

                break;
            case 6:
                ui_menu_setting();
                break;
            default:
                break;
            }
        }
    }
}

void lvgl_home_body(void)
{
    if (lvgl_lock(-1))
    {
        Select = 1;
        ui_create_screen();
        ui_create_title(GUI_Screen, "Innova LVGL Test", &lv_font_montserrat_30, lv_color_hex(0x1917FF));
        Body = lv_obj_create(GUI_Screen);
        lv_obj_set_width(Body, 320);
        lv_obj_set_height(Body, 450);
        lv_obj_set_x(Body, 0);
        lv_obj_set_y(Body, 30);
        lv_obj_set_align(Body, LV_ALIGN_TOP_MID);
        lv_obj_set_flex_flow(Body, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(Body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_clear_flag(Body, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_bg_color(Body, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(Body, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(Body, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(Body, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(Body, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(Body, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(Body, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_row(Body, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_column(Body, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

        button_1 = lv_btn_create(Body);
        lv_obj_set_width(button_1, IMG_WIDTH);
        lv_obj_set_height(button_1, IMG_HEIGHT);
        lv_obj_set_align(button_1, LV_ALIGN_CENTER);
        lv_obj_add_flag(button_1, LV_OBJ_FLAG_ADV_HITTEST);
        // lv_obj_add_flag(button_1, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
        lv_obj_clear_flag(button_1, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_radius(button_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_1, IMG_1_NO_SELECT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_1, IMG_1_SELECT, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(button_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_spread(button_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_color(button_1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_opa(button_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(button_1, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_pad(button_1, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(button_1, Proc_HomeScreenKeyInit, LV_EVENT_ALL, NULL);
        if (lv_obj_get_state(button_1) & LV_STATE_DISABLED)
            return;
        lv_group_focus_obj(button_1);
        // ESP_LOGW(__func__, "lv_obj_get_state = %X", (lv_obj_get_state(obj)&&LV_STATE_FOCUS_KEY));
        if (lv_obj_get_state(button_1) && LV_STATE_FOCUS_KEY)
        {
            lv_obj_clear_state(button_1, LV_STATE_FOCUS_KEY);
        }

        lv_obj_clear_state(button_1, LV_STATE_FOCUS_KEY);

        button_2 = lv_btn_create(Body);
        lv_obj_set_width(button_2, IMG_WIDTH);
        lv_obj_set_height(button_2, IMG_HEIGHT);
        lv_obj_set_align(button_2, LV_ALIGN_CENTER);
        lv_obj_add_flag(button_2, LV_OBJ_FLAG_ADV_HITTEST);
        // lv_obj_add_flag(button_2, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
        lv_obj_clear_flag(button_2, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_radius(button_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_2, IMG_2_NO_SELECT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_2, IMG_2_SELECT, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(button_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_spread(button_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_color(button_2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_opa(button_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(button_2, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_pad(button_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(button_2, Proc_HomeScreenKeyInit, LV_EVENT_ALL, NULL);

        button_3 = lv_btn_create(Body);
        lv_obj_set_width(button_3, IMG_WIDTH);
        lv_obj_set_height(button_3, IMG_HEIGHT);
        lv_obj_set_align(button_3, LV_ALIGN_CENTER);
        lv_obj_add_flag(button_3, LV_OBJ_FLAG_ADV_HITTEST);
        // lv_obj_add_flag(button_3, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
        lv_obj_clear_flag(button_3, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_radius(button_3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_3, IMG_3_NO_SELECT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_3, IMG_3_SELECT, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(button_3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_spread(button_3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_color(button_3, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_opa(button_3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(button_3, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_pad(button_3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(button_3, Proc_HomeScreenKeyInit, LV_EVENT_ALL, NULL);

        button_4 = lv_btn_create(Body);
        lv_obj_set_width(button_4, IMG_WIDTH);
        lv_obj_set_height(button_4, IMG_HEIGHT);
        lv_obj_set_align(button_4, LV_ALIGN_CENTER);
        lv_obj_add_flag(button_4, LV_OBJ_FLAG_ADV_HITTEST);
        // lv_obj_add_flag(button_4, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
        lv_obj_clear_flag(button_4, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_radius(button_4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_4, IMG_4_NO_SELECT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_4, IMG_4_SELECT, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(button_4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_spread(button_4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_color(button_4, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_opa(button_4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(button_4, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_pad(button_4, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(button_4, Proc_HomeScreenKeyInit, LV_EVENT_ALL, NULL);

        button_5 = lv_btn_create(Body);
        lv_obj_set_width(button_5, IMG_WIDTH);
        lv_obj_set_height(button_5, IMG_HEIGHT);
        lv_obj_set_align(button_5, LV_ALIGN_CENTER);
        lv_obj_add_flag(button_5, LV_OBJ_FLAG_ADV_HITTEST);
        // lv_obj_add_flag(button_5, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
        lv_obj_clear_flag(button_5, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_radius(button_5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_5, IMG_5_NO_SELECT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_5, IMG_5_SELECT, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(button_5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_spread(button_5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_color(button_5, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_opa(button_5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(button_5, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_pad(button_5, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(button_5, Proc_HomeScreenKeyInit, LV_EVENT_ALL, NULL);

        button_6 = lv_btn_create(Body);
        lv_obj_set_width(button_6, IMG_WIDTH);
        lv_obj_set_height(button_6, IMG_HEIGHT);
        lv_obj_set_align(button_6, LV_ALIGN_CENTER);
        lv_obj_add_flag(button_6, LV_OBJ_FLAG_ADV_HITTEST);
        // lv_obj_add_flag(button_6, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
        lv_obj_clear_flag(button_6, LV_OBJ_FLAG_SCROLLABLE); /// Flags
        lv_obj_set_style_radius(button_6, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_6, IMG_6_NO_SELECT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_src(button_6, IMG_6_SELECT, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(button_6, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_spread(button_6, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_color(button_6, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_opa(button_6, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(button_6, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_pad(button_6, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(button_6, Proc_HomeScreenKeyInit, LV_EVENT_ALL, NULL);
    }
    lvgl_unlock();
}

static void menu_cb(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *button = lv_event_get_target(e);
    if (event_code == LV_EVENT_KEY)
    {
        if (lv_indev_get_key(joy_indev) == LV_KEY_UP)
        {
            if (Select == 0)
            {
                Select = MENU_LIST - 1;
            }
            else
            {
                Select--;
            }
        }
        if (lv_indev_get_key(joy_indev) == LV_KEY_DOWN)
        {
            if (Select == MENU_LIST - 1)
            {
                Select = 0;
            }
            else
            {
                Select++;
            }
        }
        ui_focus_obj(menu_list[Select]);
        if (lv_indev_get_key(joy_indev) == LV_KEY_ESC || lv_indev_get_key(joy_indev) == LV_KEY_HOME)
        {
            lvgl_home_body();
        }
    }
}

void ui_menu_setting(void)
{
    ui_create_screen();
    lv_obj_t *Body = ui_create_panel(GUI_Screen, LCD_H_RES, 480, 0, 0, true, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_START, 0, -3, 0, 0, true);
    char str_menu[128];
    Select = 0;
    for (int ind = 0; ind < MENU_LIST; ind++)
    {
        memset(str_menu, 0x0, sizeof(str_menu));
        sprintf(str_menu, "Setting item %d", ind + 1);
        menu_list[ind] = ui_create_button(Body, str_menu, &lv_font_montserrat_24, lv_color_hex(0xFFFFFF), LV_ALIGN_LEFT_MID, 0, 0, LCD_H_RES, LV_SIZE_CONTENT, true, menu_cb);
    }
    ui_focus_obj(menu_list[0]);
}

void ui_scroll_text(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *button = lv_event_get_target(e);
    if (event_code == LV_EVENT_KEY)
    {
        if (lv_indev_get_key(joy_indev) == LV_KEY_UP)
        {
            label_scroll_y = label_scroll_y - SCROLL_STEP;
            if (label_scroll_y <= SCROLL_Y_MIN)
                label_scroll_y = SCROLL_Y_MIN;
            ESP_LOGI(__FILE__, "Func: %s - Up Scroll: %d", __func__, label_scroll_y);
            lv_obj_scroll_to_y(Body, label_scroll_y, LV_ANIM_OFF);
        }

        if (lv_indev_get_key(joy_indev) == LV_KEY_DOWN)
        {
            label_scroll_y += SCROLL_STEP;
            ESP_LOGI(__FILE__, "Func: %s - Down Scroll: %d", __func__, label_scroll_y);
            lv_obj_scroll_to_y(Body, label_scroll_y, LV_ANIM_OFF);
            if (label_scroll_y >= lv_obj_get_scroll_y(Body))
                label_scroll_y = lv_obj_get_scroll_y(Body);
        }

        if (lv_indev_get_key(joy_indev) == LV_KEY_ESC || lv_indev_get_key(joy_indev) == LV_KEY_HOME)
        {
            lvgl_home_body();
        }
    }
}

void ui_test_text_long_img(void)
{
    label_scroll_y = 0;
    const char *text = "Generates random dummy text (Lorem Ipsum).\n\
\n\
Simple, quick and unobtrusive dummy text generator for web designers and developers. This extension installs a small icon in your browser allowing you quick access to paragraphs of dummy text (Lorem ipsum style) at the click of a button.\n\
\n\
The number of paragraphs, words per paragraph and whether or not to wrap paragraphs in paragraph tags can be changed easily. Once happy with your selection, simply click the Copy button and the text is sent straight to your clipboard ready for pasting!\n\
\n\
This extension is open source and the source code is available on GitHub: http://github.com/davgothic/DummyText";
    ui_create_screen();
    Body = ui_create_panel(GUI_Screen, LCD_H_RES, 480, 0, 0, true, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_START, 0, -3, 0, 0, true);
    ui_create_panel_text(Body, text, LCD_H_RES - 20, LV_TEXT_ALIGN_LEFT, &lv_font_montserrat_24, lv_color_hex(0x000000));
    ui_create_image(Body, IMG_6_SELECT);
    ui_create_panel_text(Body, text, LCD_H_RES - 20, LV_TEXT_ALIGN_LEFT, &lv_font_montserrat_24, lv_color_hex(0x000000));
    ui_create_image(Body, IMG_6_SELECT);

    button_virtual = lv_btn_create(Body);
    lv_obj_set_style_bg_opa(button_virtual, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(button_virtual, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(button_virtual, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(button_virtual, ui_scroll_text, LV_EVENT_ALL, NULL);
    ui_focus_obj(button_virtual);
    lv_obj_scroll_to_y(Body, 0, LV_ANIM_OFF);
}

void lvgl_example_img_test(void)
{
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    group_screen = lv_group_create();
    lv_group_set_default(group_screen);
    ui_create_screen();

    for (int ind = 0; ind < 10; ind++)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    LCD_SetBrightness(4);
    lvgl_home_body();
    // lvgl_ui_test();
}

void debug_log_mem(void *param)
{
    while (1)
    {
        ESP_LOGI(__func__, "Heap size: %" PRIu32 " KB", esp_get_free_heap_size() / 1024);
        ESP_LOGW(__func__, "Internal heap size: %" PRIu32 " KB", esp_get_free_internal_heap_size() / 1024);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void Task_debug_log(void)
{
    xTaskCreatePinnedToCore(debug_log_mem, "debug_log_mem", 8 * 1024,
                            NULL, 0 | portPRIVILEGE_BIT,
                            &task_handle, 0);
}

void app_main(void)
{
    Init_MSC();
    Init_Driver();

    lvgl_register_fatfs_emmc();
    lvgl_example_img_test();
    LCD_SetBrightness(4);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    return;
}
