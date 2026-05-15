/*
 * mac_tool — demonstrate MAC address classification and OUI extraction.
 *
 * Build:  make
 * Run:    ./mac_tool [aa:bb:cc:dd:ee:ff]
 */
#include "mac.h"

#include <stdio.h>
#include <string.h>

static void analyse(const uint8_t mac[6])
{
    char fmt[32];
    nfs_mac_format(mac, fmt, sizeof(fmt));

    uint8_t oui[3];
    nfs_mac_get_oui(mac, oui);

    printf("MAC:       %s\n", fmt);
    printf("Type:      %s\n", nfs_mac_type_str(mac));
    printf("OUI:       %02x:%02x:%02x\n", oui[0], oui[1], oui[2]);
    printf("Multicast: %s\n", nfs_mac_is_multicast(mac) ? "yes" : "no");
    printf("Local:     %s\n", nfs_mac_is_local(mac) ? "yes" : "no");
    printf("\n");
}

int main(int argc, char *argv[])
{
    printf("=== MAC Address & OUI Tool ===\n\n");

    if (argc > 1) {
        uint8_t mac[6];
        if (nfs_mac_parse(argv[1], mac) == 0) {
            analyse(mac);
        } else {
            fprintf(stderr, "Invalid MAC: %s\n", argv[1]);
            return 1;
        }
        return 0;
    }

    /* Demo with well-known addresses. */
    uint8_t bcast[6]   = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t mcast[6]   = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};  /* IPv4 multicast */
    uint8_t local[6]   = {0x02, 0x42, 0xAC, 0x11, 0x00, 0x02};  /* Docker-style */
    uint8_t global[6]  = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};  /* normal NIC */

    analyse(bcast);
    analyse(mcast);
    analyse(local);
    analyse(global);

    return 0;
}
