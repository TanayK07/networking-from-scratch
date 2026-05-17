#include "ipv6_addr.h"
#include <stdio.h>
#include <string.h>

static void demo(void) {
    printf("=== IPv6 Addressing Demo ===\n\n");

    /* Test various address types */
    struct {
        const char *label;
        uint8_t addr[16];
    } addrs[] = {
        {"Loopback (::1)", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
        {"Unspecified (::)", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {"Link-local",
         {0xFE, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x1A, 0x2B, 0xFF, 0xFE, 0x3C, 0x4D, 0x5E}},
        {"Multicast (ff02::1)", {0xFF, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
        {"Global unicast (2001:db8::1)",
         {0x20, 0x01, 0x0D, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
        {"ULA (fd00::1)", {0xFD, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
    };

    for (int i = 0; i < 6; i++) {
        char buf[64];
        nfs_ipv6_addr_format(addrs[i].addr, buf, sizeof(buf));
        printf("%-30s  %s  type=%s\n", addrs[i].label, buf, nfs_ipv6_addr_type_str(addrs[i].addr));
    }

    /* EUI-64 from MAC */
    printf("\nEUI-64 from MAC 00:1A:2B:3C:4D:5E:\n");
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t prefix[8] = {0xFE, 0x80, 0, 0, 0, 0, 0, 0};
    uint8_t result[16];
    nfs_ipv6_addr_from_eui64(mac, prefix, result);
    char buf[64];
    nfs_ipv6_addr_format(result, buf, sizeof(buf));
    printf("  %s\n", buf);

    /* Solicited-node multicast */
    uint8_t snmc[16];
    nfs_ipv6_addr_solicited_node(result, snmc);
    nfs_ipv6_addr_format(snmc, buf, sizeof(buf));
    printf("  Solicited-node: %s\n", buf);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    demo();
    return 0;
}
