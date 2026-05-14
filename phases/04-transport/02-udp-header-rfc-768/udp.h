#ifndef NFS_UDP_H
#define NFS_UDP_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * UDP datagram header (RFC 768).
 *
 * The UDP header is exactly 8 bytes:
 *
 *    0      7 8     15 16    23 24    31
 *   +--------+--------+--------+--------+
 *   |   Source Port    |  Dest Port      |
 *   +--------+--------+--------+--------+
 *   |     Length       |   Checksum      |
 *   +--------+--------+--------+--------+
 *   |          Payload ...               |
 *   +------------------------------------+
 *
 * Length = header (8) + payload.  Minimum value is 8 (empty payload).
 * Checksum is optional in IPv4 (0x0000 means "not computed"),
 * mandatory in IPv6.
 * --------------------------------------------------------------- */

struct nfs_udp_hdr {
    uint16_t src_port; /* source port, network order on wire */
    uint16_t dst_port; /* destination port, network order on wire */
    uint16_t length;   /* UDP header + payload length, network order on wire */
    uint16_t checksum; /* UDP checksum, network order on wire */
} __attribute__((packed));

/* Compile-time guarantee: the struct matches the wire layout. */
_Static_assert(sizeof(struct nfs_udp_hdr) == 8, "nfs_udp_hdr must be exactly 8 bytes");

/* Minimum UDP datagram length (header only, no payload). */
#define NFS_UDP_HDR_LEN 8

/* ---------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

/* Parse raw wire bytes into a host-order nfs_udp_hdr struct.
 * Returns 0 on success, -1 on error (truncated, length < 8, etc.). */
int nfs_udp_parse(const uint8_t *buf, size_t len, struct nfs_udp_hdr *out);

/* Build a UDP datagram: write the header in network order followed by
 * the payload.  Returns total bytes written, or 0 on error. */
size_t nfs_udp_build(const struct nfs_udp_hdr *hdr, const uint8_t *payload, size_t payload_len,
                     uint8_t *buf, size_t buf_len);

/* Compute the UDP checksum with an IPv4 pseudo-header (RFC 768).
 * src_ip and dst_ip are 4 bytes each in network byte order.
 * udp_buf points to the complete UDP datagram (header + payload)
 * in wire format.
 *
 * Per RFC 768: if the computed checksum is 0, returns 0xFFFF.
 * Returns the checksum in host byte order. */
uint16_t nfs_udp_checksum_ipv4(const uint8_t *src_ip, const uint8_t *dst_ip, const uint8_t *udp_buf,
                               size_t udp_len);

/* Validate a UDP datagram's checksum against the IPv4 pseudo-header.
 * Returns 0 if the checksum is valid (or was not computed, i.e. 0x0000),
 * -1 if the checksum is invalid. */
int nfs_udp_validate(const uint8_t *src_ip, const uint8_t *dst_ip, const uint8_t *udp_buf,
                     size_t udp_len);

/* Format a parsed (host-order) UDP header into a human-readable string.
 * Writes at most buf_len bytes (including NUL terminator). */
void nfs_udp_format(const struct nfs_udp_hdr *hdr, char *buf, size_t buf_len);

#endif /* NFS_UDP_H */
