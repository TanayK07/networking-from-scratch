#ifndef NFS_CONNTRACK_H
#define NFS_CONNTRACK_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* ---------------------------------------------------------------
 * Connection Tracking (conntrack)
 *
 * Tracks TCP connections by 5-tuple (protocol, src_ip, dst_ip,
 * src_port, dst_port) and classifies each packet into one of:
 *   NEW, ESTABLISHED, RELATED, INVALID
 *
 * Idle connections are expired via a configurable timeout.
 * --------------------------------------------------------------- */

/* Connection states. */
typedef enum {
    NFS_CT_NEW         = 0,
    NFS_CT_ESTABLISHED = 1,
    NFS_CT_RELATED     = 2,
    NFS_CT_INVALID     = 3
} nfs_ct_state_t;

/* TCP flags we care about for state tracking. */
#define NFS_CT_TCP_SYN  0x02
#define NFS_CT_TCP_ACK  0x10
#define NFS_CT_TCP_FIN  0x01
#define NFS_CT_TCP_RST  0x04

/* 5-tuple identifying a connection. */
struct nfs_ct_tuple {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  protocol;   /* IPPROTO_TCP = 6, IPPROTO_UDP = 17 */
};

/* A single connection entry. */
struct nfs_ct_entry {
    struct nfs_ct_tuple tuple;
    nfs_ct_state_t      state;
    time_t              last_seen;   /* timestamp of last packet */
    uint64_t            packets;     /* packet count */
    uint64_t            bytes;       /* byte count */
    int                 active;      /* 1 = in use, 0 = free slot */
};

/* Packet descriptor passed to the tracker. */
struct nfs_ct_packet {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  protocol;
    uint8_t  tcp_flags;  /* only meaningful for TCP */
    uint16_t length;     /* total packet length */
};

#define NFS_CT_MAX_ENTRIES 256
#define NFS_CT_DEFAULT_TIMEOUT 300  /* 5 minutes */

/* Connection tracking table. */
struct nfs_conntrack {
    struct nfs_ct_entry entries[NFS_CT_MAX_ENTRIES];
    int                 count;      /* number of active entries */
    int                 timeout;    /* seconds before idle expiry */
};

/* Initialize a conntrack table.
 * timeout: idle timeout in seconds (0 = use default 300s).
 * Returns 0 on success, -1 on error. */
int nfs_conntrack_init(struct nfs_conntrack *ct, int timeout);

/* Process a packet: classify it and update the conntrack table.
 * Returns the connection state (NEW, ESTABLISHED, INVALID, etc.)
 * or NFS_CT_INVALID on error. */
nfs_ct_state_t nfs_conntrack_process(struct nfs_conntrack *ct,
                                     const struct nfs_ct_packet *pkt,
                                     time_t now);

/* Look up an existing connection by 5-tuple.
 * Returns pointer to the entry, or NULL if not found. */
const struct nfs_ct_entry *nfs_conntrack_lookup(
    const struct nfs_conntrack *ct,
    const struct nfs_ct_tuple *tuple);

/* Expire all connections that have been idle for longer than the timeout.
 * Returns the number of entries expired. */
int nfs_conntrack_expire(struct nfs_conntrack *ct, time_t now);

/* Return the number of active connections. */
int nfs_conntrack_count(const struct nfs_conntrack *ct);

/* Return the state name as a string. */
const char *nfs_ct_state_name(nfs_ct_state_t state);

#endif /* NFS_CONNTRACK_H */
