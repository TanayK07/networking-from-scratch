/*
 * ttl.c -- TTL manipulation and traceroute simulation
 *
 * Demonstrates how the IPv4 Time-To-Live field works and how
 * traceroute exploits it to map the network path.
 *
 * Each router on the path decrements TTL by 1.  When TTL reaches 0
 * the router drops the packet and sends an ICMP Time Exceeded
 * message back to the source.  By sending probes with TTL=1, 2, 3, ...
 * the source discovers each hop in order.
 */

#include "ttl.h"

#include <string.h>

/* ---------- TTL operations ------------------------------------------- */

int nfs_ttl_decrement(struct nfs_ipv4_hdr *hdr) {
    if (hdr->ttl == 0)
        return -1;

    hdr->ttl--;

    if (hdr->ttl == 0)
        return 0; /* Time Exceeded -- router should send ICMP */

    return (int)hdr->ttl;
}

void nfs_ttl_set(struct nfs_ipv4_hdr *hdr, uint8_t ttl) {
    hdr->ttl = ttl;
}

uint8_t nfs_ttl_get(const struct nfs_ipv4_hdr *hdr) {
    return hdr->ttl;
}

/* ---------- traceroute simulation ------------------------------------ */

int nfs_traceroute_sim(uint32_t src, uint32_t dst, const uint32_t *hops, size_t nhops,
                       struct nfs_traceroute_hop *results, size_t max_results) {
    (void)src; /* src is conceptual -- the probes originate here */

    int recorded = 0;

    /*
     * For each probe with increasing initial TTL:
     *   TTL starts at `probe_ttl` (1, 2, 3, ...)
     *   Walk through intermediate hops, decrementing at each.
     *   If TTL reaches 0 at hop[i], that router responds.
     *   If we pass all hops and still have TTL > 0, the packet
     *   reaches the destination.
     */
    for (size_t probe_ttl = 1;; probe_ttl++) {
        if ((size_t)recorded >= max_results)
            break;

        uint8_t ttl = (uint8_t)probe_ttl;
        int expired = 0;

        /* Walk through intermediate routers */
        for (size_t i = 0; i < nhops; i++) {
            /* Each router decrements TTL */
            if (ttl == 0) {
                /* Should not happen in normal flow, but guard */
                expired = 1;
                break;
            }
            ttl--;
            if (ttl == 0) {
                /* Time Exceeded at this router */
                results[recorded].hop_num = recorded + 1;
                results[recorded].responder_ip = hops[i];
                results[recorded].reached_dest = 0;
                recorded++;
                expired = 1;
                break;
            }
        }

        if (expired)
            continue;

        /*
         * Packet survived all intermediate hops.
         * It reaches the destination.  The destination itself
         * does not decrement TTL -- it receives the packet
         * and responds (e.g., ICMP Port Unreachable for UDP traceroute).
         */
        results[recorded].hop_num = recorded + 1;
        results[recorded].responder_ip = dst;
        results[recorded].reached_dest = 1;
        recorded++;
        break; /* traceroute complete */
    }

    return recorded;
}
