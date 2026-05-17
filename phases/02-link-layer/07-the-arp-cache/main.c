/*
 * arp_cache — demonstrate an in-memory ARP cache with aging.
 *
 * Build:  make
 * Run:    ./arp_cache
 */
#include "arp_cache.h"

#include <stdio.h>
#include <string.h>

/* Helper: pack an IPv4 address in host byte order. */
static uint32_t ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
}

int main(void) {
    printf("=== ARP Cache Demo ===\n\n");

    struct nfs_arp_cache cache;
    nfs_arp_cache_init(&cache, 16);

    uint8_t mac1[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t mac2[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t mac3[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};

    nfs_arp_cache_insert(&cache, ip(192, 168, 1, 1), mac1, NFS_ARP_REACHABLE);
    nfs_arp_cache_insert(&cache, ip(192, 168, 1, 2), mac2, NFS_ARP_REACHABLE);
    nfs_arp_cache_insert(&cache, ip(10, 0, 0, 1), mac3, NFS_ARP_INCOMPLETE);

    char buf[1024];
    nfs_arp_cache_format(&cache, buf, sizeof(buf));
    printf("Cache contents:\n%s\n", buf);

    const struct nfs_arp_entry *e = nfs_arp_cache_lookup(&cache, ip(192, 168, 1, 1));
    if (e) {
        printf("Lookup 192.168.1.1: state=%s\n", nfs_arp_state_name(e->state));
    }

    nfs_arp_cache_remove(&cache, ip(10, 0, 0, 1));
    printf("\nAfter removing 10.0.0.1:\n");
    nfs_arp_cache_format(&cache, buf, sizeof(buf));
    printf("%s", buf);

    nfs_arp_cache_free(&cache);
    return 0;
}
