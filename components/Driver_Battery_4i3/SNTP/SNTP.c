#include "SNTP.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "TimerApp.h"

#define TAG __func__

static bool _InitSNTP = false;
static bool bValid = false;

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Setup RTC Time done");
    _InitSNTP = true; 
    bValid = true; 
}

void initialize_sntp(void)
{
    if (_InitSNTP) return;
    int retry = 0;
    const int retry_count = 10;

    ESP_LOGI(TAG, "Initializing SNTP");
    // Set timezone to VietName Standard Time
    // setenv("TZ", "GMT-07", 1);
    setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
    tzset();

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "time.cloudflare.com");
    sntp_setservername(2, "time.windows.com");
    sntp_setservername(3, "time.apple.com");
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();

    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }

    struct timeval outdelta;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS)
    {
        adjtime(NULL, &outdelta);
        ESP_LOGI(TAG, "Waiting for adjusting time ... outdelta = %jd sec: %li ms: %li us",
                    (intmax_t)outdelta.tv_sec,
                    outdelta.tv_usec/1000,
                    outdelta.tv_usec%1000);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    struct tm timeinfo = { 0 };
    time_t now = 0;
    
    SystemTimer_Event1_Timeout_Set(10000);
    while (1 && !SystemTimer_Event1_Timeout_Get())
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (_InitSNTP)
        {
            break;
        }
    }
    
    sntp_get_time();
}

struct tm sntp_get_time(void)
{
    struct tm timeinfo = { 0 };
    time_t now = 0;
    time(&now);
    tzset();
    localtime_r(&now, &timeinfo);
    ESP_LOGW(__func__, "%02d:%02d %02d/%02d/%04d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_year);
    
    return timeinfo;
}

void sntp_log_time(void)
{
    struct tm timeinfo = { 0 };
    time_t now = 0;
    time(&now);
    tzset();
    localtime_r(&now, &timeinfo);
    ESP_LOGW(__func__, "%02d:%02d %02d/%02d/%04d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_year);
}

void sntp_set_time(struct tm* iTimeinfo)
{
    time_t now;
    struct tm timeinfo;
    struct timeval tv_now;

    ESP_LOGW(__func__, "%02d:%02d %02d/%02d/%04d", iTimeinfo->tm_hour, iTimeinfo->tm_min, iTimeinfo->tm_mon + 1, iTimeinfo->tm_mday, iTimeinfo->tm_year);

    /* fill date_time */
    timeinfo.tm_year= iTimeinfo->tm_year - 1900;
    timeinfo.tm_mon = iTimeinfo->tm_mon - 1;
    timeinfo.tm_mday = iTimeinfo->tm_mday;
    timeinfo.tm_hour = iTimeinfo->tm_hour;
    timeinfo.tm_min = iTimeinfo->tm_min;
    timeinfo.tm_sec = 0;
    ESP_LOGI(TAG, "Write RTC Time (GMT) to system time: %s", asctime(&timeinfo));
    now = mktime(&timeinfo);
    tv_now.tv_sec = now;
    tv_now.tv_usec = 0;
    settimeofday(&tv_now, NULL);
    sntp_get_time();
}

void sntp_time_default(void)
{
    if (_InitSNTP || bValid) return;
    time_t now;
    struct tm timeinfo;
    struct timeval tv_now;

    /* fill date_time */
    timeinfo.tm_year= 2024 - 1900;
    timeinfo.tm_mon = 7 - 1;
    timeinfo.tm_mday = 4;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    now = mktime(&timeinfo);
    tv_now.tv_sec = now;
    tv_now.tv_usec = 0;
    settimeofday(&tv_now, NULL);
    bValid = true; 
}

bool sntp_get_valid(void)
{
    return bValid;
}