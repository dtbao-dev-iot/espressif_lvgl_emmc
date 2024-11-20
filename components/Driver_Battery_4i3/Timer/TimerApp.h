#ifndef _TIMER_APP_H_
#define _TIMER_APP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"


void SystemTimer_Event1_Timeout_Set(uint32_t iTimeoutMiliSecond);
bool SystemTimer_Event1_Timeout_Get(void);
void SystemTimer_Event2_Timeout_Set(uint32_t iTimeoutMiliSecond);
bool SystemTimer_Event2_Timeout_Get(void);
void SystemTimer_UpdateHeader_Timeout_Set(uint32_t iTimeoutMiliSecond);
bool SystemTimer_UpdateHeader_Timeout_Get(void);
void SystemTimer_UpdateMonitor_Timeout_Set(uint32_t iTimeoutMiliSecond);
bool SystemTimer_UpdateMonitor_Timeout_Get(void);
void SystemTimer_UpdateTurnOff_Timeout_Set(uint32_t iTimeoutMiliSecond);
bool SystemTimer_UpdateTurnOff_Timeout_Get(void);
void SystemTimer_UpdateVBAT_DIV_Timeout_Set(uint32_t iTimeoutMiliSecond);
bool SystemTimer_UpdateVBAT_DIV_Timeout_Get(void);
void Init_Timer_System(void);
void Result_Timer(void);


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif