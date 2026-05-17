/*
 * main.c -- SLAAC demo driver
 */
#include "slaac.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    char str[64];

    printf("=== SLAAC (Stateless Address Autoconfiguration) ===\n\n");
    printf("MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n\n", mac[0], mac[1], mac[2], mac[3], mac[4],
           mac[5]);

    /* EUI-64 */
    uint8_t eui64[8];
    nfs_slaac_eui64(mac, eui64);
    printf("EUI-64 Interface ID: ");
    for (int i = 0; i < 8; i++)
        printf("%02x%s", eui64[i], i < 7 ? ":" : "\n");

    /* Link-local address */
    uint8_t linklocal[16];
    nfs_slaac_linklocal(mac, linklocal);
    printf("Link-local address:  %s\n", nfs_slaac_format_ipv6(linklocal, str, sizeof(str)));

    /* Global address from prefix 2001:db8::/64 */
    uint8_t prefix[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t global[16];
    nfs_slaac_generate_addr(prefix, 64, mac, global);
    printf("Global address:      %s\n", nfs_slaac_format_ipv6(global, str, sizeof(str)));

    /* Solicited-node multicast */
    uint8_t mcast[16];
    nfs_slaac_solicited_node(global, mcast);
    printf("Solicited-node MC:   %s\n", nfs_slaac_format_ipv6(mcast, str, sizeof(str)));

    /* Prefix validation */
    printf("\nPrefix validation:\n");
    printf("  2001:db8::/64 valid:  %d\n", nfs_slaac_validate_prefix(prefix, 64, 1));
    uint8_t ll_prefix[16] = {0xFE, 0x80};
    printf("  fe80::/64 (reject):   %d\n", nfs_slaac_validate_prefix(ll_prefix, 64, 1));
    printf("  fe80::/64 (allow):    %d\n", nfs_slaac_validate_prefix(ll_prefix, 64, 0));

    return 0;
}
