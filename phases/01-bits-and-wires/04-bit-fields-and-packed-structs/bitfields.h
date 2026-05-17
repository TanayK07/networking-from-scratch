#ifndef NFS_BITFIELDS_H
#define NFS_BITFIELDS_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * IPv4 header manipulation via shift+mask (no C bitfields).
 *
 * The struct uses plain integer types in network byte order.
 * Accessor functions extract sub-byte fields using bit shifts
 * and masks, which is portable and well-defined (unlike C
 * bitfields whose layout is implementation-defined).
 *
 * Wire format (RFC 791):
 *   Byte 0:  [Version:4][IHL:4]
 *   Byte 1:  [DSCP:6][ECN:2]
 *   Bytes 2-3:  Total Length (network order)
 *   Bytes 4-5:  Identification (network order)
 *   Bytes 6-7:  [Flags:3][Fragment Offset:13] (network order)
 *   Byte 8:  TTL
 *   Byte 9:  Protocol
 *   Bytes 10-11: Header Checksum (network order)
 *   Bytes 12-15: Source Address
 *   Bytes 16-19: Destination Address
 * --------------------------------------------------------------- */

struct nfs_ipv4_hdr {
    uint8_t ver_ihl;     /* version (4) + IHL (4) */
    uint8_t tos;         /* DSCP (6) + ECN (2) */
    uint16_t total_len;  /* total length (network order) */
    uint16_t ident;      /* identification (network order) */
    uint16_t flags_frag; /* flags (3) + fragment offset (13), network order */
    uint8_t ttl;         /* time to live */
    uint8_t protocol;    /* protocol number */
    uint16_t checksum;   /* header checksum (network order) */
    uint32_t src_addr;   /* source IP (network order) */
    uint32_t dst_addr;   /* destination IP (network order) */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ipv4_hdr) == 20, "IPv4 header must be exactly 20 bytes");

/* ---- Field extraction (from network-order header) ---- */

uint8_t nfs_ipv4_version(const struct nfs_ipv4_hdr *h);
uint8_t nfs_ipv4_ihl(const struct nfs_ipv4_hdr *h);
uint8_t nfs_ipv4_dscp(const struct nfs_ipv4_hdr *h);
uint8_t nfs_ipv4_ecn(const struct nfs_ipv4_hdr *h);
uint16_t nfs_ipv4_flags(const struct nfs_ipv4_hdr *h);
uint16_t nfs_ipv4_frag_offset(const struct nfs_ipv4_hdr *h);

/* ---- Field setters ---- */

void nfs_ipv4_set_ver_ihl(struct nfs_ipv4_hdr *h, uint8_t ver, uint8_t ihl);

/* ---- Formatting ---- */

/* Write a human-readable summary of the header into buf.
 * Returns the number of characters written (excluding NUL). */
void nfs_ipv4_format(const struct nfs_ipv4_hdr *h, char *buf, size_t sz);

#endif /* NFS_BITFIELDS_H */
