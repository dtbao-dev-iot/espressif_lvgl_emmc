#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

// Pin eMMC Battery Tester 4inch3
#define CONFIG_EMMC_PIN_CLK     44
#define CONFIG_EMMC_PIN_CMD     43
#define CONFIG_EMMC_PIN_D0      47
#define CONFIG_EMMC_PIN_D1      21
#define CONFIG_EMMC_PIN_D2      14
#define CONFIG_EMMC_PIN_D3      13

#define BASE_PATH "/data" // base path to mount the partition
#define CHAR_LETTER_DRV '/' // base path to mount the partition

void lvgl_register_fatfs_emmc(void);