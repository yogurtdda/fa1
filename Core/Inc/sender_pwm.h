#ifndef SENDER_PWM_H
#define SENDER_PWM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t ready, requested_hz[3], actual_millihz[3], changes;
} SenderPwmDiagnostics;
extern volatile SenderPwmDiagnostics pwm_diag;
/* Start D1=PA15/TIM2_CH1, D2=PH6/TIM12_CH1, D3=PB4/TIM3_CH1 at 1 kHz. */
int SenderPwm_Init(void);
/* Apply a confirmed GUI frequency to one independent, 50% duty PWM output. */
int SenderPwm_SetFrequency(unsigned channel, uint32_t hz);
#ifdef __cplusplus
}
#endif
#endif
