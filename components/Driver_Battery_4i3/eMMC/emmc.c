#include "emmc.h"
#include "ff.h"
#include "lvgl.h"

#define TAG "emmc"
#define DEBUG_LVGL_EMMC 0

static sdmmc_card_t *card = NULL;

static void write_data_test(void);
static esp_err_t storage_init_sdmmc(void);
static FILE *my_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode);
static lv_fs_res_t my_close_cb(lv_fs_drv_t *drv, void *file_p);
static lv_fs_res_t my_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br);
static lv_fs_res_t my_write_cb(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw);
static lv_fs_res_t my_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos);
static lv_fs_res_t my_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p);
static void lvgl_fs_test(void);

static bool Init__eMMC = false;
FILE* file_fs;
uint32_t time_start = 0;
uint32_t time_end = 0;
uint32_t time_total = 0;

static esp_err_t storage_init_sdmmc(void)
{
    esp_err_t ret = ESP_OK;
    bool host_init = false;
    sdmmc_card_t *sd_card;
    if (!Init__eMMC)
    {
        Init__eMMC = true;
        // Options for mounting the filesystem.
        // If format_if_mount_failed is set to true, eMMC will be partitioned and
        // formatted in case when mounting fails.
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = true,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024};
        const char mount_point[] = BASE_PATH;
        ESP_LOGI(TAG, "Initializing eMMC");

        // Use settings defined above to initialize eMMC and mount FAT filesystem.
        // Note: esp_vfs_fat_sdmmc_mount is all-in-one convenience functions.
        // Please check its source code and implement error recovery when developing
        // production applications.

        ESP_LOGI(TAG, "Using SDMMC peripheral");

        // By default, eMMC frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
        // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 52MHz for SDMMC)
        // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

        // This initializes the slot without card detect (CD) and write protect (WP) signals.
        // Other fields will be initialized to zero
        sdmmc_slot_config_t slot_config = {
            .cd = SDMMC_SLOT_NO_CD,
            .wp = SDMMC_SLOT_NO_WP,
        };
        // Set bus IOs
        slot_config.width = 4;
        slot_config.clk = CONFIG_EMMC_PIN_CLK;
        slot_config.cmd = CONFIG_EMMC_PIN_CMD;
        slot_config.d0 = CONFIG_EMMC_PIN_D0;
        slot_config.d1 = CONFIG_EMMC_PIN_D1;
        slot_config.d2 = CONFIG_EMMC_PIN_D2;
        slot_config.d3 = CONFIG_EMMC_PIN_D3;

        // Enable internal pullups on enabled pins. The internal pullups
        // are insufficient however, please make sure 10k external pullups are
        // connected on the bus. This is for debug / example purpose only.
        slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        host_init = true;
        ESP_LOGI(TAG, "Mounting filesystem");
        ret = esp_vfs_fat_sdmmc_mount(BASE_PATH, &host, &slot_config, &mount_config, &sd_card);

        if (ret != ESP_OK)
        {
            if (ret == ESP_FAIL)
            {
                ESP_LOGE(TAG, "Failed to mount filesystem. "
                              "If you want the eMMC to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
            }
            else
            {
                ESP_LOGE(TAG, "Failed to initialize the eMMC (%s). "
                              "Make sure eMMC lines have pull-up resistors in place.",
                         esp_err_to_name(ret));
            }
            goto clean;
        }
        ESP_LOGI(TAG, "Filesystem mounted");

        // Card has been initialized, print its properties
        sdmmc_card_print_info(stdout, sd_card);
        card = sd_card;
        // write_data_test();
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
    }
    return ret;
}

