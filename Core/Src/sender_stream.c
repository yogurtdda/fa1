#include "sender_stream.h"
#include "sender_protocol.h"
#include "sender_pwm.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include "lwip/api.h"
#include "lwip/etharp.h"
#include "lwip/tcpip.h"
#include <string.h>

extern ADC_HandleTypeDef hadc3;
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim4;
extern struct netif gnetif;
volatile SenderStreamDiagnostics stream_diag;

#define DMA_RAM __attribute__((section(".dma_buffers"), aligned(32)))
#define STREAM_RAM __attribute__((section(".stream_buffers"), aligned(32)))
#define RAW_SAMPLES 2048U
#define RING_BLOCKS 8U

/* ADC's 8 KiB circular buffer: each half contains 1024 [IN0, IN8] pairs. */
static uint16_t adc_raw[RAW_SAMPLES * 2U] DMA_RAM;
/* Three additional 2 KiB byte buffers sample the actual output pads via DMA2. */
static uint8_t gpio_raw[3][RAW_SAMPLES] DMA_RAM;
static DMA_HandleTypeDef gpio_dma[3];

typedef struct { uint32_t half, sequence; } CompletedHalf;
typedef struct {
    uint32_t sequence, session, pwm_hz[3];
    uint32_t records[SENDER_BLOCK_SAMPLES];
} SampleBlock;
static SampleBlock block_ring[RING_BLOCKS] STREAM_RAM;
static StaticTask_t capture_tcb, network_tcb;
static StackType_t capture_stack[1024] STREAM_RAM;
static StackType_t network_stack[1536] STREAM_RAM;
static StaticQueue_t completed_queue_control;
static uint8_t completed_queue_storage[4U * sizeof(CompletedHalf)];
static QueueHandle_t completed_queue;
static TaskHandle_t network_task;
static uint32_t last_irq_cycles;

/* Configure one timer-requested GPIO byte stream without enabling a 500 kHz ISR. */
static void InitGpioDma(unsigned channel, DMA_Stream_TypeDef *instance, uint32_t address)
{
    DMA_HandleTypeDef *dma = &gpio_dma[channel];
    dma->Instance = instance;
    dma->Init.Channel = DMA_CHANNEL_7;
    dma->Init.Direction = DMA_PERIPH_TO_MEMORY;
    dma->Init.PeriphInc = DMA_PINC_DISABLE;
    dma->Init.MemInc = DMA_MINC_ENABLE;
    dma->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    dma->Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    dma->Init.Mode = DMA_CIRCULAR;
    dma->Init.Priority = DMA_PRIORITY_HIGH;
    dma->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(dma) != HAL_OK ||
        HAL_DMA_Start(dma, address, (uint32_t)gpio_raw[channel], RAW_SAMPLES) != HAL_OK)
        Error_Handler();
}

