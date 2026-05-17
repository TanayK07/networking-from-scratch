#include "rtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void nfs_rtable_init(struct nfs_rtable *t, size_t capacity) {
    t->entries = calloc(capacity, sizeof(struct nfs_rt_entry));
    t->count = 0;
    t->capacity = capacity;
}

void nfs_rtable_free(struct nfs_rtable *t) {
    free(t->entries);
    t->entries = NULL;
    t->count = 0;
    t->capacity = 0;
}

int nfs_rtable_add(struct nfs_rtable *t, uint32_t dest, uint8_t prefix, uint32_t gw, int iface,
                   uint32_t metric, int flags) {
    if (t->count >= t->capacity)
        return -1;

    /* Mask the destination to ensure it matches the prefix */
    uint32_t mask = nfs_prefix_mask(prefix);

    struct nfs_rt_entry *e = &t->entries[t->count++];
    e->dest = dest & mask;
    e->prefix_len = prefix;
    e->gateway = gw;
    e->iface = iface;
    e->metric = metric;
    e->flags = flags;

    return 0;
}

const struct nfs_rt_entry *nfs_rtable_lookup(const struct nfs_rtable *t, uint32_t dst) {
    const struct nfs_rt_entry *best = NULL;

    for (size_t i = 0; i < t->count; i++) {
        const struct nfs_rt_entry *e = &t->entries[i];

        /* Check if the route is up */
        if (!(e->flags & NFS_RTF_UP))
            continue;

        uint32_t mask = nfs_prefix_mask(e->prefix_len);
        if ((dst & mask) != e->dest)
            continue;

        /* This entry matches; check if it's better */
        if (!best) {
            best = e;
        } else if (e->prefix_len > best->prefix_len) {
            /* Longer prefix wins */
            best = e;
        } else if (e->prefix_len == best->prefix_len && e->metric < best->metric) {
            /* Same prefix length: lower metric wins */
            best = e;
        }
    }

    return best;
}

int nfs_rtable_remove(struct nfs_rtable *t, uint32_t dest, uint8_t prefix) {
    uint32_t mask = nfs_prefix_mask(prefix);
    uint32_t masked_dest = dest & mask;

    for (size_t i = 0; i < t->count; i++) {
        if (t->entries[i].dest == masked_dest && t->entries[i].prefix_len == prefix) {
            /* Swap with last and shrink */
            t->entries[i] = t->entries[t->count - 1];
            t->count--;
            return 0;
        }
    }

    return -1;
}

/* Format a single IPv4 address (host byte order) into dotted-decimal. */
static int fmt_ip4(uint32_t ip, char *buf, size_t sz) {
    return snprintf(buf, sz, "%u.%u.%u.%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF,
                    ip & 0xFF);
}

int nfs_rtable_format(const struct nfs_rtable *t, char *buf, size_t sz) {
    int total = 0;
    int n;

    for (size_t i = 0; i < t->count; i++) {
        const struct nfs_rt_entry *e = &t->entries[i];
        char dest[20], gw[20];
        fmt_ip4(e->dest, dest, sizeof(dest));
        fmt_ip4(e->gateway, gw, sizeof(gw));

        if (e->prefix_len == 0) {
            n = snprintf(buf + total, sz - (size_t)total, "default via %s dev if%d metric %u", gw,
                         e->iface, e->metric);
        } else if (e->flags & NFS_RTF_GATEWAY) {
            n = snprintf(buf + total, sz - (size_t)total, "%s/%u via %s dev if%d metric %u", dest,
                         e->prefix_len, gw, e->iface, e->metric);
        } else {
            n = snprintf(buf + total, sz - (size_t)total, "%s/%u dev if%d metric %u", dest,
                         e->prefix_len, e->iface, e->metric);
        }
        if (n < 0)
            return -1;
        total += n;

        if ((size_t)total < sz) {
            buf[total++] = '\n';
        }
    }

    if ((size_t)total < sz)
        buf[total] = '\0';

    return total;
}
