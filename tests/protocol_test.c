#include "sender_protocol.h"
#include <assert.h>
#include <stdio.h>

/* Produce a known-value packet so an independent Python decoder can validate it. */
int main(void)
{
    uint8_t packet[SENDER_PACKET_BYTES] = {0};
    uint32_t pwm[3] = {1000, 2500, 5000};
    assert(SenderProtocol_Record(0x123, 0xABC, 5) == 0x05ABC123U);
    assert(SenderProtocol_Record(0xFFFF, 0xFFFF, 15) == 0x07FFFFFFU);
    SenderProtocol_Header(packet, 0x11223344U, 7U, 3U, pwm);
    for (unsigned i = 0; i < SENDER_PACKET_SAMPLES; ++i) {
        uint32_t value = SenderProtocol_Record(i, 4095U-i, i & 7U);
        for (unsigned byte = 0; byte < 4; ++byte)
            packet[SENDER_HEADER_BYTES + i*4 + byte] = (uint8_t)(value >> (byte*8));
    }
    FILE *f = fopen("protocol_test_packet.bin", "wb");
    assert(f != NULL);
    assert(fwrite(packet, 1, sizeof packet, f) == sizeof packet);
    fclose(f);
    puts("PASS: 27-bit packing and explicit protocol serialization");
    return 0;
}