/* TIM4 triggers ADC and resets TIM8, whose update requests read all three GPIOs. */
static void StartSamplingHardware(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    /* Cortex-M7's trace unit is locked after reset on this board. */
    *(volatile uint32_t *)0xE0001FB0UL = 0xC5ACCE55U;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    /* Stop the trigger and PWM timers whenever SWD halts the CPU. Otherwise
     * repeated conversions while DMA IRQs are halted can leave ADC in overrun. */
    DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM8_STOP;
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM4_STOP | DBGMCU_APB1_FZ_DBG_TIM2_STOP |
                     DBGMCU_APB1_FZ_DBG_TIM3_STOP | DBGMCU_APB1_FZ_DBG_TIM12_STOP;
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t divisor = HAL_RCC_GetHCLKFreq() / pclk;
    uint32_t timer_hz = (RCC->DCKCFGR1 & RCC_DCKCFGR1_TIMPRE) ?
        (divisor <= 4U ? HAL_RCC_GetHCLKFreq() : pclk * 4U) :
        (divisor == 1U ? pclk : pclk * 2U);
    if (timer_hz % SENDER_SAMPLE_HZ != 0U) Error_Handler();
    TIM4->CR1 = 0U; TIM4->CR2 = 0U; TIM4->DIER = 0U;
    TIM4->PSC = timer_hz / 10000000U - 1U;
    TIM4->ARR = 10000000U / SENDER_SAMPLE_HZ - 1U;
    TIM4->CNT = 0U; TIM4->EGR = TIM_EGR_UG; TIM4->SR = 0U;
    TIM4->CR2 = TIM_TRGO_UPDATE;
    htim4.Init.Prescaler = TIM4->PSC; htim4.Init.Period = TIM4->ARR;
    TIM8->CR1 = 0U; TIM8->CR2 = 0U; TIM8->DIER = 0U;
    TIM8->SMCR = 0U; TIM8->PSC = 0U; TIM8->ARR = 65535U;
    TIM8->RCR = 0U; TIM8->CNT = 0U; TIM8->EGR = TIM_EGR_UG; TIM8->SR = 0U;
    /* Keep unused compare matches away from CNT=0, where reset/update occurs. */
    TIM8->CCR1 = 65535U; TIM8->CCR2 = 65535U; TIM8->CCR3 = 65535U;
    /* RM0385 table 149: TIM8 ITR2 is TIM4 TRGO. Reset mode generates one
     * update per master trigger; ARR cannot overflow within the 2 us period. */
    TIM8->SMCR = TIM_TS_ITR2 | TIM_SLAVEMODE_RESET;
    TIM8->CR2 = TIM_CR2_CCDS;
    htim8.Init.Prescaler = 0U; htim8.Init.Period = TIM8->ARR;

    hadc3.Init.DMAContinuousRequests = ENABLE;
    /* ES0290 section 2.2.1 workaround for TIM4 TRGO -> ADC with APB2 divider. */
    __HAL_RCC_DAC_CLK_ENABLE();
    hadc3.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T4_TRGO;
    hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc3) != HAL_OK) Error_Handler();
    /* ADC's FIFO decouples the short data-register deadline from SRAM bus
     * contention with GPIO DMA, CPU packing and the active display. */
    hadc3.DMA_Handle->Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hadc3.DMA_Handle->Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL;
    hadc3.DMA_Handle->Init.MemBurst = DMA_MBURST_INC4;
    hadc3.DMA_Handle->Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(hadc3.DMA_Handle) != HAL_OK) Error_Handler();
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5U, 0U);
    HAL_NVIC_SetPriority(ADC_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(ADC_IRQn);

    /* RM0385 table 26: TIM8 CH1/2/3 -> DMA2 streams 2/3/4, channel 7.
     * PA15 is in IDR's upper byte, PH6/PB4 are in its lower byte. */
    InitGpioDma(0U, DMA2_Stream2, (uint32_t)&GPIOA->IDR + 1U);
    InitGpioDma(1U, DMA2_Stream3, (uint32_t)&GPIOH->IDR);
    InitGpioDma(2U, DMA2_Stream4, (uint32_t)&GPIOB->IDR);
    if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc_raw, RAW_SAMPLES * 2U) != HAL_OK)
        Error_Handler();
    /* CCDS routes all three CC DMA requests to update, without output pins. */
    TIM8->DIER = TIM_DIER_CC1DE | TIM_DIER_CC2DE | TIM_DIER_CC3DE;
    stream_diag.sample_hz = SENDER_SAMPLE_HZ;
    stream_diag.running = 1U;
    stream_diag.started_tick = HAL_GetTick();
    last_irq_cycles = DWT->CYCCNT;
    __DSB();
    TIM8->CR1 = TIM_CR1_CEN;
    TIM4->CR1 = TIM_CR1_CEN;
}