static void write_data_test(void)
{
    // Use POSIX and C standard library functions to work with files:

    // First create a file.
    // const char *file_hello = BASE_PATH "/hello.txt";

    // ESP_LOGI(TAG, "Opening file %s", file_hello);
    // FILE *f = fopen(file_hello, "w");
    // if (f == NULL)
    // {
    //     ESP_LOGE(TAG, "Failed to open file for writing");
    //     return;
    // }
    // fprintf(f, "Hello!\n");
    // fclose(f);
    // ESP_LOGI(TAG, "File written");

    // const char *file_foo = BASE_PATH "/foo.txt";

    // // Check if destination file exists before renaming
    // struct stat st;
    // if (stat(file_foo, &st) == 0)
    // {
    //     // Delete it if it exists
    //     unlink(file_foo);
    // }

    // // Rename original file
    // ESP_LOGI(TAG, "Renaming file %s to %s", file_hello, file_foo);
    // if (rename(file_hello, file_foo) != 0)
    // {
    //     ESP_LOGE(TAG, "Rename failed");
    //     return;
    // }

    // // Open renamed file for reading
    // ESP_LOGI(TAG, "Reading file %s", file_foo);
    // f = fopen(file_foo, "r");
    // if (f == NULL)
    // {
    //     ESP_LOGE(TAG, "Failed to open file for reading");
    //     return;
    // }

    // // Read a line from file
    // char line[64];
    // fgets(line, sizeof(line), f);
    // fclose(f);

    // // Strip newline
    // char *pos = strchr(line, '\n');
    // if (pos)
    // {
    //     *pos = '\0';
    // }
    // ESP_LOGI(TAG, "Read from file: '%s'", line);

    const char *file_hello = BASE_PATH "/hello.txt";
    int pos_p = 0;
    ESP_LOGI(TAG, "Opening file %s", file_hello);
    FILE *f = fopen(file_hello, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "Hello Hello Hello Hello Hello Hello Hello Hello Hello !\n");
    fclose(f);
    ESP_LOGI(TAG, "File written");

    // FILE *f = fopen("/data/Battery Health Check and Start Test Conditions.bin", "r");
    f = fopen(file_hello, "w");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for seeking");
        return;
    }
    pos_p = ftell((FILE *)f);
    ESP_LOGW(TAG, "pos_p = %d", pos_p);
    if(fseek(f, 10, SEEK_SET) != 0)
    {
        ESP_LOGI(TAG, "Seek done");
    }
    else
    {
        ESP_LOGE(TAG, "Seek fail");
    }
    pos_p = ftell((FILE *)f);
    ESP_LOGW(TAG, "pos_p = %d", pos_p);
    fclose(f);
}

void lvgl_register_fatfs_emmc(void)
{
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = CHAR_LETTER_DRV;
    fs_drv.cache_size = 1024*32;
    // fs_drv.file_size = sizeof(my_file_object);
    // fs_drv.rddir_size = sizeof(my_dir_object);
    // fs_drv.ready_cb = my_ready_cb;
    fs_drv.open_cb = my_open_cb;
    fs_drv.close_cb = my_close_cb;
    fs_drv.read_cb = my_read_cb;
    fs_drv.write_cb = my_write_cb;
    fs_drv.seek_cb = my_seek_cb;
    fs_drv.tell_cb = my_tell_cb;
    // fs_drv.trunc_cb = my_trunc_cb;
    // fs_drv.size_cb = my_size_cb;
    // fs_drv.rename_cb = my_rename_cb;

    // fs_drv.dir_open_cb = my_dir_open_cb;
    // fs_drv.dir_read_cb = my_dir_read_cb;
    // fs_drv.dir_close_cb = my_dir_close_cb;

    // fs_drv.free_space_cb = my_free_space_cb;

    // fs_drv.user_data = my_user_data;

    lv_fs_drv_register(&fs_drv);
    // write_data_test();
    // lvgl_fs_test();
}

/**
 * Open a file
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable
 * @param path path to the file beginning with the driver letter (e.g. S:/folder/file.txt)
 * @param mode read: FS_MODE_RD, write: FS_MODE_WR, both: FS_MODE_RD | FS_MODE_WR
 * @return LV_FS_RES_OK or any error from lv_fs_res_t enum
 */
