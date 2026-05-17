/*
 * main.c -- TTL and traceroute trick demo
 *
 * Shows how TTL decrement works at each hop, and simulates
 * traceroute to discover the path through a network.
 */

#include "ttl.h"

#include <stdio.h>
#include <string.h>

/* Build an IPv4 address in host byte order from four octets */
static uint32_t ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
}

static void print_ip(uint32_t addr) {
    printf("%u.%u.%u.%u", (addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF,
           addr & 0xFF);
}

int main(void) {
    printf("=== TTL and the Traceroute Trick Demo ===\n\n");

    /* --- Part 1: TTL decrement demo --- */
    printf("--- Part 1: TTL Decrement ---\n\n");

    struct nfs_ipv4_hdr pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.ver_ihl = 0x45; /* IPv4, 5 words (20 bytes) */
    pkt.total_len = 40; /* 20 header + 20 payload   */
    pkt.ttl = 3;
    pkt.protocol = 17; /* UDP */

    printf("Initial TTL: %u\n", nfs_ttl_get(&pkt));

    for (int hop = 1; hop <= 4; hop++) {
        int result = nfs_ttl_decrement(&pkt);
        if (result > 0) {
            printf("  Hop %d: TTL decremented to %d -- forwarding\n", hop, result);
        } else if (result == 0) {
            printf("  Hop %d: TTL reached 0 -- ICMP Time Exceeded!\n", hop);
        } else {
            printf("  Hop %d: TTL was already 0 -- packet dropped\n", hop);
        }
    }

    /* --- Part 2: Traceroute simulation --- */
    printf("\n--- Part 2: Traceroute Simulation ---\n\n");

    uint32_t src = ip(192, 168, 1, 100);
    uint32_t dst = ip(8, 8, 8, 8);

    /* Intermediate routers on the path */
    uint32_t hops[] = {
        ip(192, 168, 1, 1), /* home router         */
        ip(10, 0, 0, 1),    /* ISP gateway         */
        ip(72, 14, 233, 1), /* backbone router     */
    };
    size_t nhops = sizeof(hops) / sizeof(hops[0]);

    printf("Traceroute from ");
    print_ip(src);
    printf(" to ");
    print_ip(dst);
    printf(":\n\n");

    struct nfs_traceroute_hop results[16];
    int count = nfs_traceroute_sim(src, dst, hops, nhops, results, 16);

    for (int i = 0; i < count; i++) {
        printf("  %2d  ", results[i].hop_num);
        print_ip(results[i].responder_ip);
        if (results[i].reached_dest)
            printf("  <-- destination reached!");
        else
            printf("  (ICMP Time Exceeded)");
        printf("\n");
    }

    printf("\nTotal hops: %d\n", count);

    return 0;
}