/* ISR only publishes the completed half; copying, packing and UDP run in tasks. */
static void PublishHalf(unsigned half)
{
    if (!stream_diag.running) return;
    uint32_t now = DWT->CYCCNT;
    stream_diag.dma_last_interval_cycles = now - last_irq_cycles;
    last_irq_cycles = now;
    CompletedHalf event = {half, stream_diag.dma_blocks++};
    stream_diag.adc_ndtr = hadc3.DMA_Handle->Instance->NDTR;
    for (unsigned i = 0; i < 3U; ++i) {
        stream_diag.gpio_ndtr[i] = gpio_dma[i].Instance->NDTR;
        if (__HAL_DMA_GET_FLAG(&gpio_dma[i], __HAL_DMA_GET_TE_FLAG_INDEX(&gpio_dma[i])))
            ++stream_diag.gpio_dma_errors;
    }
    unsigned expected = half == 0U ? SENDER_BLOCK_SAMPLES : 0U;
    for (unsigned i = 0; i < 3U; ++i) {
        unsigned position = (RAW_SAMPLES - stream_diag.gpio_ndtr[i]) % RAW_SAMPLES;
        if ((position + RAW_SAMPLES - expected) % RAW_SAMPLES > 8U) {
            ++stream_diag.gpio_sync_errors;
            HAL_ADC_ErrorCallback(&hadc3);
            return;
        }
    }
    BaseType_t wake = pdFALSE;
    if (xQueueSendFromISR(completed_queue, &event, &wake) != pdPASS)
        ++stream_diag.dma_queue_drops;
    portYIELD_FROM_ISR(wake);
}

/* ADC DMA's half/full callbacks identify the safe half of all four DMA streams. */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *adc)
{
    if (adc->Instance == ADC3) { ++stream_diag.half_callbacks; PublishHalf(0U); }
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc)
{
    if (adc->Instance == ADC3) { ++stream_diag.full_callbacks; PublishHalf(1U); }
}

/* Record a hardware ADC failure instead of silently presenting stale samples. */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *adc)
{
    if (adc->Instance == ADC3) {
        ++stream_diag.adc_errors;
        stream_diag.last_adc_error = adc->ErrorCode;
        stream_diag.error_tick = HAL_GetTick();
        stream_diag.running = 0U;
        TIM8->CR1 &= ~TIM_CR1_CEN;
        TIM4->CR1 &= ~TIM_CR1_CEN;
        CompletedHalf recovery = {2U, 0U};
        BaseType_t wake = pdFALSE;
        xQueueSendFromISR(completed_queue, &recovery, &wake);
        portYIELD_FROM_ISR(wake);
    }
}

/* Re-align all four DMA rings after an ADC fault; a new session marks the gap. */
static void RecoverSampling(void)
{
    HAL_ADC_Stop_DMA(&hadc3);
    for (unsigned i = 0; i < 3U; ++i) HAL_DMA_Abort(&gpio_dma[i]);
    xQueueReset(completed_queue);
    ++stream_diag.session;
    ++stream_diag.restarts;
    StartSamplingHardware();
}

/* Pack only a completed, still-owned DMA half into the bounded network ring. */
static void CaptureTask(void *argument)
{
    (void)argument;
    CompletedHalf event;
    for (;;) {
        xQueueReceive(completed_queue, &event, portMAX_DELAY);
        if (!stream_diag.running) { RecoverSampling(); continue; }
        if (stream_diag.dma_blocks != event.sequence + 1U) {
            ++stream_diag.overwritten_blocks; continue;
        }
        if (stream_diag.producer - stream_diag.consumer >= RING_BLOCKS) {
            ++stream_diag.ring_drops; continue;
        }
        uint32_t start = DWT->CYCCNT;
        SampleBlock *block = &block_ring[stream_diag.producer % RING_BLOCKS];
        unsigned offset = event.half * SENDER_BLOCK_SAMPLES;
        block->sequence = event.sequence;
        block->session = stream_diag.session;
        for (unsigned i = 0; i < 3U; ++i) block->pwm_hz[i] = pwm_diag.requested_hz[i];
        for (unsigned i = 0; i < SENDER_BLOCK_SAMPLES; ++i) {
            unsigned raw = offset + i;
            unsigned digital = ((gpio_raw[0][raw] >> 7U) & 1U) |
                (((gpio_raw[1][raw] >> 6U) & 1U) << 1U) |
                (((gpio_raw[2][raw] >> 4U) & 1U) << 2U);
            block->records[i] = SenderProtocol_Record(adc_raw[raw*2U], adc_raw[raw*2U+1U], digital);
        }
        __DMB();
        if (stream_diag.dma_blocks != event.sequence + 1U) {
            ++stream_diag.overwritten_blocks; continue;
        }
        uint32_t cycles = DWT->CYCCNT - start;
        if (cycles > stream_diag.pack_max_cycles) stream_diag.pack_max_cycles = cycles;
        stream_diag.last_block = event.sequence;
        stream_diag.last_record = block->records[SENDER_BLOCK_SAMPLES-1U];
        stream_diag.last_digital = stream_diag.last_record >> 24U;
        stream_diag.packed_samples += SENDER_BLOCK_SAMPLES;
        ++stream_diag.packed_blocks;
        ++stream_diag.producer;
        xTaskNotifyGive(network_task);
    }
}

