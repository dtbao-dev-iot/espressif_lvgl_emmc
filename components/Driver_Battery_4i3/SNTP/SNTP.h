#ifndef __SNTP_H__
#define __SNTP_H__

#ifdef __cplusplus
extern "C" {
#endif
#warning Khai báo SNTP 
#include "stdbool.h"
#include "stdlib.h"
#include "string.h"

void initialize_sntp(void);
struct tm sntp_get_time(void);
void sntp_set_time(struct tm* iTimeinfo);
bool sntp_get_valid(void);
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif