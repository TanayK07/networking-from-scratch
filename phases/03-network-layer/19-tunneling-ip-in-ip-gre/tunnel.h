#ifndef NFS_TUNNEL_H
#define NFS_TUNNEL_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * IP-in-IP Tunneling (RFC 2003, protocol 4)
 *
 * Encapsulates an inner IPv4 packet inside an outer IPv4 header.
 * The outer header uses protocol number 4 (IPPROTO_IPIP).
 *
 * GRE Tunneling (RFC 2784/2890, protocol 47)
 *
 * GRE header (4 bytes minimum):
 *   Bits 0-15:  C R K S s Recur(3) A Flags(4) Version(3)
 *   Bits 16-31: Protocol Type
 *   Optional:   Checksum(2)+Reserved(2) if C=1
 *               Key(4) if K=1
 *               Sequence(4) if S=1
 * --------------------------------------------------------------- */

#define NFS_IPPROTO_IPIP 4
#define NFS_IPPROTO_GRE  47

/* GRE flag bits (in the first 2 bytes, network byte order) */
#define NFS_GRE_FLAG_C 0x8000 /* Checksum present */
#define NFS_GRE_FLAG_K 0x2000 /* Key present */
#define NFS_GRE_FLAG_S 0x1000 /* Sequence number present */

/* Common GRE protocol types */
#define NFS_GRE_PROTO_IPV4 0x0800
#define NFS_GRE_PROTO_IPV6 0x86DD

/* ---- Minimal IPv4 header (20 bytes) ---- */

struct nfs_ipv4_hdr {
    uint8_t ver_ihl; /* version (4) | IHL (4) */
    uint8_t tos;
    uint16_t total_len;  /* network byte order */
    uint16_t id;         /* network byte order */
    uint16_t flags_frag; /* flags (3) | fragment offset (13) */
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum; /* network byte order */
    uint32_t src_addr; /* network byte order */
    uint32_t dst_addr; /* network byte order */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ipv4_hdr) == 20, "IPv4 header must be 20 bytes");

/* ---- GRE header (parsed) ---- */

struct nfs_gre_hdr {
    uint16_t flags;    /* C/K/S bits + version */
    uint16_t protocol; /* encapsulated protocol, host byte order */
    int has_checksum;
    uint16_t checksum; /* valid if has_checksum */
    int has_key;
    uint32_t key; /* valid if has_key */
    int has_seq;
    uint32_t seq;   /* valid if has_seq */
    size_t hdr_len; /* total GRE header size in bytes */
};

/* ---- IP-in-IP functions ---- */

/* Compute one's-complement checksum for IPv4 header. */
uint16_t nfs_ip_checksum(const void *data, size_t len);

/* Build an outer IPv4 header for IP-in-IP tunneling.
 * Wraps `inner` (inner_len bytes) with an outer IPv4 header.
 * Sets protocol to 4 (IPIP).
 * Returns total bytes written, or -1 on error. */
int nfs_ipip_encapsulate(uint32_t outer_src, uint32_t outer_dst, uint8_t ttl, const uint8_t *inner,
                         size_t inner_len, uint8_t *out, size_t out_sz);

/* Decapsulate an IP-in-IP packet.
 * Validates outer header, checks protocol == 4.
 * Returns pointer to inner packet and sets *inner_len, or NULL on error. */
const uint8_t *nfs_ipip_decapsulate(const uint8_t *pkt, size_t pkt_len, size_t *inner_len);

/* ---- GRE functions ---- */

/* Build a GRE header + encapsulated payload.
 * `flags` can include NFS_GRE_FLAG_C, NFS_GRE_FLAG_K, NFS_GRE_FLAG_S.
 * Returns total bytes written (GRE header + payload), or -1 on error. */
int nfs_gre_encapsulate(uint16_t flags, uint16_t protocol, uint32_t key, uint32_t seq,
                        const uint8_t *payload, size_t payload_len, uint8_t *out, size_t out_sz);

/* Parse a GRE header from raw bytes.
 * Fills `hdr` with parsed fields.
 * Returns 0 on success, -1 on error. */
int nfs_gre_parse(const uint8_t *data, size_t len, struct nfs_gre_hdr *hdr);

/* Get pointer to GRE payload (after the GRE header).
 * Returns pointer and sets *payload_len, or NULL on error. */
const uint8_t *nfs_gre_payload(const uint8_t *data, size_t len, size_t *payload_len);

/* Compute GRE checksum (one's complement over GRE header + payload). */
uint16_t nfs_gre_checksum(const uint8_t *data, size_t len);

#endif /* NFS_TUNNEL_H */
