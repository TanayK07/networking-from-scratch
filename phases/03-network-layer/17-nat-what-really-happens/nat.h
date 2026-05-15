#ifndef NFS_NAT_H
#define NFS_NAT_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Network Address Translation (NAT)
 *
 * NAT rewrites source IP/port on outbound packets so that many
 * internal hosts share a single external IP (NAPT / PAT).  A
 * translation table maps (internal_ip, internal_port, remote_ip,
 * remote_port, protocol) to an allocated external port.  Inbound
 * replies are reverse-translated using the same table.
 *
 * This module implements:
 *  - Outbound translation: allocate an external port, record mapping.
 *  - Inbound reverse lookup: find original internal IP/port.
 *  - Entry expiration: remove stale entries after a timeout.
 * --------------------------------------------------------------- */

/* Common protocol numbers */
#define NFS_PROTO_TCP  6
#define NFS_PROTO_UDP 17

/* A single NAT translation entry (5-tuple + external port). */
struct nfs_nat_entry {
    uint32_t internal_ip;
    uint16_t internal_port;
    uint32_t external_ip;
    uint16_t external_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint8_t  protocol;
    double   last_used;
};

/* NAT translation table. */
struct nfs_nat_table {
    struct nfs_nat_entry *entries;
    size_t   count;
    size_t   capacity;
    uint32_t external_ip;
    uint16_t next_port;    /* next external port to allocate */
};

/* Initialize a NAT table.
 * `capacity` — max entries.
 * `external_ip` — the public IP for outbound packets.
 * `port_start` — first ephemeral port to allocate. */
void nfs_nat_init(struct nfs_nat_table *t, size_t capacity,
                  uint32_t external_ip, uint16_t port_start);

/* Free all resources held by the NAT table. */
void nfs_nat_free(struct nfs_nat_table *t);

/* Translate an outbound packet.
 * Creates a new mapping or reuses an existing one for the same flow.
 * On success, writes the translated source IP/port to *new_src_ip
 * and *new_src_port.  Returns 0 on success, -1 if table is full. */
int nfs_nat_outbound(struct nfs_nat_table *t,
                     uint32_t src_ip,  uint16_t src_port,
                     uint32_t dst_ip,  uint16_t dst_port,
                     uint8_t  proto,
                     uint32_t *new_src_ip, uint16_t *new_src_port);

/* Reverse-translate an inbound reply.
 * Looks up a mapping by (src_ip, src_port, dst_port, proto) where
 * dst_port is the external port allocated by NAT.
 * On success, writes the original destination to *orig_dst_ip and
 * *orig_dst_port.  Returns 0 if found, -1 if not. */
int nfs_nat_inbound(struct nfs_nat_table *t,
                    uint32_t src_ip,  uint16_t src_port,
                    uint16_t dst_port, uint8_t proto,
                    uint32_t *orig_dst_ip, uint16_t *orig_dst_port);

/* Expire entries older than `timeout` seconds relative to `now`.
 * Returns the number of entries removed. */
int nfs_nat_expire(struct nfs_nat_table *t, double now, double timeout);

/* Format the NAT table into a human-readable string. */
void nfs_nat_format(const struct nfs_nat_table *t, char *buf, size_t sz);

#endif /* NFS_NAT_H */
