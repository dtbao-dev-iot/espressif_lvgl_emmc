#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "stdint.h"

#define MEM_SIZE_1KB 1024
#define MEM_SIZE_1MB (1*MEM_SIZE_1KB*MEM_SIZE_1KB)
#define MEM_SIZE_10MB (10*MEM_SIZE_1MB)
#define MEM_SIZE_50MB (50*MEM_SIZE_1MB)
#define MEM_SIZE_100MB (100*MEM_SIZE_1MB)
#define MEM_SIZE_200MB (200*MEM_SIZE_1MB)
#define MEM_SIZE_500MB (500*MEM_SIZE_1MB)
#define MEM_SIZE_1GB (1*MEM_SIZE_1KB*MEM_SIZE_1KB*MEM_SIZE_1KB)
#define SIZE_BUF 1024
#define NAME_FILE_1MB "Benchmark_1MB.txt"
#define NAME_FILE_10MB "Benchmark_10MB.txt"
#define NAME_FILE_50MB "Benchmark_50MB.txt"
#define NAME_FILE_100MB "Benchmark_100MB.txt"
#define NAME_FILE_200MB "Benchmark_200MB.txt"
#define NAME_FILE_500MB "Benchmark_500MB.txt"
#define NAME_FILE_1GB "Benchmark_1GB.txt"

void set_base_path(char* _base_path);
void test_benchmark_emmc(void);