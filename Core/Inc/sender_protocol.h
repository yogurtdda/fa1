#ifndef SENDER_PROTOCOL_H
#define SENDER_PROTOCOL_H
#include <stdint.h>
#define SENDER_SAMPLE_HZ 500000U
#define SENDER_BLOCK_SAMPLES 1024U
#define SENDER_PACKET_SAMPLES 256U
#define SENDER_HEADER_BYTES 48U
#define SENDER_PACKET_BYTES (SENDER_HEADER_BYTES + SENDER_PACKET_SAMPLES * 4U)
/* Two raw 12-bit ADC values plus three measured GPIO levels occupy 27 bits. */
static inline uint32_t SenderProtocol_Record(uint16_t in0, uint16_t in8, unsigned digital)
{
    return (in0 & 0xFFFU) | ((uint32_t)(in8 & 0xFFFU) << 12U) |
           ((uint32_t)(digital & 7U) << 24U);
}
/* Serialize one MTU-safe packet explicitly in little endian; no C struct goes on wire. */
void SenderProtocol_Header(uint8_t *out, uint32_t session, uint32_t block,
                           unsigned fragment, uint32_t const pwm_hz[3]);
#endif
