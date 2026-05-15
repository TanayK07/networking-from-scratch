#ifndef NFS_IPV6_H
#define NFS_IPV6_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * IPv6 fixed header — 40 bytes (RFC 8200).
 *
 * vtc_flow is stored in network byte order on the wire.
 * In host order it encodes:
 *   bits 31-28: version (must be 6)
 *   bits 27-20: traffic class
 *   bits 19-0:  flow label
 * --------------------------------------------------------------------------- */

struct nfs_ipv6_hdr {
    uint32_t vtc_flow;      /* version (4) | traffic class (8) | flow label (20) */
    uint16_t payload_len;   /* payload length (not including this header)         */
    uint8_t  next_hdr;      /* next header type                                  */
    uint8_t  hop_limit;     /* hop limit (analogous to TTL)                      */
    uint8_t  src[16];       /* source address — 128 bits                         */
    uint8_t  dst[16];       /* destination address — 128 bits                    */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ipv6_hdr) == 40,
               "nfs_ipv6_hdr must be exactly 40 bytes");

/* Extract the 4-bit version field (must be 6). */
uint8_t nfs_ipv6_version(const struct nfs_ipv6_hdr *h);

/* Extract the 8-bit traffic class. */
uint8_t nfs_ipv6_traffic_class(const struct nfs_ipv6_hdr *h);

/* Extract the 20-bit flow label. */
uint32_t nfs_ipv6_flow_label(const struct nfs_ipv6_hdr *h);

/* Parse raw bytes into an nfs_ipv6_hdr struct.
 * Validates: len >= 40 and version == 6.
 * Returns 0 on success, -1 on error. */
int nfs_ipv6_parse(const uint8_t *data, size_t len, struct nfs_ipv6_hdr *hdr);

/* Build a 40-byte IPv6 header into out.
 * Sets version = 6.
 * Returns 40 on success, -1 on error (e.g. out_sz < 40). */
int nfs_ipv6_build(uint8_t tc, uint32_t flow_label,
                   uint8_t next_hdr, uint8_t hop_limit,
                   const uint8_t src[16], const uint8_t dst[16],
                   uint16_t payload_len,
                   uint8_t *out, size_t out_sz);

/* Format a 128-bit IPv6 address as full form:
 *   "2001:0db8:0000:0000:0000:0000:0000:0001"
 * (8 groups of 4 hex digits separated by colons).
 * buf must be at least 40 bytes. Returns number of chars written. */
int nfs_ipv6_addr_format(const uint8_t addr[16], char *buf, size_t sz);

/* Parse a full-form IPv6 address string (8 colon-separated groups of
 * 4 hex digits) into a 16-byte address.
 * Returns 0 on success, -1 on error. */
int nfs_ipv6_addr_parse(const char *str, uint8_t addr[16]);

/* Return a human-readable name for a Next Header value. */
const char *nfs_ipv6_next_hdr_name(uint8_t nh);

#endif /* NFS_IPV6_H */