static FILE *my_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    #if DEBUG_LVGL_EMMC || 1
    ESP_LOGI(TAG, "path (%s)\n", path);
    FILE* file_p = NULL;
    lv_fs_res_t res = LV_FS_RES_NOT_IMP;

    time_start = esp_log_timestamp();
    if (mode == LV_FS_MODE_WR)
    {
        /*Open a file for write*/
        file_p = fopen(path, "w");
        if (file_p == NULL)
        {
            ESP_LOGE(TAG, "Failed to open file for writing, path: %s", path);
            res = LV_FS_RES_NOT_IMP;
        }
        else
            res = LV_FS_RES_OK;
    }
    else if (mode == LV_FS_MODE_RD)
    {
        /*Open a file for read*/
        file_p = fopen(path, "r");
        if (file_p == NULL)
        {
            ESP_LOGE(TAG, "Failed to open file for reading, path: %s", path);
            res = LV_FS_RES_NOT_IMP;
        }
        else
            res = LV_FS_RES_OK;
    }
    else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD))
    {
        /*Open a file for read and write*/
        file_p = fopen(path, "w+");
        if (file_p == NULL)
        {
            ESP_LOGE(TAG, "Failed to open file for writing and reading, path: %s", path);
            res = LV_FS_RES_NOT_IMP;
        }
        else
            res = LV_FS_RES_OK;
    }
    file_fs = file_p;
    time_end = esp_log_timestamp();
    ESP_LOGW(TAG, "%s - Time handle: %"PRIu32, __func__, time_end - time_start);
    time_total += time_end - time_start;
    ESP_LOGW(TAG, "%s - Total time handle: %"PRIu32, __func__, time_total);
    return file_p;
    #else
    FILE* file_p = NULL;
    lv_fs_res_t res = LV_FS_RES_NOT_IMP;

    if (mode == LV_FS_MODE_WR)
    {
        /*Open a file for write*/
        file_p = fopen(path, "w");
        if (file_p == NULL)
        {
            res = LV_FS_RES_NOT_IMP;
        }
        else
            res = LV_FS_RES_OK;
    }
    else if (mode == LV_FS_MODE_RD)
    {
        /*Open a file for read*/
        file_p = fopen(path, "r");
        if (file_p == NULL)
        {
            res = LV_FS_RES_NOT_IMP;
        }
        else
            res = LV_FS_RES_OK;
    }
    else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD))
    {
        /*Open a file for read and write*/
        file_p = fopen(path, "w+");
        if (file_p == NULL)
        {
            res = LV_FS_RES_NOT_IMP;
        }
        else
            res = LV_FS_RES_OK;
    }
    file_fs = file_p;
    return file_p;
    #endif
    
}

/**
 * Close an opened file
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable. (opened with lv_ufs_open)
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv_fs_res_t enum
 */
static lv_fs_res_t my_close_cb(lv_fs_drv_t *drv, void *file_p)
{
    #if DEBUG_LVGL_EMMC
    ESP_LOGI(TAG, "my_close_cb");
    time_start = esp_log_timestamp();
    if (fclose((FILE *)file_p) == NULL)
    {
        time_end = esp_log_timestamp();
        ESP_LOGW(TAG, "%s - Time handle: %"PRIu32, __func__, time_end - time_start);
        time_total += time_end - time_start;
        ESP_LOGW(TAG, "%s - Total time handle: %"PRIu32, __func__, time_total);
        return LV_FS_RES_NOT_IMP;
    }
    else
    {
        time_end = esp_log_timestamp();
        ESP_LOGW(TAG, "%s - Time handle: %"PRIu32, __func__, time_end - time_start);
        time_total += time_end - time_start;
        ESP_LOGW(TAG, "%s - Total time handle: %"PRIu32, __func__, time_total);
        return LV_FS_RES_OK;
    }
    #else
    if (fclose((FILE *)file_p) == NULL)
        return LV_FS_RES_NOT_IMP;
    else
        return LV_FS_RES_OK;
    #endif
}

