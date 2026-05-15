#include "framing.h"
#include <stdio.h>
#include <string.h>

static void print_hex(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
}

static void demo_byte_stuffing(void)
{
    printf("=== PPP Byte Stuffing (RFC 1662) ===\n");

    /* Payload containing flag and escape bytes */
    uint8_t data[] = {0x01, 0x7E, 0x02, 0x7D, 0x03};
    uint8_t frame[32];
    uint8_t recovered[32];

    printf("Original data:  ");
    print_hex(data, sizeof(data));

    int flen = nfs_byte_stuff(data, sizeof(data), frame, sizeof(frame));
    printf("Stuffed frame:  ");
    print_hex(frame, (size_t)flen);

    int rlen = nfs_byte_unstuff(frame, (size_t)flen, recovered, sizeof(recovered));
    printf("Unstuffed data: ");
    print_hex(recovered, (size_t)rlen);

    printf("Roundtrip: %s\n\n",
           (rlen == (int)sizeof(data) && memcmp(data, recovered, sizeof(data)) == 0)
               ? "OK" : "FAIL");
}

static void demo_bit_stuffing(void)
{
    printf("=== HDLC Bit Stuffing ===\n");

    /* Data with five consecutive 1s: 0xFF = 11111111 */
    uint8_t data[] = {0xFF};
    uint8_t stuffed[4];
    uint8_t recovered[4];

    printf("Original bits: ");
    for (int i = 7; i >= 0; i--) printf("%d", (data[0] >> i) & 1);
    printf(" (0x%02X)\n", data[0]);

    int nbits = nfs_bit_stuff(data, 8, stuffed, sizeof(stuffed));
    printf("Stuffed bits (%d): ", nbits);
    for (int i = 0; i < nbits; i++) {
        printf("%d", (stuffed[i / 8] >> (7 - (i % 8))) & 1);
    }
    printf("\n");

    int rbits = nfs_bit_unstuff(stuffed, (size_t)nbits, recovered, sizeof(recovered));
    printf("Unstuffed bits (%d): ", rbits);
    for (int i = 0; i < rbits; i++) {
        printf("%d", (recovered[i / 8] >> (7 - (i % 8))) & 1);
    }
    printf("\nRoundtrip: %s\n\n",
           (rbits == 8 && recovered[0] == data[0]) ? "OK" : "FAIL");
}

static void demo_cobs(void)
{
    printf("=== COBS Encoding ===\n");

    uint8_t data[] = {0x00, 0x11, 0x00, 0x00, 0x22};
    uint8_t encoded[16];
    uint8_t decoded[16];

    printf("Original: ");
    print_hex(data, sizeof(data));

    int elen = nfs_cobs_encode(data, sizeof(data), encoded, sizeof(encoded));
    printf("Encoded:  ");
    print_hex(encoded, (size_t)elen);

    int dlen = nfs_cobs_decode(encoded, (size_t)elen, decoded, sizeof(decoded));
    printf("Decoded:  ");
    print_hex(decoded, (size_t)dlen);

    printf("Roundtrip: %s\n",
           (dlen == (int)sizeof(data) && memcmp(data, decoded, sizeof(data)) == 0)
               ? "OK" : "FAIL");
}

int main(void)
{
    demo_byte_stuffing();
    demo_bit_stuffing();
    demo_cobs();
    return 0;
}
