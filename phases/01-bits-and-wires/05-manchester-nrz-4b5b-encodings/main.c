#include "encoding.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_symbols(const uint8_t *sym, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        printf("%d", sym[i]);
        if ((i + 1) % 8 == 0) printf(" ");
    }
    printf("\n");
}

static void demo_manchester(void)
{
    printf("=== Manchester Encoding (IEEE 802.3) ===\n");
    uint8_t data[] = {0xA5};  /* 10100101 */
    uint8_t symbols[32];
    uint8_t decoded[1];

    printf("Input byte: 0x%02X (binary: 10100101)\n", data[0]);

    size_t nsym = nfs_manchester_encode(data, 1, symbols, sizeof(symbols));
    printf("Manchester symbols (%zu): ", nsym);
    print_symbols(symbols, nsym);

    int n = nfs_manchester_decode(symbols, nsym, decoded, sizeof(decoded));
    printf("Decoded: 0x%02X (roundtrip %s)\n\n",
           decoded[0], (n == 1 && decoded[0] == data[0]) ? "OK" : "FAIL");
}

static void demo_nrzi(void)
{
    printf("=== NRZI Encoding ===\n");
    uint8_t data[] = {0xA5};
    uint8_t symbols[16];
    uint8_t decoded[1];

    printf("Input byte: 0x%02X\n", data[0]);

    size_t nsym = nfs_nrzi_encode(data, 1, symbols, sizeof(symbols));
    printf("NRZI symbols (%zu): ", nsym);
    print_symbols(symbols, nsym);

    int n = nfs_nrzi_decode(symbols, nsym, decoded, sizeof(decoded));
    printf("Decoded: 0x%02X (roundtrip %s)\n\n",
           decoded[0], (n == 1 && decoded[0] == data[0]) ? "OK" : "FAIL");
}

static void demo_4b5b(void)
{
    printf("=== 4B/5B Encoding Table ===\n");
    printf("Nibble  5-bit code  Binary\n");
    for (uint8_t i = 0; i < 16; i++) {
        int code = nfs_4b5b_encode(i);
        printf("  0x%X     0x%02X     ", i, code);
        for (int bit = 4; bit >= 0; bit--)
            printf("%d", (code >> bit) & 1);
        printf("\n");
    }
    printf("\nRoundtrip check: ");
    int ok = 1;
    for (uint8_t i = 0; i < 16; i++) {
        int code = nfs_4b5b_encode(i);
        int dec = nfs_4b5b_decode((uint8_t)code);
        if (dec != i) { ok = 0; break; }
    }
    printf("%s\n", ok ? "ALL PASS" : "FAIL");
}

int main(void)
{
    demo_manchester();
    demo_nrzi();
    demo_4b5b();
    return 0;
}