/**
 * 从打开的文件中读取数据
 * @param drv 指向此函数所属驱动程序的指针
 * @param file_p 指向 file_t 变量的指针。
 * @param buf 指向存储读取数据的内存块的指针
 * @param btr 要读取的字节数
 * @param br 实际读取字节数（Byte Read）
 * @return LV_FS_RES_OK: 没有错误，文件被读取
 * lv_fs_res_t 枚举中的任何错误
 */
static lv_fs_res_t my_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    #if DEBUG_LVGL_EMMC
    ESP_LOGI(TAG, "my_read_cb");
    time_start = esp_log_timestamp();
    *br = fread(buf, 1, btr, (FILE *)file_p);
    time_end = esp_log_timestamp();
    ESP_LOGW(TAG, "%s - Time handle: %"PRIu32, __func__, time_end - time_start);
    time_total += time_end - time_start;
    ESP_LOGW(TAG, "%s - Total time handle: %"PRIu32, __func__, time_total);
    ESP_LOGI(TAG, "btr: %"PRIu32, btr);
    ESP_LOGI(TAG, "br: %"PRIu32, *br);
    if (*br == 0)
    {
        ESP_LOGE(TAG, "f_read error");
        return LV_FS_RES_NOT_IMP;
    }
    else
        return LV_FS_RES_OK;
    #else
    *br = fread(buf, 1, btr, (FILE *)file_p);
    if (*br == 0)
    {
        return LV_FS_RES_NOT_IMP;
    }
    else
    {
        return LV_FS_RES_OK;
    }
    #endif    
}

/**
 * 写入文件
 * @param drv 指向此函数所属驱动程序的指针
 * @param file_p 指向 file_t 变量的指针
 * @param buf 指向要写入字节的缓冲区的指针
 * @param btr 要写入的字节数
 * @param br 实际写入的字节数（Bytes Written）。 如果未使用，则为 NULL。
 * @return LV_FS_RES_OK 或来自 lv_fs_res_t 枚举的任何错误
 */
static lv_fs_res_t my_write_cb(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw)
{
    #if DEBUG_LVGL_EMMC
    ESP_LOGI(TAG, "my_write_cb");
    /* Add your code here*/
    time_start = esp_log_timestamp();
    *bw = fwrite(buf, 1, btw, (FILE *)file_p);
    time_end = esp_log_timestamp();
    ESP_LOGW(TAG, "%s - Time handle: %"PRIu32, __func__, time_end - time_start);
    time_total += time_end - time_start;
    ESP_LOGW(TAG, "%s - Total time handle: %"PRIu32, __func__, time_total);
    if (*bw == 0)
    {
        ESP_LOGE(TAG, "f_write error");
        return LV_FS_RES_NOT_IMP;
    }
    else
        return LV_FS_RES_OK;
    #else
    /* Add your code here*/
    *bw = fwrite(buf, 1, btw, (FILE *)file_p);
    if (*bw == 0)
    {
        return LV_FS_RES_NOT_IMP;
    }
    else
        return LV_FS_RES_OK;
    #endif
    
}

/**
 * Set the read write pointer. Also expand the file size if necessary.
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable. (opened with lv_ufs_open )
 * @param pos the new position of read write pointer
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv_fs_res_t enum
 */
static lv_fs_res_t my_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos)
{
    #if DEBUG_LVGL_EMMC
    ESP_LOGI(TAG, "my_seek_cb");
    ESP_LOGI(TAG, "seek set pos: %"PRIu32, pos);
    int seek = 0;
    time_start = esp_log_timestamp();
    seek = fseek((FILE *)file_p, pos, SEEK_SET);
    int pos_p = ftell((FILE *)file_p);
    time_end = esp_log_timestamp();
    ESP_LOGW(TAG, "%s - Time handle: %"PRIu32, __func__, time_end - time_start);
    time_total += time_end - time_start;
    ESP_LOGW(TAG, "%s - Total time handle: %"PRIu32, __func__, time_total);
    ESP_LOGW(TAG, "pos_p = %d", pos_p);
    /* Add your code here*/
    if (pos_p != pos)
    {
        ESP_LOGE(TAG, "f_lseek error (%d)", seek);
        return LV_FS_RES_NOT_IMP;
    }
    else
    {
        return LV_FS_RES_OK;
    }
    #else
    int seek = 0;
    seek = fseek((FILE *)file_p, pos, SEEK_SET);
    int pos_p = ftell((FILE *)file_p);
    /* Add your code here*/
    if (pos_p != pos)
    {
        return LV_FS_RES_NOT_IMP;
    }
    else
    {
        return LV_FS_RES_OK;
    }
    #endif
}

