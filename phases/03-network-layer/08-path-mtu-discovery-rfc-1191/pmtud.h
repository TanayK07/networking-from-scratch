#ifndef NFS_PMTUD_H
#define NFS_PMTUD_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Path MTU Discovery (RFC 1191)
 *
 * PMTUD works by setting the Don't Fragment (DF) bit on outgoing
 * IP packets.  When a router along the path has a link MTU smaller
 * than the packet, it drops the packet and returns an ICMP
 * "Fragmentation Needed and DF Set" (Type 3, Code 4) message
 * containing the next-hop MTU.
 *
 * The sender caches the discovered PMTU per destination and uses
 * the RFC 1191 plateau table to pick the next lower MTU when no
 * next-hop MTU is provided in the ICMP message.
 *
 * This module implements the PMTU cache, plateau table lookup,
 * and probe timer logic.
 * --------------------------------------------------------------- */

/* RFC 1191 plateau table (common MTU values across link technologies) */
#define NFS_PMTU_PLATEAU_COUNT 11

/* A single cache entry mapping a destination to its discovered PMTU. */
struct nfs_pmtu_entry {
    uint32_t dest;         /* destination IP (network byte order) */
    uint32_t pmtu;         /* discovered path MTU in bytes        */
    double   last_updated; /* timestamp of last update (seconds)  */
};

/* Cache of PMTU entries, one per destination. */
struct nfs_pmtu_cache {
    struct nfs_pmtu_entry *entries;
    size_t count;
    size_t capacity;
};

/* Initialize a PMTU cache with the given capacity. */
void nfs_pmtu_cache_init(struct nfs_pmtu_cache *c, size_t capacity);

/* Free all resources held by a PMTU cache. */
void nfs_pmtu_cache_free(struct nfs_pmtu_cache *c);

/* Insert or update a PMTU entry.  Returns 0 on success, -1 if full. */
int nfs_pmtu_cache_update(struct nfs_pmtu_cache *c, uint32_t dest,
                          uint32_t new_mtu, double now);

/* Look up the cached PMTU for a destination.  Returns 0 if unknown. */
uint32_t nfs_pmtu_cache_lookup(const struct nfs_pmtu_cache *c,
                               uint32_t dest);

/* RFC 1191 plateau table: return the next lower MTU value.
 * The plateau table is: {65535,32000,17914,8166,4352,2002,1492,1006,
 *                        508,296,68}.
 * Returns the next value strictly less than current_mtu, or 68 (the
 * minimum) if current_mtu is already at or below 68. */
uint32_t nfs_pmtu_next_lower(uint32_t current_mtu);

/* Return 1 if enough time has elapsed since the last PMTU update
 * to warrant probing a larger MTU again (RFC 1191 recommends 10 min). */
int nfs_pmtu_should_probe(const struct nfs_pmtu_entry *e, double now,
                          double probe_interval);

/* Format the PMTU cache into a human-readable string. */
void nfs_pmtu_format(const struct nfs_pmtu_cache *c, char *buf, size_t sz);

#endif /* NFS_PMTUD_H */
