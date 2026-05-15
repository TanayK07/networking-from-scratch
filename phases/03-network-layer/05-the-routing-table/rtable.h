#ifndef NFS_RTABLE_H
#define NFS_RTABLE_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * IP routing table with longest-prefix-match (LPM) lookup.
 *
 * Each entry maps a destination prefix (network + prefix length) to
 * a gateway, interface index, metric, and flags.  Lookup returns the
 * most-specific matching prefix; ties are broken by lowest metric.
 * --------------------------------------------------------------- */

/* Route flags */
#define NFS_RTF_UP      0x01    /* Route is up */
#define NFS_RTF_GATEWAY 0x02    /* Destination is reached via gateway */
#define NFS_RTF_HOST    0x04    /* Host route (/32) */

struct nfs_rt_entry {
    uint32_t dest;          /* Network address (host byte order) */
    uint8_t  prefix_len;    /* CIDR prefix length (0-32) */
    uint32_t gateway;       /* Next-hop gateway (host byte order, 0=direct) */
    int      iface;         /* Outgoing interface index */
    uint32_t metric;        /* Route metric (lower = preferred) */
    int      flags;         /* NFS_RTF_* flags */
};

struct nfs_rtable {
    struct nfs_rt_entry *entries;
    size_t count;
    size_t capacity;
};

/* Initialise a routing table that can hold `capacity` entries. */
void nfs_rtable_init(struct nfs_rtable *t, size_t capacity);

/* Free all resources. */
void nfs_rtable_free(struct nfs_rtable *t);

/* Add a route.  Returns 0 on success, -1 if the table is full. */
int nfs_rtable_add(struct nfs_rtable *t, uint32_t dest, uint8_t prefix,
                   uint32_t gw, int iface, uint32_t metric, int flags);

/* Look up a destination using longest-prefix match.
 * Ties (same prefix length) are broken by lowest metric.
 * Returns a pointer to the matching entry, or NULL. */
const struct nfs_rt_entry *nfs_rtable_lookup(const struct nfs_rtable *t,
                                             uint32_t dst);

/* Remove the route matching (dest, prefix).
 * Returns 0 on success, -1 if not found. */
int nfs_rtable_remove(struct nfs_rtable *t, uint32_t dest, uint8_t prefix);

/* Format the routing table like `ip route` output.
 * Returns characters written (excluding NUL), or -1 on error. */
int nfs_rtable_format(const struct nfs_rtable *t, char *buf, size_t sz);

/* Helper: build an IPv4 address from four octets (host byte order). */
static inline uint32_t nfs_ip4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) |
           ((uint32_t)c << 8)  | (uint32_t)d;
}

/* Helper: compute a prefix mask from a prefix length. */
static inline uint32_t nfs_prefix_mask(uint8_t len)
{
    if (len == 0) return 0;
    return ~((uint32_t)0) << (32 - len);
}

#endif /* NFS_RTABLE_H */
