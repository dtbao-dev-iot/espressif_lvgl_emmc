#include "usb_serial.h"

#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"

#define TAG __func__
#define DEBUG_USB_DATA 1
#define ENABLE_USB 1
#define PIN_USB_VBUS 16

#if CONFIG_SPIRAM
static EXT_RAM_BSS_ATTR uint8_t buf_rx[CONFIG_TINYUSB_CDC_RX_BUFSIZE];
static EXT_RAM_BSS_ATTR uint8_t buf_tx[CONFIG_TINYUSB_CDC_TX_BUFSIZE];
EXT_RAM_BSS_ATTR structUSBData strtUSBData;
#else
static uint8_t buf_rx[CONFIG_TINYUSB_CDC_RX_BUFSIZE];
static uint8_t buf_tx[CONFIG_TINYUSB_CDC_TX_BUFSIZE];
structUSBData strtUSBData;
#endif
int channel = 0;
size_t receive_data = 0;
size_t size_handle = 0;

static void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event);
static void USBD_CtlPrepareRx(uint8_t* buffer, uint32_t iSize);

const char* hid_string_descriptor[5] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "Innova",             // 1: Manufacturer
    "Innova USB Device",      // 2: Product
    "",              // 3: Serials, should use chip ID
    "HID interface",  // 4: HID
};

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x1720, // This is Espressif VID. This needs to be changed according to Users / Customers
    .idProduct = 0x430,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

static void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    /* initialization */
    size_t rx_size = 0;
    channel = itf;

    /* read */
    esp_err_t ret = tinyusb_cdcacm_read(itf, &buf_rx[receive_data], CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {
        receive_data += rx_size;
        // ESP_LOGI(TAG, "receive_data: %d", receive_data);
        // if (receive_data == SizeReportID_1 + 16
        // )
        {
            #if(DEBUG_USB_DATA)
            ESP_LOGI(TAG, "receive_data: %d", receive_data);
            // ESP_LOG_BUFFER_HEXDUMP(TAG, buf_rx, receive_data, ESP_LOG_INFO);
            #endif
            USBD_CtlPrepareRx(buf_rx, receive_data);
            receive_data = 0;
        }
        
    } else {
        ESP_LOGE(TAG, "Read error");
    }
}

static void init_usb_vbus(void)
{
    gpio_reset_pin(PIN_USB_VBUS);
    gpio_set_direction(PIN_USB_VBUS, GPIO_MODE_INPUT);
}

bool get_usb_vbus(void)
{
    #if ENABLE_USB
    if(!gpio_get_level(PIN_USB_VBUS))
    {
        vTaskDelay(pdMS_TO_TICKS(30));
        if(!gpio_get_level(PIN_USB_VBUS))
        {
            return false;
        }
    }
    return true;
    #else
    return false;
    #endif
}

void init_usb_serial(void)
{
    #if ENABLE_USB
    ESP_LOGI(TAG, "USB initialization");
    descriptor_config.idProduct = 0x430;
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &descriptor_config,
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .external_phy = false,
        .configuration_descriptor = NULL,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // tinyusb_config_cdcacm_t acm_cfg = {
    //     .usb_dev = TINYUSB_USBDEV_0,
    //     .cdc_port = TINYUSB_CDC_ACM_0,
    //     .rx_unread_buf_sz = CONFIG_TINYUSB_CDC_RX_BUFSIZE,
    //     .callback_rx = &tinyusb_cdc_rx_callback, // the first way to register a callback
    //     .callback_rx_wanted_char = NULL,
    //     .callback_line_state_changed = NULL,
    //     .callback_line_coding_changed = NULL
    // };

    // ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    // /* the second way to register a callback */
    // ESP_ERROR_CHECK(tinyusb_cdcacm_register_callback(
    //                     TINYUSB_CDC_ACM_0,
    //                     CDC_EVENT_LINE_STATE_CHANGED,
    //                     NULL));
    ESP_LOGI(TAG, "USB initialization DONE");
    #else
    ESP_LOGE(TAG, "USB process disable");
    #endif
    init_usb_vbus();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void USBD_CtlSendData(void)
{
    memset(buf_tx, 0x0, CONFIG_TINYUSB_CDC_TX_BUFSIZE);
    memcpy(buf_tx, strtUSBData.strtHidIn.p_bDataBuffer, strtUSBData.strtHidIn.iInSize);
    
    /* write back */
    #if(DEBUG_USB_DATA)
    // ESP_LOG_BUFFER_HEXDUMP(TAG, buf_tx, CONFIG_TINYUSB_CDC_TX_BUFSIZE, ESP_LOG_WARN);
    #endif
    tinyusb_cdcacm_write_queue(channel, buf_tx, CONFIG_TINYUSB_CDC_TX_BUFSIZE);
    tinyusb_cdcacm_write_flush(channel, 0);
    ESP_LOGI(TAG, "Done!");
}

static void USBD_CtlPrepareRx(uint8_t* buffer, uint32_t iSize)
{
    memset(&strtUSBData, 0x0, sizeof(strtUSBData));
    memcpy(&strtUSBData.strtHidOut, buffer, iSize);
    #if(DEBUG_USB_DATA)
    ESP_LOGI(TAG, "iReportId: %d", strtUSBData.strtHidOut.iReportId);
    ESP_LOGI(TAG, "iCommand: %d", strtUSBData.strtHidOut.iCommand);
    ESP_LOGI(TAG, "iAddress: %d", strtUSBData.strtHidOut.iAddress);
    ESP_LOGI(TAG, "iSize: %d", strtUSBData.strtHidOut.iSize);
    // ESP_LOG_BUFFER_HEXDUMP(TAG, strtUSBData.strtHidOut.iFlashBuffer, strtUSBData.strtHidOut.iSize, ESP_LOG_INFO);
    #endif
    // xTaskCreate(USBProcessData,
    //             "USBProcessData",
    //             4096,
    //             NULL,
    //             10,
    //             NULL);
}