/**
 * Give the position of the read write pointer
 * @param drv pointer to a driver where this function belongs
 * @param file_p pointer to a file_t variable.
 * @param pos_p pointer to to store the result
 * @return LV_FS_RES_OK: no error, the file is read
 *         any error from lv_fs_res_t enum
 */
static lv_fs_res_t my_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    lv_fs_res_t res = LV_FS_RES_UNKNOWN;

    #if DEBUG_LVGL_EMMC
    ESP_LOGI(TAG, "my_tell_cb");
    time_start = esp_log_timestamp();
    fseek((FILE *)file_fs, 0, SEEK_END);
    *pos_p = ftell((FILE *)file_fs);
    time_end = esp_log_timestamp();
    ESP_LOGW(TAG, "%s - Time handle: %"PRIu32, __func__, time_end - time_start);
    time_total += time_end - time_start;
    ESP_LOGW(TAG, "%s - Total time handle: %"PRIu32, __func__, time_total);
    ESP_LOGI(TAG, "pos_p = %"PRIu32, *pos_p);
    if (*pos_p >= 0)
    {
        res = LV_FS_RES_OK;
    }
    return res;
    #else
    fseek((FILE *)file_fs, 0, SEEK_END);
    *pos_p = ftell((FILE *)file_fs);

    if (*pos_p >= 0)
    {
        res = LV_FS_RES_OK;
    }
    return res;
    #endif
}

static void lvgl_fs_test(void)
{
    char rbuf[30] = {0};
    uint32_t rsize = 0;
    lv_fs_file_t fd;
    lv_fs_res_t res;
    // char file_path[256];
    // sprintf(file_path, "%c%s%s", CHAR_LETTER_DRV, BASE_PATH, "/foo.txt");
    // ESP_LOGI(TAG, "open %s", file_path);
    // res = lv_fs_open(&fd, file_path, LV_FS_MODE_WR);
    // if (res != LV_FS_RES_OK)
    // {
    //     ESP_LOGI(TAG, "open %s ERROR\n", file_path);
    //     return;
    // }

    // ESP_LOGI(TAG, "write %s", __func__);
    // res = lv_fs_write(&fd, "Hello", 5, &rsize);
    // if (res != LV_FS_RES_OK)
    // {
    //     ESP_LOGI(TAG, "write ERROR\n");
    //     return;
    // }

    // lv_fs_close(&fd);
    // ESP_LOGI(TAG, "close %s", __func__);

    // res = lv_fs_open(&fd, file_path, LV_FS_MODE_RD);
    // if (res != LV_FS_RES_OK)
    // {
    //     ESP_LOGI(TAG, "open %s ERROR\n", file_path);
    //     return;
    // }

    // ESP_LOGI(TAG, "read %s", __func__);
    // res = lv_fs_read(&fd, rbuf, 100, &rsize);
    // if (res != LV_FS_RES_OK)
    // {
    //     ESP_LOGI(TAG, "read ERROR\n");
    //     return;
    // }

    // ESP_LOGI(TAG, "READ(%" PRIu32 "): %s", rsize, rbuf);

    // lv_fs_close(&fd);
    // ESP_LOGI(TAG, "close %s", __func__);

    res = lv_fs_open(&fd, "//data/Battery Health Check and Start Test Conditions.bin", LV_FS_MODE_WR);
    if (lv_fs_seek(&fd, 20, LV_FS_SEEK_SET) == LV_FS_RES_OK)
    {
        ESP_LOGI(TAG, "Done lv_fs_seek");
    }
    lv_fs_close(&fd);
}