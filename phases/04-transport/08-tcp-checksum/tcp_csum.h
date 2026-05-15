#ifndef NFS_TCP_CSUM_H
#define NFS_TCP_CSUM_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * IPv4 pseudo-header for TCP checksum computation.
 *
 * Same structure as used for UDP, but protocol = 6 (TCP).
 *
 *    0      7 8     15 16    23 24    31
 *   +--------+--------+--------+--------+
 *   |           source address           |
 *   +--------+--------+--------+--------+
 *   |         destination address        |
 *   +--------+--------+--------+--------+
 *   |  zero  | proto  |   TCP length     |
 *   +--------+--------+--------+--------+
 *
 * 12 bytes total.  proto = 6 for TCP.
 * --------------------------------------------------------------- */

struct nfs_ipv4_pseudo_hdr {
    uint32_t src_addr;    /* source IP, network byte order      */
    uint32_t dst_addr;    /* destination IP, network byte order  */
    uint8_t  zero;        /* must be zero                        */
    uint8_t  protocol;    /* IP protocol number (6 = TCP)        */
    uint16_t tcp_length;  /* TCP segment length, network order   */
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

/* Compute the TCP checksum over the IPv4 pseudo-header + TCP segment.
 * src_ip, dst_ip: IPv4 addresses in NETWORK byte order.
 * tcp_segment: pointer to the complete TCP segment (header + payload).
 * tcp_len: total length of the TCP segment in bytes.
 * Returns the checksum in host byte order. */
uint16_t nfs_tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                          const uint8_t *tcp_segment, size_t tcp_len);

/* Verify TCP checksum.
 * Returns 1 if valid, 0 if invalid. */
int nfs_tcp_verify_checksum(uint32_t src_ip, uint32_t dst_ip,
                            const uint8_t *tcp_segment, size_t tcp_len);

/* Incremental checksum: accumulate partial sum over a buffer.
 * seed = 0 for the first call, or a previous partial sum.
 * Returns a 32-bit partial sum (carries not yet folded). */
uint16_t nfs_checksum_partial(const void *data, size_t len, uint32_t seed);

/* Fold a 32-bit partial sum into the final 16-bit checksum
 * with the bitwise NOT applied (RFC 1071). */
uint16_t nfs_checksum_fold(uint32_t sum);

#endif /* NFS_TCP_CSUM_H */
