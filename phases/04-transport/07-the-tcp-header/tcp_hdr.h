#ifndef NFS_TCP_HDR_H
#define NFS_TCP_HDR_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP header (RFC 9293, minimum 20 bytes).
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |          Source Port          |       Destination Port        |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                        Sequence Number                       |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                    Acknowledgment Number                     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |  Data |       |C|E|U|A|P|R|S|F|                               |
 *   | Offset| Rsrvd |W|C|R|C|S|S|Y|I|            Window             |
 *   |       |       |R|E|G|K|H|T|N|N|                               |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |           Checksum            |         Urgent Pointer        |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * The 16-bit field at offset 12 (data_off_flags) encodes:
 *   bits 15-12: Data Offset (header length in 32-bit words)
 *   bits 11-9:  Reserved (must be zero)
 *   bits 8-0:   Flags (CWR, ECE, URG, ACK, PSH, RST, SYN, FIN)
 * --------------------------------------------------------------- */

struct nfs_tcp_hdr {
    uint16_t src_port;       /* source port, network byte order      */
    uint16_t dst_port;       /* destination port, network byte order  */
    uint32_t seq;            /* sequence number, network byte order   */
    uint32_t ack;            /* acknowledgment number, network order  */
    uint16_t data_off_flags; /* data offset (4 bits) + reserved (3 bits)
                                + flags (9 bits), network byte order */
    uint16_t window;         /* window size, network byte order       */
    uint16_t checksum;       /* checksum, network byte order          */
    uint16_t urgent;         /* urgent pointer, network byte order    */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_tcp_hdr) == 20, "nfs_tcp_hdr must be exactly 20 bytes");

/* Minimum TCP header length (no options). */
#define NFS_TCP_HDR_MIN_LEN 20

/* ---------------------------------------------------------------
 * TCP flag constants (bottom 9 bits of the data_off_flags field).
 * --------------------------------------------------------------- */
#define NFS_TCP_FIN 0x001
#define NFS_TCP_SYN 0x002
#define NFS_TCP_RST 0x004
#define NFS_TCP_PSH 0x008
#define NFS_TCP_ACK 0x010
#define NFS_TCP_URG 0x020
#define NFS_TCP_ECE 0x040
#define NFS_TCP_CWR 0x080

/* ---------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

/* Extract the data offset (header length in 32-bit words)
 * from the data_off_flags field.  Returns 5-15. */
uint8_t nfs_tcp_data_offset(const struct nfs_tcp_hdr *h);

/* Extract the 9-bit flags field.  Input header is in network byte order. */
uint16_t nfs_tcp_flags(const struct nfs_tcp_hdr *h);

/* Check whether a specific flag is set. */
int nfs_tcp_has_flag(const struct nfs_tcp_hdr *h, uint16_t flag);

/* Parse raw wire bytes into a host-order nfs_tcp_hdr struct.
 * Validates that data_offset >= 5 and len >= 20.
 * Returns 0 on success, -1 on error. */
int nfs_tcp_parse(const uint8_t *data, size_t len, struct nfs_tcp_hdr *hdr);

/* Build a minimal 20-byte TCP header with no options.
 * Writes to `out` (must be >= out_sz bytes).
 * Returns header length (20) on success, -1 on error. */
int nfs_tcp_build(uint16_t src_port, uint16_t dst_port, uint32_t seq, uint32_t ack, uint16_t flags,
                  uint16_t window, uint8_t *out, size_t out_sz);

/* Format a parsed (host-order) TCP header into a human-readable string.
 * Example: "12345->80 seq=1000 ack=2000 [SYN,ACK] win=65535"
 * Writes at most sz bytes including NUL terminator. */
void nfs_tcp_format(const struct nfs_tcp_hdr *h, char *buf, size_t sz);

/* Return a static string representation of the flags.
 * Example: "SYN", "SYN,ACK", "FIN,ACK". */
const char *nfs_tcp_flag_str(uint16_t flags);

#endif /* NFS_TCP_HDR_H */
