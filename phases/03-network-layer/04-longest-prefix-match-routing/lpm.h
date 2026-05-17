#ifndef NFS_LPM_H
#define NFS_LPM_H

#include <stddef.h>
#include <stdint.h>

/*
 * Longest-Prefix Match (LPM) Routing Table
 *
 * A simple linear-scan routing table that finds the most specific
 * (longest prefix) route matching a given destination IP address.
 *
 * All IP addresses are in HOST byte order (uint32_t).
 */

struct nfs_route {
    uint32_t prefix;    /* Network prefix (host byte order) */
    uint8_t prefix_len; /* Prefix length in bits (0-32)     */
    uint32_t next_hop;  /* Next-hop gateway (host byte order) */
    int interface_id;   /* Outgoing interface identifier     */
};

struct nfs_route_table {
    struct nfs_route *routes;
    size_t count;
    size_t capacity;
};

/*
 * Initialize a route table with given capacity.
 * Allocates memory for up to `capacity` routes.
 */
void nfs_route_table_init(struct nfs_route_table *t, size_t capacity);

/*
 * Free all memory associated with a route table.
 */
void nfs_route_table_free(struct nfs_route_table *t);

/*
 * Add a route to the table. If a route with the same prefix and
 * prefix_len already exists, it is replaced (updated).
 * Returns 0 on success, -1 on error (table full or invalid args).
 */
int nfs_route_add(struct nfs_route_table *t, uint32_t prefix, uint8_t prefix_len, uint32_t next_hop,
                  int iface);

/*
 * Remove the route matching the given prefix and prefix_len.
 * Returns 0 on success, -1 if not found.
 */
int nfs_route_remove(struct nfs_route_table *t, uint32_t prefix, uint8_t prefix_len);

/*
 * Look up the longest matching prefix for destination `dst`.
 * Returns a pointer to the best matching route, or NULL if no match.
 */
const struct nfs_route *nfs_route_lookup(const struct nfs_route_table *t, uint32_t dst);

/*
 * Format the routing table as a human-readable string into buf (up to sz bytes).
 */
void nfs_route_table_format(const struct nfs_route_table *t, char *buf, size_t sz);

#endif /* NFS_LPM_H */
