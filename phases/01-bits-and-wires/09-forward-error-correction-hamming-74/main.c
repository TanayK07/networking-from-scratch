#include "hamming.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_bits(uint8_t val, int nbits)
{
    for (int i = nbits - 1; i >= 0; i--)
        printf("%d", (val >> i) & 1);
}

static void demo_encode_decode(void)
{
    printf("=== Hamming(7,4) Encode/Decode ===\n\n");
    printf("Data  Binary  Codeword  Syndrome\n");

    for (uint8_t i = 0; i < 16; i++) {
        uint8_t cw = nfs_hamming74_encode(i);
        uint8_t syn = nfs_hamming74_syndrome(cw);
        printf("  %X   ", i);
        print_bits(i, 4);
        printf("    ");
        print_bits(cw, 7);
        printf("       %d\n", syn);
    }
}

static void demo_error_correction(void)
{
    printf("\n=== Single-Bit Error Correction ===\n\n");

    uint8_t data = 0x0A;  /* 1010 */
    uint8_t cw = nfs_hamming74_encode(data);
    printf("Original data: 0x%X  Codeword: ", data);
    print_bits(cw, 7);
    printf("\n");

    /* Flip each bit position and show correction */
    for (int pos = 1; pos <= 7; pos++) {
        uint8_t corrupted = cw ^ (uint8_t)(1 << (pos - 1));
        uint8_t recovered;
        int result = nfs_hamming74_decode(corrupted, &recovered);

        printf("  Flip pos %d: ", pos);
        print_bits(corrupted, 7);
        printf(" → syndrome=%d, recovered=0x%X (%s)\n",
               nfs_hamming74_syndrome(corrupted),
               recovered,
               (result == 1 && recovered == data) ? "CORRECTED" : "ERROR");
    }
}

static void demo_buffer(void)
{
    printf("\n=== Buffer Encode/Decode ===\n");
    uint8_t data[] = {0xDE, 0xAD};
    uint8_t codes[4];
    uint8_t decoded[2];

    size_t ncodes = nfs_hamming_encode_buf(data, 2, codes, sizeof(codes));
    printf("Input:   0x%02X 0x%02X\n", data[0], data[1]);
    printf("Codes:   ");
    for (size_t i = 0; i < ncodes; i++) printf("0x%02X ", codes[i]);
    printf("\n");

    int n = nfs_hamming_decode_buf(codes, ncodes, decoded, sizeof(decoded));
    printf("Decoded: 0x%02X 0x%02X (%s)\n",
           decoded[0], decoded[1],
           (n == 2 && decoded[0] == data[0] && decoded[1] == data[1]) ? "OK" : "FAIL");
}

int main(void)
{
    demo_encode_decode();
    demo_error_correction();
    demo_buffer();
    return 0;
}
