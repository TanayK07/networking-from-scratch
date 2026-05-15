/* Path MTU Discovery (RFC 1191) — interactive demonstration. */

#include "pmtud.h"
#include <stdio.h>

int main(void)
{
    printf("=== Path MTU Discovery (RFC 1191) ===\n\n");

    /* --- Plateau table walk --- */
    printf("RFC 1191 plateau table traversal starting from 65535:\n");
    uint32_t mtu = 65535;
    while (mtu > 68) {
        uint32_t next = nfs_pmtu_next_lower(mtu);
        printf("  %5u  ->  %5u\n", mtu, next);
        mtu = next;
    }
    printf("  %5u  (minimum)\n\n", mtu);

    /* --- PMTU cache demonstration --- */
    struct nfs_pmtu_cache cache;
    nfs_pmtu_cache_init(&cache, 16);

    /* Simulate discovering PMTU to 10.0.0.1 */
    uint32_t dest = (10u << 24) | 1;  /* 10.0.0.1 */
    printf("Simulating PMTUD to 10.0.0.1:\n");
    printf("  Initial lookup: MTU = %u (unknown)\n",
           nfs_pmtu_cache_lookup(&cache, dest));

    /* First packet sent with DF=1, interface MTU 1500 */
    nfs_pmtu_cache_update(&cache, dest, 1500, 0.0);
    printf("  After setting DF, assume link MTU 1500: MTU = %u\n",
           nfs_pmtu_cache_lookup(&cache, dest));

    /* ICMP Fragmentation Needed received, next-hop MTU 1006 */
    printf("  ICMP Frag Needed received!  Trying plateau: %u\n",
           nfs_pmtu_next_lower(1500));
    nfs_pmtu_cache_update(&cache, dest, 1006, 1.0);
    printf("  Cache updated: MTU = %u\n",
           nfs_pmtu_cache_lookup(&cache, dest));

    /* Another ICMP, need to go lower */
    printf("  Another ICMP!  Trying plateau: %u\n",
           nfs_pmtu_next_lower(1006));
    nfs_pmtu_cache_update(&cache, dest, 508, 2.0);
    printf("  Cache updated: MTU = %u\n\n",
           nfs_pmtu_cache_lookup(&cache, dest));

    /* Add a second destination */
    uint32_t dest2 = (192u << 24) | (168u << 16) | (1u << 8) | 100;
    nfs_pmtu_cache_update(&cache, dest2, 1492, 3.0);

    /* Format and print cache */
    char buf[1024];
    nfs_pmtu_format(&cache, buf, sizeof(buf));
    printf("%s", buf);

    /* Probe timer check */
    printf("\nProbe timer (600s interval):\n");
    struct nfs_pmtu_entry e = { .dest = dest, .pmtu = 508,
                                .last_updated = 2.0 };
    printf("  At t=100:  should_probe = %d\n",
           nfs_pmtu_should_probe(&e, 100.0, 600.0));
    printf("  At t=700:  should_probe = %d\n",
           nfs_pmtu_should_probe(&e, 700.0, 600.0));

    nfs_pmtu_cache_free(&cache);
    printf("\nDone.\n");
    return 0;
}
