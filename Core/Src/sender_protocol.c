#include "sender_protocol.h"
/* Encode an integer independently of host structure padding and byte order. */
static void Put32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4U; ++i) p[i] = (uint8_t)(value >> (i * 8U));
}
void SenderProtocol_Header(uint8_t *p, uint32_t session, uint32_t block,
                           unsigned fragment, uint32_t const pwm_hz[3])
{
    p[0]='T'; p[1]='X'; p[2]='S'; p[3]='1';
    p[4]=1U; p[5]=SENDER_HEADER_BYTES;
    p[6]=0U; p[7]=1U; /* 256 samples, little endian */
    Put32(p+8, session);
    Put32(p+12, block * 4U + fragment);
    uint64_t first = (uint64_t)block * SENDER_BLOCK_SAMPLES + fragment * SENDER_PACKET_SAMPLES;
    Put32(p+16, (uint32_t)first); Put32(p+20, (uint32_t)(first >> 32U));
    Put32(p+24, 2000U); /* nominal period in ns */
    for (unsigned i = 0; i < 3U; ++i) Put32(p+28+i*4U, pwm_hz[i]);
    Put32(p+40, block);
    p[44]=3U; p[45]=0U; /* actual GPIO samples + sequential ADC scan */
    p[46]=0x58U; p[47]=0x02U; /* rank 2 is sampled 600 ns after rank 1 */
}
