#include "sender_pwm.h"
#include "main.h"

extern TIM_HandleTypeDef htim2, htim3, htim12;
volatile SenderPwmDiagnostics pwm_diag;
static TIM_HandleTypeDef *const timers[3] = {&htim2, &htim12, &htim3};

/* Resolve the APB1 timer clock, including the STM32F7 TIMPRE option. */
static uint32_t TimerClock(void)
{
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t divisor = HAL_RCC_GetHCLKFreq() / pclk;
    if (RCC->DCKCFGR1 & RCC_DCKCFGR1_TIMPRE)
        return divisor <= 4U ? HAL_RCC_GetHCLKFreq() : pclk * 4U;
    return divisor == 1U ? pclk : pclk * 2U;
}

int SenderPwm_SetFrequency(unsigned channel, uint32_t hz)
{
    if (channel >= 3U || hz < 100U || hz > 10000U) return 0;
    TIM_TypeDef *tim = timers[channel]->Instance;
    if (tim == NULL) return 0;
    uint32_t divider = TimerClock() / 1000000U;
    if (divider == 0U) return 0;
    uint32_t counter_hz = TimerClock() / divider;
    uint32_t period = (counter_hz + hz / 2U) / hz;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t enabled = tim->CR1 & TIM_CR1_CEN;
    tim->CR1 &= ~TIM_CR1_CEN;
    tim->PSC = divider - 1U;
    tim->ARR = period - 1U;
    tim->CCR1 = period / 2U;
    tim->CNT = 0U;
    tim->EGR = TIM_EGR_UG;
    tim->SR = 0U;
    tim->CR1 |= enabled;
    pwm_diag.requested_hz[channel] = hz;
    pwm_diag.actual_millihz[channel] = (uint32_t)(((uint64_t)counter_hz * 1000U) / period);
    ++pwm_diag.changes;
    __set_PRIMASK(primask);
    return 1;
}

int SenderPwm_Init(void)
{
    for (unsigned i = 0; i < 3U; ++i) {
        if (!SenderPwm_SetFrequency(i, 1000U)) return 0;
        if (HAL_TIM_PWM_Start(timers[i], TIM_CHANNEL_1) != HAL_OK) return 0;
    }
    pwm_diag.ready = 1U;
    return 1;
}
