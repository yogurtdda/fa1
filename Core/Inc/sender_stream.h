#ifndef SENDER_STREAM_H
#define SENDER_STREAM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t running, sample_hz, session;
    uint32_t dma_blocks, dma_queue_drops, overwritten_blocks, ring_drops;
    uint32_t packed_blocks, packed_samples, adc_errors, gpio_dma_errors;
    uint32_t pack_max_cycles, dma_last_interval_cycles;
    uint32_t link_up, link_dropped_blocks, arp_wait_blocks;
    uint32_t udp_ok, udp_errors, udp_payload_bytes, netconn_ready;
    uint32_t producer, consumer, last_block, last_record, last_digital;
    uint32_t gpio_ndtr[3], adc_ndtr;
    uint32_t half_callbacks, full_callbacks, started_tick, error_tick;
    uint32_t restarts;
    uint32_t gpio_sync_errors, last_adc_error;
} SenderStreamDiagnostics;
extern volatile SenderStreamDiagnostics stream_diag;
/* After MX_LWIP_Init: start synchronous ADC/GPIO DMA and the pack/send tasks. */
void SenderStream_Start(void);
#ifdef __cplusplus
}
#endif
#endif
