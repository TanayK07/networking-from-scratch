#ifndef NFS_UDP_CSUM_H
#define NFS_UDP_CSUM_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * IPv4 pseudo-header (RFC 768, RFC 1071).
 *
 * Used when computing UDP (and TCP) checksums over IPv4.
 *
 *    0      7 8     15 16    23 24    31
 *   +--------+--------+--------+--------+
 *   |           source address           |
 *   +--------+--------+--------+--------+
 *   |         destination address        |
 *   +--------+--------+--------+--------+
 *   |  zero  | proto  |   UDP length     |
 *   +--------+--------+--------+--------+
 *
 * 12 bytes total.  proto = 17 for UDP.
 * --------------------------------------------------------------- */

struct nfs_ipv4_pseudo_hdr {
    uint32_t src_addr;   /* source IP, network byte order      */
    uint32_t dst_addr;   /* destination IP, network byte order  */
    uint8_t zero;        /* must be zero                        */
    uint8_t protocol;    /* IP protocol number (17 = UDP)       */
    uint16_t udp_length; /* UDP length, network byte order      */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ipv4_pseudo_hdr) == 12,
               "nfs_ipv4_pseudo_hdr must be exactly 12 bytes");

/* ---------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

/* RFC 1071 Internet checksum.
 * Computes the 16-bit one's-complement checksum of `len` bytes.
 * Returns the result in host byte order. */
uint16_t nfs_internet_checksum(const void *data, size_t len);

/* Compute the UDP checksum over the IPv4 pseudo-header + UDP segment.
 * src_ip, dst_ip: source and destination IPv4 addresses in NETWORK byte order.
 * udp_segment: pointer to the complete UDP segment (header + payload).
 * udp_len: length of the UDP segment in bytes.
 *
 * Per RFC 768: if the computed checksum is 0, return 0xFFFF.
 * Returns the checksum in host byte order. */
uint16_t nfs_udp_checksum(uint32_t src_ip, uint32_t dst_ip, const uint8_t *udp_segment,
                          size_t udp_len);

/* Verify UDP checksum.
 * Returns 1 if valid, 0 if invalid.
 * Per RFC 768: checksum == 0 on the wire means "not computed" (always valid). */
int nfs_udp_verify_checksum(uint32_t src_ip, uint32_t dst_ip, const uint8_t *udp_segment,
                            size_t udp_len);

#endif /* NFS_UDP_CSUM_H */
