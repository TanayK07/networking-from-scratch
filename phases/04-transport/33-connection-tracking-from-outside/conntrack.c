/*
 * conntrack.c -- Connection tracking table
 *
 * Tracks TCP/UDP connections by 5-tuple (protocol, src_ip, dst_ip,
 * src_port, dst_port). Classifies packets as NEW, ESTABLISHED, or
 * INVALID. Supports timeout-based expiry of idle connections.
 */

#include "conntrack.h"
#include <string.h>

int nfs_conntrack_init(struct nfs_conntrack *ct, int timeout) {
    if (!ct)
        return -1;
    memset(ct, 0, sizeof(*ct));
    ct->timeout = timeout > 0 ? timeout : NFS_CT_DEFAULT_TIMEOUT;
    return 0;
}

/* Check if two tuples match (either direction). */
static int tuple_match(const struct nfs_ct_tuple *a, const struct nfs_ct_tuple *b) {
    /* Forward match */
    if (a->protocol == b->protocol && a->src_ip == b->src_ip && a->dst_ip == b->dst_ip &&
        a->src_port == b->src_port && a->dst_port == b->dst_port)
        return 1;

    /* Reverse match (reply direction) */
    if (a->protocol == b->protocol && a->src_ip == b->dst_ip && a->dst_ip == b->src_ip &&
        a->src_port == b->dst_port && a->dst_port == b->src_port)
        return 1;

    return 0;
}

/* Find an existing entry that matches the packet, checking both directions. */
static struct nfs_ct_entry *find_entry(struct nfs_conntrack *ct, const struct nfs_ct_tuple *tuple) {
    for (int i = 0; i < NFS_CT_MAX_ENTRIES; i++) {
        if (ct->entries[i].active && tuple_match(&ct->entries[i].tuple, tuple))
            return &ct->entries[i];
    }
    return NULL;
}

/* Find a free slot. */
static struct nfs_ct_entry *find_free(struct nfs_conntrack *ct) {
    for (int i = 0; i < NFS_CT_MAX_ENTRIES; i++) {
        if (!ct->entries[i].active)
            return &ct->entries[i];
    }
    return NULL;
}

nfs_ct_state_t nfs_conntrack_process(struct nfs_conntrack *ct, const struct nfs_ct_packet *pkt,
                                     time_t now) {
    if (!ct || !pkt)
        return NFS_CT_INVALID;

    struct nfs_ct_tuple tuple = {.src_ip = pkt->src_ip,
                                 .dst_ip = pkt->dst_ip,
                                 .src_port = pkt->src_port,
                                 .dst_port = pkt->dst_port,
                                 .protocol = pkt->protocol};

    /* Look up existing entry */
    struct nfs_ct_entry *entry = find_entry(ct, &tuple);

    if (entry) {
        /* Existing connection */
        entry->last_seen = now;
        entry->packets++;
        entry->bytes += pkt->length;

        if (pkt->protocol == 6) { /* TCP */
            /* RST terminates the connection */
            if (pkt->tcp_flags & NFS_CT_TCP_RST) {
                entry->active = 0;
                ct->count--;
                return NFS_CT_INVALID;
            }
            /* SYN on established connection is invalid */
            if (entry->state == NFS_CT_ESTABLISHED && (pkt->tcp_flags & NFS_CT_TCP_SYN) &&
                !(pkt->tcp_flags & NFS_CT_TCP_ACK)) {
                return NFS_CT_INVALID;
            }
            /* SYN+ACK or ACK on NEW -> ESTABLISHED */
            if (entry->state == NFS_CT_NEW && (pkt->tcp_flags & NFS_CT_TCP_ACK)) {
                entry->state = NFS_CT_ESTABLISHED;
                return NFS_CT_ESTABLISHED;
            }
        } else {
            /* UDP: any reply packet promotes to ESTABLISHED */
            if (entry->state == NFS_CT_NEW) {
                /* Check if this is the reply direction */
                if (pkt->src_ip == entry->tuple.dst_ip && pkt->dst_ip == entry->tuple.src_ip) {
                    entry->state = NFS_CT_ESTABLISHED;
                    return NFS_CT_ESTABLISHED;
                }
            }
        }

        return entry->state;
    }

    /* No existing entry: create new */
    if (pkt->protocol == 6) {
        /* TCP: only SYN (without ACK) can create a new connection */
        if (!(pkt->tcp_flags & NFS_CT_TCP_SYN) || (pkt->tcp_flags & NFS_CT_TCP_ACK)) {
            return NFS_CT_INVALID;
        }
    }

    struct nfs_ct_entry *slot = find_free(ct);
    if (!slot)
        return NFS_CT_INVALID; /* table full */

    memset(slot, 0, sizeof(*slot));
    slot->tuple = tuple;
    slot->state = NFS_CT_NEW;
    slot->last_seen = now;
    slot->packets = 1;
    slot->bytes = pkt->length;
    slot->active = 1;
    ct->count++;

    return NFS_CT_NEW;
}

const struct nfs_ct_entry *nfs_conntrack_lookup(const struct nfs_conntrack *ct,
                                                const struct nfs_ct_tuple *tuple) {
    if (!ct || !tuple)
        return NULL;

    for (int i = 0; i < NFS_CT_MAX_ENTRIES; i++) {
        if (ct->entries[i].active && tuple_match(&ct->entries[i].tuple, tuple))
            return &ct->entries[i];
    }
    return NULL;
}

int nfs_conntrack_expire(struct nfs_conntrack *ct, time_t now) {
    if (!ct)
        return -1;

    int expired = 0;
    for (int i = 0; i < NFS_CT_MAX_ENTRIES; i++) {
        if (ct->entries[i].active && (now - ct->entries[i].last_seen) >= ct->timeout) {
            ct->entries[i].active = 0;
            ct->count--;
            expired++;
        }
    }
    return expired;
}

int nfs_conntrack_count(const struct nfs_conntrack *ct) {
    if (!ct)
        return 0;
    return ct->count;
}

const char *nfs_ct_state_name(nfs_ct_state_t state) {
    switch (state) {
    case NFS_CT_NEW:
        return "NEW";
    case NFS_CT_ESTABLISHED:
        return "ESTABLISHED";
    case NFS_CT_RELATED:
        return "RELATED";
    case NFS_CT_INVALID:
        return "INVALID";
    default:
        return "UNKNOWN";
    }
}
