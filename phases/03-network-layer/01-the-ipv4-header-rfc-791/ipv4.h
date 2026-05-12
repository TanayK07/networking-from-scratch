#ifndef NFS_IPV4_H
#define NFS_IPV4_H

#include <stddef.h>
#include <stdint.h>

/* Protocol numbers (RFC 790). */
#define NFS_IPPROTO_ICMP    1
#define NFS_IPPROTO_TCP     6
#define NFS_IPPROTO_UDP    17
#define NFS_IPPROTO_IPV6   41

/* IPv4 flags. */
#define NFS_IPV4_FLAG_MF   0x1   /* More Fragments */
#define NFS_IPV4_FLAG_DF   0x2   /* Don't Fragment */

/* IPv4 header — 20 bytes minimum (IHL=5, no options).
 *
 * All multi-byte fields are stored in HOST byte order after parsing
 * and must be in NETWORK byte order on the wire.  The parse/build
 * functions handle the conversion.
 */
struct nfs_ipv4_hdr {
    uint8_t  version;        /* 4 bits — must be 4              */
    uint8_t  ihl;            /* 4 bits — header length in 32-bit words */
    uint8_t  dscp;           /* 6 bits — Differentiated Services */
    uint8_t  ecn;            /* 2 bits — Explicit Congestion     */
    uint16_t total_length;   /* total datagram length in bytes   */
    uint16_t identification; /* fragment id                      */
    uint8_t  flags;          /* 3 bits — 0/DF/MF                */
    uint16_t frag_offset;    /* 13 bits — in 8-byte units        */
    uint8_t  ttl;            /* time to live                     */
    uint8_t  protocol;       /* upper-layer protocol (TCP/UDP/…) */
    uint16_t checksum;       /* header checksum                  */
    uint32_t src_addr;       /* source IP — host order after parse */
    uint32_t dst_addr;       /* dest IP   — host order after parse */
};

/* Return values. */
#define NFS_IPV4_OK            0
#define NFS_IPV4_ERR_SHORT    -1   /* buffer shorter than 20 bytes     */
#define NFS_IPV4_ERR_VERSION  -2   /* version field != 4               */
#define NFS_IPV4_ERR_IHL      -3   /* IHL < 5                          */
#define NFS_IPV4_ERR_CHECKSUM -4   /* header checksum mismatch         */

/* Parse raw wire bytes into a host-order struct.
 * Returns NFS_IPV4_OK on success, negative on error. */
int nfs_ipv4_parse(const uint8_t *buf, size_t len, struct nfs_ipv4_hdr *out);

/* Serialize a host-order struct into wire bytes.
 * Computes and fills in the checksum automatically.
 * Returns the number of bytes written, or 0 on error. */
size_t nfs_ipv4_build(const struct nfs_ipv4_hdr *hdr, uint8_t *buf, size_t len);

/* Compute the RFC 1071 checksum over the 20-byte header in `buf`.
 * `buf` must already be in wire format.  Returns checksum in host order. */
uint16_t nfs_ipv4_checksum(const uint8_t *buf, size_t hdr_len);

/* Format a host-order IPv4 address as dotted decimal into `buf`.
 * `buf` must be at least 16 bytes.  Returns `buf`. */
char *nfs_ipv4_format_addr(uint32_t addr, char *buf);

/* Return a human-readable name for a protocol number, e.g. "TCP".
 * Returns "unknown" for unrecognised values. */
const char *nfs_ipv4_protocol_name(uint8_t proto);

#endif /* NFS_IPV4_H */
