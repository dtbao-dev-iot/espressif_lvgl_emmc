#include "Key.h"
#include "Buzzer.h"
#include "lvgl/lvgl.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
/*=============================================================================
*
*   @section Macros
*
/=============================================================================*/
#define TAG "Key"
#define VOL_DETECT              100
#define PIN_ONOFF_LINK          17
#define PIN_POWER_EN            46
#define WAIT_TIME_SYSTEM_INIT   2000

typedef enum 
{
    VOLT_KEY_HOME    = 30,//0 - 140
    VOLT_KEY_BACK    = 580,//370 - 570
    VOLT_KEY_ENTER   = 880,//800 - 1000 
    VOLT_KEY_UP      = 1340,//1270 - 1470
    VOLT_KEY_DN      = 1780,//1710 - 1910
    VOLT_KEY_LEFT    = 2140,//2090 - 2290
    VOLT_KEY_RIGHT   = 2500,//2420 - 2620
}voltage_button_t;
/* -------------------- Characterization Helper Data Types ------------------ */
/* ----------------------------- Context Structure--------------------------- */
/*=============================================================================
*
*   @section Private functions
*
/=============================================================================*/

status_button_t KEY_GetState(void);

/*=============================================================================
*
*   @section Private data
*
/=============================================================================*/
status_button_t Cur_Button = KEY_NONE;
static bool key_stt = false;

void init_key(void)
{
    gpio_reset_pin(PIN_ONOFF_LINK);
    gpio_set_direction(PIN_ONOFF_LINK, GPIO_MODE_INPUT);
    gpio_reset_pin(PIN_POWER_EN);
    gpio_set_direction(PIN_POWER_EN, GPIO_MODE_OUTPUT);
}

void Wait_ConfirmToolOn(void)
{
    while (esp_log_timestamp() < WAIT_TIME_SYSTEM_INIT)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }  
    TurnON_Tool();
}

void TurnON_Tool(void)
{
    // ESP_LOGE(__func__, "Done");
    gpio_set_level(PIN_POWER_EN, 1);
}

void TurnOFF_Tool(void)
{
    gpio_set_level(PIN_POWER_EN, 0);
    while (1);
}

static uint32_t adc_button_get_mv()
{
    return 0;
}

status_button_t KEY_GetState(void)
{
    uint32_t volt_button = adc_button_get_mv();
    status_button_t button = KEY_NONE;
    if(abs(volt_button - VOLT_KEY_HOME) < VOL_DETECT)
    {
        button = KEY_HOME;
    }
    if(abs(volt_button - VOLT_KEY_BACK) < VOL_DETECT)
    {
        button = KEY_BACK;
    }
    if(abs(volt_button - VOLT_KEY_ENTER) < VOL_DETECT)
    {
        button = KEY_ENTER;
    }
    if(abs(volt_button - VOLT_KEY_UP) < VOL_DETECT)
    {
        button = KEY_UP;
    }
    if(abs(volt_button - VOLT_KEY_DN) < VOL_DETECT)
    {
        button = KEY_DN;
    }
    if(abs(volt_button - VOLT_KEY_LEFT) < VOL_DETECT)
    {
        button = KEY_LEFT;
    }
    if(abs(volt_button - VOLT_KEY_RIGHT) < VOL_DETECT)
    {
        button = KEY_RIGHT;
    }
    // if (key_stt == false)
    // {
    //     button = KEY_NONE;
    // }
    return button;
}

status_button_t Proc_GetStatusKey(void)
{
    status_button_t _button = KEY_NONE;
    status_button_t button = KEY_NONE;
    uint8_t index_retry = 0;
    _button = KEY_GetState();
    if(_button != KEY_NONE)
    {
        while (index_retry < 3)
        {
            index_retry++;
            _button = KEY_GetState(); 
            if(_button == KEY_NONE)
            {
                button = KEY_NONE;
                break;
            } 
            if (_button != button)
            {
                button = _button;
                index_retry = 0;
            }
        }
    }
    else
    {
        Cur_Button = KEY_NONE;
    }
    if(button != Cur_Button)
    {
        Cur_Button = button;
    }
    return button;
}

bool Proc_GetStatusKeyOnOff(void)
{
    if(!gpio_get_level(PIN_ONOFF_LINK))
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        if(!gpio_get_level(PIN_ONOFF_LINK))
        {
            // ESP_LOGE(__func__, "Press");
            return true;
        }
    }
    return false;
}

status_button_t Get_KeyCur(void)
{
    if (Proc_GetStatusKeyOnOff() == true)
    {
        return KEY_POWER;
    }
    return Cur_Button;
}

void Enable_AllKey(void)
{
    key_stt = true;
}

void Disable_AllKey(void)
{
    key_stt = false;
}