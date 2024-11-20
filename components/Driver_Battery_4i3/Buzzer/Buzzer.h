
#ifndef _BUZZER_H_
#define _BUZZER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PIN_BUZZER                  12
// SiT modified BEEP speak for 1 second with frequency = 5KHz
#define BUZZER_TIMER                LEDC_TIMER_0
#define BUZZER_MODE                 LEDC_LOW_SPEED_MODE
#define BUZZER_CHANNEL              LEDC_CHANNEL_0
#define BUZZER_DUTY_RES             LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define BUZZER_DUTY                 (4095) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define BUZZER_FREQUENCY            (5000) // Frequency in Hertz. Set frequency at 5 kHz

typedef enum _enumTone
{
  TONE_KEY      =   1,
  TONE_POWER    =   2,
  TONE_DONE     =   4,
  TONE_ERASE    =   4,
  TONE_ERROR    =   8,
} enumTone;

void init_buzzer(void);
void BuzzerControl(uint8_t eToneType);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif