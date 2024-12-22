#include "ff.h"
#include "esp_log.h"
#include "esp_err.h"
#include "dirent.h"
#include <errno.h>
#include <string.h>

void ShowFileList(char *path)
{
    struct dirent *d;
    DIR *dh = opendir(path);
    if (!dh) {
        if (errno == ENOENT) {
            //If the directory is not found
            ESP_LOGE(__func__, "Directory doesn't exist %s", path);
        } else {
            //If the directory is not readable then throw error and exit
            ESP_LOGE(__func__, "Unable to read directory %s", path);
        }
        return;
    }
    ESP_LOGI("Files in", "%s", path);
    //While the next entry is not readable we will print directory files
    while ((d = readdir(dh)) != NULL) 
    {
        ESP_LOGI("File", "%s", d->d_name);
    }
}

FRESULT f_deltree (TCHAR *path   /* Pointer to the directory path */)
{
    FRESULT res;
    FILINFO fno;
    DIR dir;
    int i;
    char *fn;   /* This function is assuming non-Unicode cfg. */
#if _USE_LFN
    static char lfn[_MAX_LFN + 1];
    fno.lfname = lfn;
    fno.lfsize = sizeof(lfn);
#endif

    res = f_opendir(&dir, path);                       /* Open the directory */
    ESP_LOGW(__func__, "res %d", res);
    ESP_LOGW(__func__, "f_opendir %s", path);
    if (res == FR_OK)
    {
        i = strlen(path);
        for (;;)
        {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            ESP_LOGW(__func__, "f_readdir");
            ESP_LOGW(__func__, "res %d", res);
            if ((res != FR_OK) || (fno.fname[0] == 0))
            {
                break;  /* Break on error or end of dir */
            }
            if (fno.fname[0] == '.')
            {
                continue;             /* Ignore dot entry */
            }
#if _USE_LFN
            fn = *fno.lfname ? fno.lfname : fno.fname;
#else
            fn = fno.fname;
#endif
            path[i] = '/';
            strcpy(&path[i + 1], fn);
            if (fno.fattrib & AM_DIR)
            {                    /* It is a directory */
                res = f_deltree(path);
            }
            else
            {                                       /* It is a file. */
                res = f_unlink(path);
            }
            if (res != FR_OK)
            {
                break;
            }
            path[i] = 0;
        }
        if (fno.fname[0] == 0)
        {
            res = f_unlink(path);
        }
    }

    return res;
}