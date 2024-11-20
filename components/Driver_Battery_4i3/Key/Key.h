
#ifndef _KEY_H_
#define _KEY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum 
{
    KEY_NONE = 0,
    KEY_HOME,
    KEY_BACK,
    KEY_ENTER,
    KEY_UP,
    KEY_DN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_POWER,
}status_button_t;

void init_key(void);
status_button_t Proc_GetStatusKey(void);
bool Proc_GetStatusKeyOnOff(void);
status_button_t Get_KeyCur(void);
void Enable_AllKey(void);
void Disable_AllKey(void);
void TurnON_Tool(void);
void TurnOFF_Tool(void);
void Wait_ConfirmToolOn(void);
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif