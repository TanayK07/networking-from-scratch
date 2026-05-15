/* Network Address Translation — implementation. */

#include "nat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void nfs_nat_init(struct nfs_nat_table *t, size_t capacity,
                  uint32_t external_ip, uint16_t port_start)
{
    t->entries     = calloc(capacity, sizeof(struct nfs_nat_entry));
    t->count       = 0;
    t->capacity    = capacity;
    t->external_ip = external_ip;
    t->next_port   = port_start;
}

void nfs_nat_free(struct nfs_nat_table *t)
{
    free(t->entries);
    t->entries  = NULL;
    t->count    = 0;
    t->capacity = 0;
}

/* Find an existing mapping for the same 5-tuple. */
static struct nfs_nat_entry *find_entry(struct nfs_nat_table *t,
                                        uint32_t src_ip,  uint16_t src_port,
                                        uint32_t dst_ip,  uint16_t dst_port,
                                        uint8_t  proto)
{
    for (size_t i = 0; i < t->count; i++) {
        struct nfs_nat_entry *e = &t->entries[i];
        if (e->internal_ip   == src_ip  &&
            e->internal_port == src_port &&
            e->remote_ip     == dst_ip   &&
            e->remote_port   == dst_port &&
            e->protocol      == proto)
            return e;
    }
    return NULL;
}

int nfs_nat_outbound(struct nfs_nat_table *t,
                     uint32_t src_ip,  uint16_t src_port,
                     uint32_t dst_ip,  uint16_t dst_port,
                     uint8_t  proto,
                     uint32_t *new_src_ip, uint16_t *new_src_port)
{
    /* Check for existing mapping (same flow reuses port). */
    struct nfs_nat_entry *e = find_entry(t, src_ip, src_port,
                                         dst_ip, dst_port, proto);
    if (e) {
        *new_src_ip   = e->external_ip;
        *new_src_port = e->external_port;
        return 0;
    }

    /* Create new mapping. */
    if (t->count >= t->capacity)
        return -1;

    e = &t->entries[t->count++];
    e->internal_ip   = src_ip;
    e->internal_port = src_port;
    e->external_ip   = t->external_ip;
    e->external_port = t->next_port++;
    e->remote_ip     = dst_ip;
    e->remote_port   = dst_port;
    e->protocol      = proto;
    e->last_used     = 0.0;

    *new_src_ip   = e->external_ip;
    *new_src_port = e->external_port;
    return 0;
}

int nfs_nat_inbound(struct nfs_nat_table *t,
                    uint32_t src_ip,  uint16_t src_port,
                    uint16_t dst_port, uint8_t proto,
                    uint32_t *orig_dst_ip, uint16_t *orig_dst_port)
{
    for (size_t i = 0; i < t->count; i++) {
        struct nfs_nat_entry *e = &t->entries[i];
        if (e->external_port == dst_port &&
            e->remote_ip     == src_ip   &&
            e->remote_port   == src_port &&
            e->protocol      == proto) {
            *orig_dst_ip   = e->internal_ip;
            *orig_dst_port = e->internal_port;
            return 0;
        }
    }
    return -1;  /* no matching entry */
}

int nfs_nat_expire(struct nfs_nat_table *t, double now, double timeout)
{
    int removed = 0;
    size_t i = 0;
    while (i < t->count) {
        if ((now - t->entries[i].last_used) >= timeout) {
            /* Swap with last entry to remove in O(1). */
            t->entries[i] = t->entries[t->count - 1];
            t->count--;
            removed++;
        } else {
            i++;
        }
    }
    return removed;
}

static void format_ip(char *buf, size_t sz, uint32_t ip)
{
    snprintf(buf, sz, "%u.%u.%u.%u",
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
             (ip >> 8)  & 0xFF,  ip        & 0xFF);
}

void nfs_nat_format(const struct nfs_nat_table *t, char *buf, size_t sz)
{
    size_t off = 0;
    int n = snprintf(buf + off, sz - off,
                     "NAT Table (%zu/%zu entries):\n",
                     t->count, t->capacity);
    if (n < 0) return;
    off += (size_t)n;

    for (size_t i = 0; i < t->count && off < sz; i++) {
        const struct nfs_nat_entry *e = &t->entries[i];
        char int_ip[16], ext_ip[16], rem_ip[16];
        format_ip(int_ip, sizeof(int_ip), e->internal_ip);
        format_ip(ext_ip, sizeof(ext_ip), e->external_ip);
        format_ip(rem_ip, sizeof(rem_ip), e->remote_ip);

        n = snprintf(buf + off, sz - off,
                     "  %s:%u -> %s:%u  (remote %s:%u, proto %u)\n",
                     int_ip, e->internal_port,
                     ext_ip, e->external_port,
                     rem_ip, e->remote_port,
                     e->protocol);
        if (n < 0) return;
        off += (size_t)n;
    }
}
