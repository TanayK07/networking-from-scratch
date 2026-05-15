#include "arp_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Init / free                                                        */
/* ------------------------------------------------------------------ */

void nfs_arp_cache_init(struct nfs_arp_cache *c, size_t capacity)
{
    c->entries  = calloc(capacity, sizeof(*c->entries));
    c->capacity = capacity;
    c->count    = 0;
}

void nfs_arp_cache_free(struct nfs_arp_cache *c)
{
    free(c->entries);
    c->entries  = NULL;
    c->capacity = 0;
    c->count    = 0;
}

/* ------------------------------------------------------------------ */
/*  Internal: find index by IP, returns -1 if not found               */
/* ------------------------------------------------------------------ */

static int find_index(const struct nfs_arp_cache *c, uint32_t ip)
{
    for (size_t i = 0; i < c->count; i++) {
        if (c->entries[i].ip == ip)
            return (int)i;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/*  Insert / update                                                    */
/* ------------------------------------------------------------------ */

int nfs_arp_cache_insert(struct nfs_arp_cache *c, uint32_t ip,
                         const uint8_t mac[6], int state)
{
    int idx = find_index(c, ip);
    if (idx >= 0) {
        /* Update existing entry. */
        memcpy(c->entries[idx].mac, mac, 6);
        c->entries[idx].state     = state;
        c->entries[idx].timestamp = time(NULL);
        return 0;
    }

    /* New entry — check capacity. */
    if (c->count >= c->capacity)
        return -1;

    struct nfs_arp_entry *e = &c->entries[c->count++];
    e->ip        = ip;
    memcpy(e->mac, mac, 6);
    e->state     = state;
    e->timestamp = time(NULL);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Lookup                                                             */
/* ------------------------------------------------------------------ */

const struct nfs_arp_entry *nfs_arp_cache_lookup(
    const struct nfs_arp_cache *c, uint32_t ip)
{
    int idx = find_index(c, ip);
    if (idx < 0)
        return NULL;
    return &c->entries[idx];
}

/* ------------------------------------------------------------------ */
/*  Remove                                                             */
/* ------------------------------------------------------------------ */

int nfs_arp_cache_remove(struct nfs_arp_cache *c, uint32_t ip)
{
    int idx = find_index(c, ip);
    if (idx < 0)
        return -1;

    /* Swap with the last entry for O(1) removal. */
    size_t last = c->count - 1;
    if ((size_t)idx != last)
        c->entries[idx] = c->entries[last];
    c->count--;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Aging                                                              */
/* ------------------------------------------------------------------ */

int nfs_arp_cache_age(struct nfs_arp_cache *c, int timeout_sec)
{
    time_t now = time(NULL);
    int changed = 0;

    for (size_t i = 0; i < c->count; /* increment inside */) {
        struct nfs_arp_entry *e = &c->entries[i];
        double age = difftime(now, e->timestamp);

        if (e->state == NFS_ARP_REACHABLE && age >= timeout_sec) {
            e->state     = NFS_ARP_STALE;
            e->timestamp = now;
            changed++;
            i++;
        } else if (e->state == NFS_ARP_STALE && age >= timeout_sec) {
            /* Remove: swap with last. */
            size_t last = c->count - 1;
            if (i != last)
                c->entries[i] = c->entries[last];
            c->count--;
            changed++;
            /* Don't increment i — re-check the swapped entry. */
        } else {
            i++;
        }
    }
    return changed;
}

/* ------------------------------------------------------------------ */
/*  State name                                                         */
/* ------------------------------------------------------------------ */

const char *nfs_arp_state_name(int state)
{
    switch (state) {
    case NFS_ARP_INCOMPLETE: return "INCOMPLETE";
    case NFS_ARP_REACHABLE:  return "REACHABLE";
    case NFS_ARP_STALE:      return "STALE";
    case NFS_ARP_FAILED:     return "FAILED";
    default:                 return "UNKNOWN";
    }
}

/* ------------------------------------------------------------------ */
/*  Format                                                             */
/* ------------------------------------------------------------------ */

void nfs_arp_cache_format(const struct nfs_arp_cache *c,
                          char *buf, size_t sz)
{
    if (!buf || sz == 0) return;
    buf[0] = '\0';

    size_t off = 0;
    for (size_t i = 0; i < c->count && off < sz - 1; i++) {
        const struct nfs_arp_entry *e = &c->entries[i];
        uint8_t a = (uint8_t)(e->ip >> 24);
        uint8_t b = (uint8_t)(e->ip >> 16);
        uint8_t cc = (uint8_t)(e->ip >> 8);
        uint8_t d = (uint8_t)(e->ip);
        int n = snprintf(buf + off, sz - off,
                         "%u.%u.%u.%u  %02x:%02x:%02x:%02x:%02x:%02x  %s\n",
                         a, b, cc, d,
                         e->mac[0], e->mac[1], e->mac[2],
                         e->mac[3], e->mac[4], e->mac[5],
                         nfs_arp_state_name(e->state));
        if (n < 0) break;
        off += (size_t)n;
    }
}
