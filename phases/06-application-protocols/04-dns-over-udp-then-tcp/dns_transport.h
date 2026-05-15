#ifndef NFS_DNS_TRANSPORT_H
#define NFS_DNS_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * DNS over UDP then TCP.
 *
 * UDP transport: single datagram per DNS message, traditional
 * 512-byte limit.
 *
 * TCP transport: each DNS message is prefixed with a 2-byte
 * length field (network byte order).
 *
 * TC (truncation) bit in the DNS header triggers a TCP retry.
 * --------------------------------------------------------------- */

/* Traditional UDP message size limit (RFC 1035) */
#define NFS_DNS_UDP_MAX     512

/* Maximum TCP DNS message size (practical limit) */
#define NFS_DNS_TCP_MAX     65535

/* DNS header size */
#define NFS_DNS_HDR_SIZE    12

/* TCP length prefix size */
#define NFS_DNS_TCP_PREFIX  2

/* DNS header (wire format, network byte order). */
struct __attribute__((packed)) nfs_dns_transport_hdr {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

_Static_assert(sizeof(struct nfs_dns_transport_hdr) == 12,
               "DNS header must be 12 bytes");

/* --- TCP framing ------------------------------------------------ */

/* Add a 2-byte TCP length prefix to a DNS message.
 * Writes the framed message to `out` (2 + msg_len bytes).
 * Returns the total framed length, or -1 on error. */
int nfs_dns_tcp_frame(const uint8_t *msg, uint16_t msg_len,
                      uint8_t *out, size_t out_sz);

/* Extract a DNS message from a TCP-framed buffer.
 * Reads the 2-byte length prefix, validates it, and sets
 * `msg_out` to point within `data` and sets `msg_len` to the
 * message length.
 * Returns the total bytes consumed (2 + message length), or
 * -1 on error (including incomplete data). */
int nfs_dns_tcp_unframe(const uint8_t *data, size_t data_len,
                        const uint8_t **msg_out, uint16_t *msg_len);

/* --- Truncation detection --------------------------------------- */

/* Returns non-zero if the TC (truncation) bit is set in the
 * DNS header flags. */
int nfs_dns_is_truncated(const uint8_t *msg, size_t msg_len);

/* Set the TC bit in a DNS message header.
 * Returns 0 on success, -1 if the message is too short. */
int nfs_dns_set_truncated(uint8_t *msg, size_t msg_len);

/* Clear the TC bit in a DNS message header.
 * Returns 0 on success, -1 if the message is too short. */
int nfs_dns_clear_truncated(uint8_t *msg, size_t msg_len);

/* --- Message size validation ------------------------------------ */

/* Returns non-zero if the message fits within the UDP limit. */
int nfs_dns_fits_udp(size_t msg_len);

/* Returns non-zero if the message fits within TCP max. */
int nfs_dns_fits_tcp(size_t msg_len);

/* Truncate a DNS response to fit within `max_size` bytes.
 * Reduces ANCOUNT/NSCOUNT/ARCOUNT as needed, sets TC bit.
 * Returns the new message length, or -1 on error.
 * NOTE: This is a simplified truncation that only adjusts
 * the header counts and sets TC -- the caller must handle
 * the actual section truncation. For this lesson we implement
 * a simpler approach: just keep the header + question. */
int nfs_dns_truncate_response(const uint8_t *msg, size_t msg_len,
                              uint16_t max_size,
                              uint8_t *out, size_t out_sz);

/* --- Multi-message TCP stream parsing --------------------------- */

/* Parse multiple TCP-framed DNS messages from a buffer.
 * For each message found, stores the offset and length.
 * Returns the number of complete messages found, or -1 on error.
 * `offsets` and `lengths` arrays must have at least `max_msgs` entries. */
int nfs_dns_tcp_parse_stream(const uint8_t *data, size_t data_len,
                             size_t *offsets, uint16_t *lengths,
                             int max_msgs);

#endif /* NFS_DNS_TRANSPORT_H */
