/* Path MTU Discovery (RFC 1191) — implementation. */

#include "pmtud.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RFC 1191 plateau table — common MTU sizes across link technologies.
 * Sorted descending so we can walk it to find the next lower value. */
static const uint32_t plateau_table[NFS_PMTU_PLATEAU_COUNT] = {
    65535, 32000, 17914, 8166, 4352, 2002, 1492, 1006, 508, 296, 68};

void nfs_pmtu_cache_init(struct nfs_pmtu_cache *c, size_t capacity) {
    c->entries = calloc(capacity, sizeof(struct nfs_pmtu_entry));
    c->count = 0;
    c->capacity = capacity;
}

void nfs_pmtu_cache_free(struct nfs_pmtu_cache *c) {
    free(c->entries);
    c->entries = NULL;
    c->count = 0;
    c->capacity = 0;
}

int nfs_pmtu_cache_update(struct nfs_pmtu_cache *c, uint32_t dest, uint32_t new_mtu, double now) {
    /* Search for existing entry. */
    for (size_t i = 0; i < c->count; i++) {
        if (c->entries[i].dest == dest) {
            c->entries[i].pmtu = new_mtu;
            c->entries[i].last_updated = now;
            return 0;
        }
    }
    /* Insert new entry. */
    if (c->count >= c->capacity)
        return -1;
    c->entries[c->count].dest = dest;
    c->entries[c->count].pmtu = new_mtu;
    c->entries[c->count].last_updated = now;
    c->count++;
    return 0;
}

uint32_t nfs_pmtu_cache_lookup(const struct nfs_pmtu_cache *c, uint32_t dest) {
    for (size_t i = 0; i < c->count; i++) {
        if (c->entries[i].dest == dest)
            return c->entries[i].pmtu;
    }
    return 0; /* unknown */
}

uint32_t nfs_pmtu_next_lower(uint32_t current_mtu) {
    /* Walk the plateau table and return the first value strictly less
     * than current_mtu.  If nothing qualifies, return 68 (minimum). */
    for (int i = 0; i < NFS_PMTU_PLATEAU_COUNT; i++) {
        if (plateau_table[i] < current_mtu)
            return plateau_table[i];
    }
    return 68;
}

int nfs_pmtu_should_probe(const struct nfs_pmtu_entry *e, double now, double probe_interval) {
    return (now - e->last_updated) >= probe_interval ? 1 : 0;
}

void nfs_pmtu_format(const struct nfs_pmtu_cache *c, char *buf, size_t sz) {
    size_t off = 0;
    int n = snprintf(buf + off, sz - off, "PMTU Cache (%zu entries):\n", c->count);
    if (n < 0)
        return;
    off += (size_t)n;

    for (size_t i = 0; i < c->count && off < sz; i++) {
        uint32_t ip = c->entries[i].dest;
        n = snprintf(buf + off, sz - off, "  %u.%u.%u.%u  ->  MTU %u  (updated %.1f)\n",
                     (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
                     c->entries[i].pmtu, c->entries[i].last_updated);
        if (n < 0)
            return;
        off += (size_t)n;
    }
}
