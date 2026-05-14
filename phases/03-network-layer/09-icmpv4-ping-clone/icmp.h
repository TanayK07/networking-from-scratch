#ifndef NFS_ICMP_H
#define NFS_ICMP_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * ICMPv4 message header (RFC 792).
 *
 * The first 8 bytes are common to Echo Request / Echo Reply.
 * Other ICMP types reuse the same first 4 bytes (type, code,
 * checksum) but interpret bytes 4-7 differently. This struct
 * models the echo-specific layout used by ping.
 * --------------------------------------------------------------- */

struct nfs_icmp_hdr {
    uint8_t type;      /* message type                        */
    uint8_t code;      /* type-specific sub-code               */
    uint16_t checksum; /* one's complement checksum, net order */
    uint16_t id;       /* identifier (echo), network order     */
    uint16_t seq;      /* sequence number (echo), net order    */
} __attribute__((packed));

/* Compile-time guarantee: the struct matches the wire layout. */
_Static_assert(sizeof(struct nfs_icmp_hdr) == 8, "nfs_icmp_hdr must be exactly 8 bytes");

/* ---------------------------------------------------------------
 * ICMP type constants (RFC 792)
 * --------------------------------------------------------------- */

#define NFS_ICMP_ECHO_REPLY    0
#define NFS_ICMP_DEST_UNREACH  3
#define NFS_ICMP_ECHO_REQUEST  8
#define NFS_ICMP_TIME_EXCEEDED 11

/* ---------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

/* Parse raw bytes into an nfs_icmp_hdr struct. Multi-byte fields
 * are converted to host byte order in `out`.
 * Returns 0 on success, -1 on error (NULL pointer, truncated buf). */
int nfs_icmp_parse(const uint8_t *buf, size_t len, struct nfs_icmp_hdr *out);

/* Build an ICMP Echo Request packet in `buf`.
 * `id` and `seq` are in host byte order; they are converted to
 * network order in the packet. `payload` may be NULL if
 * `payload_len` is 0. The ICMP checksum is computed over the
 * entire message (header + payload).
 * Returns the number of bytes written, or 0 on error. */
size_t nfs_icmp_build_echo_request(uint16_t id, uint16_t seq, const uint8_t *payload,
                                   size_t payload_len, uint8_t *buf, size_t buf_len);

/* Validate the ICMP checksum of a raw ICMP message.
 * Returns 0 if the checksum is correct, -1 on error. */
int nfs_icmp_validate_checksum(const uint8_t *buf, size_t len);

/* Return a human-readable name for an ICMP type value.
 * Returns "Unknown" for unrecognized types. */
const char *nfs_icmp_type_name(uint8_t type);

#endif /* NFS_ICMP_H */
