#include "wol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void hexdump(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
        else if ((i + 1) % 8 == 0)
            printf("  ");
        else
            printf(" ");
    }
    if (len % 16 != 0)
        printf("\n");
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s build <mac>              -- build magic packet\n", prog);
    fprintf(stderr, "  %s build <mac> <password>   -- build with SecureOn password\n", prog);
    fprintf(stderr, "  %s demo                     -- run a demo\n", prog);
    fprintf(stderr, "\nMAC format: aa:bb:cc:dd:ee:ff or aa-bb-cc-dd-ee-ff\n");
}

static void demo(void) {
    printf("=== Wake-on-LAN Demo ===\n\n");

    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t pkt[108];

    int n = nfs_wol_build(mac, pkt, sizeof(pkt));
    printf("Magic packet for 00:1a:2b:3c:4d:5e (%d bytes):\n", n);
    hexdump(pkt, (size_t)n);

    /* Validate */
    uint8_t extracted[6];
    int rc = nfs_wol_validate(pkt, (size_t)n, extracted);
    char fmt[18];
    nfs_wol_mac_format(extracted, fmt, sizeof(fmt));
    printf("\nValidation: %s, extracted MAC: %s\n", rc == 0 ? "PASS" : "FAIL", fmt);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "demo") == 0) {
        demo();
        return 0;
    }

    if (strcmp(argv[1], "build") == 0 && argc >= 3) {
        uint8_t mac[6];
        if (nfs_wol_mac_parse(argv[2], mac) < 0) {
            fprintf(stderr, "Invalid MAC: %s\n", argv[2]);
            return 1;
        }

        uint8_t pkt[108];
        int n;

        if (argc == 4) {
            /* Treat password as raw hex bytes (8 or 12 hex chars) */
            fprintf(stderr, "Password mode not implemented in CLI\n");
            return 1;
        }

        n = nfs_wol_build(mac, pkt, sizeof(pkt));
        if (n < 0) {
            fprintf(stderr, "Failed to build magic packet\n");
            return 1;
        }

        char fmt[18];
        nfs_wol_mac_format(mac, fmt, sizeof(fmt));
        printf("Magic packet for %s (%d bytes):\n", fmt, n);
        hexdump(pkt, (size_t)n);
        return 0;
    }

    usage(argv[0]);
    return 1;
}
