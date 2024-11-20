#include "test_benchmark_fatfs.h"

static char base_path[20];
static char path_file[256];
static uint8_t data_write[SIZE_BUF];

static void fatfs_build_path(char* nameFile)
{
    sprintf(path_file, "%s/%s", base_path, nameFile);
    ESP_LOGI(__func__, "Name file: %s", nameFile);
}

static void fatfs_write_file(size_t size)
{
    for (size_t i = 0; i < SIZE_BUF; i++)
    {
        data_write[i] = 0xFA;
    }
    
    FILE* file_p = NULL;
    int index_write = size/SIZE_BUF;
    float start_time = esp_log_timestamp();
    float end_time = 0;
    file_p = fopen(path_file, "w");
    if (file_p == NULL)
    {
        ESP_LOGE(__func__, "Fail open file");
        return;
    }

    for (int ind = 0; ind < index_write; ind++)
    {
        fwrite(data_write, 1, SIZE_BUF, (FILE *)file_p);
    }

    if (index_write*SIZE_BUF < size)
    {
        fwrite(data_write, 1, (size - index_write*SIZE_BUF), (FILE *)file_p);
    }

    fclose((FILE *)file_p);
    end_time = esp_log_timestamp();
    ESP_LOGI(__func__, "Write time: %0.2f - Speed write: %0.2fMB/s", (float)(end_time - start_time)/1000,
    (float) (size / ((end_time - start_time) / 1000) / MEM_SIZE_1MB));
}

static void fatfs_read_file(size_t size)
{
    FILE* file_p = NULL;
    int index_read = size/SIZE_BUF;
    float start_time = esp_log_timestamp();
    float end_time = 0;
    file_p = fopen(path_file, "r");
    if (file_p == NULL)
    {
        ESP_LOGE(__func__, "Fail open file");
        return;
    }

    for (int ind = 0; ind < index_read; ind++)
    {
        fread(data_write, 1, SIZE_BUF, (FILE *)file_p);
    }

    if (index_read*SIZE_BUF < size)
    {
        fread(data_write, 1, (size - index_read*SIZE_BUF), (FILE *)file_p);
    }

    fclose((FILE *)file_p);
    end_time = esp_log_timestamp();
    ESP_LOGW(__func__, "Read time: %0.2f - Speed read: %0.2fMB/s", (float)(end_time - start_time)/1000,
    (float) (size / ((end_time - start_time) / 1000) / MEM_SIZE_1MB));
}

static void benchmark_read_write_test(char* nameFile, size_t sizeFile)
{
    fatfs_build_path(nameFile);
    // Check if destination file exists before renaming
    struct stat st;
    if (stat(path_file, &st) == 0) {
        // Delete it if it exists
        unlink(path_file);
    }
    
    fatfs_write_file(sizeFile);
    fatfs_read_file(sizeFile);

    if (stat(path_file, &st) == 0) {
        // Delete it if it exists
        unlink(path_file);
    }
}

void set_base_path(char* _base_path)
{
    sprintf(base_path, "%s", _base_path);
}

void test_benchmark_emmc(void)
{
    benchmark_read_write_test(NAME_FILE_1MB, MEM_SIZE_1MB);
    benchmark_read_write_test(NAME_FILE_10MB, MEM_SIZE_10MB);
    // benchmark_read_write_test(NAME_FILE_50MB, MEM_SIZE_50MB);
    // benchmark_read_write_test(NAME_FILE_100MB, MEM_SIZE_100MB);
    // benchmark_read_write_test(NAME_FILE_200MB, MEM_SIZE_200MB);
    // benchmark_read_write_test(NAME_FILE_500MB, MEM_SIZE_500MB);
    // benchmark_read_write_test(NAME_FILE_1GB, MEM_SIZE_1GB);
}