/* Send to 192.168.10.2:5000; link/ARP waits discard old blocks, never halt ADC. */
static void NetworkTask(void *argument)
{
    (void)argument;
    ip_addr_t destination;
    IP_ADDR4(&destination, 192,168,10,2);
    struct netconn *udp = netconn_new(NETCONN_UDP);
    if (udp == NULL || netconn_bind(udp, IP_ADDR_ANY, 5001U) != ERR_OK) Error_Handler();
    stream_diag.netconn_ready = 1U;
    uint32_t last_arp = HAL_GetTick() - 1000U;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (stream_diag.consumer != stream_diag.producer) {
            SampleBlock *block = &block_ring[stream_diag.consumer % RING_BLOCKS];
            int ready;
            LOCK_TCPIP_CORE();
            stream_diag.link_up = netif_is_link_up(&gnetif) && netif_is_up(&gnetif);
            struct eth_addr *mac; const ip4_addr_t *ip;
            ready = stream_diag.link_up &&
                etharp_find_addr(&gnetif, ip_2_ip4(&destination), &mac, &ip) >= 0;
            if (stream_diag.link_up && !ready && HAL_GetTick() - last_arp >= 1000U) {
                etharp_request(&gnetif, ip_2_ip4(&destination)); last_arp = HAL_GetTick();
            }
            UNLOCK_TCPIP_CORE();
            if (!stream_diag.link_up) ++stream_diag.link_dropped_blocks;
            else if (!ready) ++stream_diag.arp_wait_blocks;
            else {
                for (unsigned fragment = 0; fragment < 4U; ++fragment) {
                    struct netbuf *buffer = netbuf_new();
                    uint8_t *payload = buffer ? netbuf_alloc(buffer, SENDER_PACKET_BYTES) : NULL;
                    if (!payload) {
                        if (buffer) netbuf_delete(buffer);
                        ++stream_diag.udp_errors; continue;
                    }
                    SenderProtocol_Header(payload, block->session, block->sequence,
                                          fragment, block->pwm_hz);
                    /* STM32F746 is little endian; records are already LE32. */
                    memcpy(payload + SENDER_HEADER_BYTES,
                           block->records + fragment * SENDER_PACKET_SAMPLES,
                           SENDER_PACKET_SAMPLES * 4U);
                    if (netconn_sendto(udp, buffer, &destination, 5000U) == ERR_OK) {
                        ++stream_diag.udp_ok;
                        stream_diag.udp_payload_bytes += SENDER_PACKET_BYTES;
                    } else ++stream_diag.udp_errors;
                    netbuf_delete(buffer);
                }
            }
            __DMB();
            ++stream_diag.consumer;
        }
    }
}

void SenderStream_Start(void)
{
    completed_queue = xQueueCreateStatic(4U, sizeof(CompletedHalf), completed_queue_storage,
                                         &completed_queue_control);
    if (!completed_queue) Error_Handler();
    network_task = xTaskCreateStatic(NetworkTask, "SampleUDP", 1536U, NULL,
                    tskIDLE_PRIORITY + 2U, network_stack, &network_tcb);
    TaskHandle_t capture = xTaskCreateStatic(CaptureTask, "SamplePack", 1024U, NULL,
                    tskIDLE_PRIORITY + 5U, capture_stack, &capture_tcb);
    if (!network_task || !capture) Error_Handler();
    stream_diag.session = HAL_GetUIDw0() ^ HAL_GetTick() ^ DWT->CYCCNT;
    StartSamplingHardware();
}
