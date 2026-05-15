/*
 * lpm.c -- Longest-Prefix Match routing table (linear scan)
 *
 * A straightforward implementation that scans all routes to find
 * the one with the longest matching prefix.  Not optimized for
 * large tables (a trie would be better), but clear and correct
 * for learning purposes.
 */

#include "lpm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- helpers -------------------------------------------------- */

/*
 * Compute the bitmask for a given prefix length.
 *   prefix_len == 0  =>  mask = 0x00000000
 *   prefix_len == 8  =>  mask = 0xFF000000
 *   prefix_len == 32 =>  mask = 0xFFFFFFFF
 */
static uint32_t prefix_mask(uint8_t prefix_len)
{
    if (prefix_len == 0)
        return 0;
    return 0xFFFFFFFF << (32 - prefix_len);
}

/* ---------- public API ----------------------------------------------- */

void nfs_route_table_init(struct nfs_route_table *t, size_t capacity)
{
    t->routes   = calloc(capacity, sizeof(struct nfs_route));
    t->count    = 0;
    t->capacity = capacity;
}

void nfs_route_table_free(struct nfs_route_table *t)
{
    free(t->routes);
    t->routes   = NULL;
    t->count    = 0;
    t->capacity = 0;
}

int nfs_route_add(struct nfs_route_table *t, uint32_t prefix,
                  uint8_t prefix_len, uint32_t next_hop, int iface)
{
    if (!t || prefix_len > 32)
        return -1;

    uint32_t mask = prefix_mask(prefix_len);

    /* Check for duplicate prefix/len -- replace if found */
    for (size_t i = 0; i < t->count; i++) {
        if (t->routes[i].prefix_len == prefix_len &&
            (t->routes[i].prefix & mask) == (prefix & mask)) {
            t->routes[i].prefix   = prefix & mask;
            t->routes[i].next_hop = next_hop;
            t->routes[i].interface_id = iface;
            return 0;
        }
    }

    if (t->count >= t->capacity)
        return -1;

    t->routes[t->count].prefix       = prefix & mask;
    t->routes[t->count].prefix_len   = prefix_len;
    t->routes[t->count].next_hop     = next_hop;
    t->routes[t->count].interface_id = iface;
    t->count++;
    return 0;
}

int nfs_route_remove(struct nfs_route_table *t, uint32_t prefix,
                     uint8_t prefix_len)
{
    if (!t || prefix_len > 32)
        return -1;

    uint32_t mask = prefix_mask(prefix_len);

    for (size_t i = 0; i < t->count; i++) {
        if (t->routes[i].prefix_len == prefix_len &&
            (t->routes[i].prefix & mask) == (prefix & mask)) {
            /* Swap with last entry and shrink */
            t->routes[i] = t->routes[t->count - 1];
            t->count--;
            return 0;
        }
    }
    return -1;
}

const struct nfs_route *nfs_route_lookup(const struct nfs_route_table *t,
                                          uint32_t dst)
{
    if (!t)
        return NULL;

    const struct nfs_route *best = NULL;
    int best_len = -1;

    for (size_t i = 0; i < t->count; i++) {
        const struct nfs_route *r = &t->routes[i];
        uint32_t mask = prefix_mask(r->prefix_len);

        if ((dst & mask) == (r->prefix & mask)) {
            if ((int)r->prefix_len > best_len) {
                best     = r;
                best_len = (int)r->prefix_len;
            }
        }
    }
    return best;
}

void nfs_route_table_format(const struct nfs_route_table *t, char *buf, size_t sz)
{
    if (!t || !buf || sz == 0)
        return;

    size_t offset = 0;
    int n;

    n = snprintf(buf + offset, sz - offset,
                 "%-18s %-18s %s\n",
                 "Prefix", "Next Hop", "Iface");
    if (n < 0 || (size_t)n >= sz - offset) return;
    offset += (size_t)n;

    n = snprintf(buf + offset, sz - offset,
                 "--------------------------------------------------\n");
    if (n < 0 || (size_t)n >= sz - offset) return;
    offset += (size_t)n;

    for (size_t i = 0; i < t->count; i++) {
        const struct nfs_route *r = &t->routes[i];
        uint32_t p  = r->prefix;
        uint32_t nh = r->next_hop;

        n = snprintf(buf + offset, sz - offset,
                     "%u.%u.%u.%u/%-5u %u.%u.%u.%u      %d\n",
                     (p >> 24) & 0xFF, (p >> 16) & 0xFF,
                     (p >> 8) & 0xFF, p & 0xFF,
                     r->prefix_len,
                     (nh >> 24) & 0xFF, (nh >> 16) & 0xFF,
                     (nh >> 8) & 0xFF, nh & 0xFF,
                     r->interface_id);
        if (n < 0 || (size_t)n >= sz - offset) return;
        offset += (size_t)n;
    }
}
