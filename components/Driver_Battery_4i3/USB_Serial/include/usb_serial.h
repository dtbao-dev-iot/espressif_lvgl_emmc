#ifndef _USB_SERIAL_H_
#define _USB_SERIAL_H_

#ifdef __cplusplus
extern "C"
{
#endif

// #include "upgrade.h"
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

typedef struct _structHidOutData
{
    unsigned int iReportId;
    unsigned int iCommand;
    unsigned int iAddress;
    unsigned int iSize;
    unsigned int iFlashBuffer[128];
} structHidOutData;   /*Data from Host to device*/

typedef struct _structHidInData
{
    unsigned int   iInSize;             /*Buffer size to send to PC*/
    unsigned char* p_bDataBuffer;       /*Buffer to send to PC*/
} structHidInData;           /*Data from Device to host*/

typedef struct _structUSBData
{
    structHidOutData strtHidOut;
    structHidInData  strtHidIn;
} structUSBData;

extern structUSBData strtUSBData;
void init_usb_serial(void);
void USBD_CtlSendData(void);
bool get_usb_vbus(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif