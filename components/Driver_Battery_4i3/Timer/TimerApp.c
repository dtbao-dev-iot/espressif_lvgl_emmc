#include "TimerApp.h"
#include "esp_timer.h"

static esp_timer_handle_t tmr_handle;
static uint32_t timer_count = 0;
uint32_t jiffies = 0;
uint32_t iTimeOut_Event_1 = 0;
uint32_t iTimeOut_Event_2 = 0;
uint32_t iUpdateHeaderTimeOut = 0;
uint32_t iUpdateMonitorTimeOut = 0;
uint32_t iUpdateKeyPowerTimeOut = 0;
uint32_t iUpdateVBat_divTimeOut = 0;
uint32_t iTimeOutAfterJiffies;

void SystemTimer_Event1_Timeout_Set(uint32_t iTimeoutMiliSecond)
{
    iTimeOut_Event_1 = iTimeoutMiliSecond + esp_log_timestamp();
}

bool SystemTimer_Event1_Timeout_Get(void)
{    
    if (esp_log_timestamp() >= iTimeOut_Event_1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void SystemTimer_Event2_Timeout_Set(uint32_t iTimeoutMiliSecond)
{
    iTimeOut_Event_2 = iTimeoutMiliSecond + esp_log_timestamp();
}

bool SystemTimer_Event2_Timeout_Get(void)
{    
    if (esp_log_timestamp() >= iTimeOut_Event_2)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void SystemTimer_UpdateHeader_Timeout_Set(uint32_t iTimeoutMiliSecond)
{
    iUpdateHeaderTimeOut = iTimeoutMiliSecond + esp_log_timestamp();
}

bool SystemTimer_UpdateHeader_Timeout_Get(void)
{    
    if (esp_log_timestamp() >= iUpdateHeaderTimeOut)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void SystemTimer_UpdateMonitor_Timeout_Set(uint32_t iTimeoutMiliSecond)
{
    iUpdateMonitorTimeOut = iTimeoutMiliSecond + esp_log_timestamp();
}

bool SystemTimer_UpdateMonitor_Timeout_Get(void)
{    
    if (esp_log_timestamp() >= iUpdateMonitorTimeOut)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void SystemTimer_UpdateTurnOff_Timeout_Set(uint32_t iTimeoutMiliSecond)
{
    iUpdateKeyPowerTimeOut = iTimeoutMiliSecond + esp_log_timestamp();
}

bool SystemTimer_UpdateTurnOff_Timeout_Get(void)
{    
    if (esp_log_timestamp() >= iUpdateKeyPowerTimeOut)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void SystemTimer_UpdateVBAT_DIV_Timeout_Set(uint32_t iTimeoutMiliSecond)
{
    iUpdateVBat_divTimeOut = iTimeoutMiliSecond + esp_log_timestamp();
}

bool SystemTimer_UpdateVBAT_DIV_Timeout_Get(void)   
{    
    if (esp_log_timestamp() >= iUpdateVBat_divTimeOut)
    {
        return true;
    }
    else
    {
        return false;
    }
}


static void timer_callback(void)
{
    timer_count++;
}
void Init_Timer_System(void)
{
    timer_count = 0;
    esp_timer_create_args_t tmr_args;
    tmr_args.callback = timer_callback;
    tmr_args.skip_unhandled_events = false;
    tmr_args.name = "pulse_timer";
    tmr_args.dispatch_method = ESP_TIMER_TASK;
    esp_timer_create(&tmr_args, &tmr_handle);
    esp_timer_start_periodic(tmr_handle, 1);
}

void Result_Timer(void)
{
    esp_timer_stop(tmr_handle);
    esp_timer_delete(tmr_handle);
    ESP_LOGW("Timer", "Result = %"PRIu32"us", timer_count);
}