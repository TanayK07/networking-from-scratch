#ifndef NFS_NETWORK_H
#define NFS_NETWORK_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * What is a Network — protocol header, packet building/parsing,
 * encapsulation/decapsulation, and the Internet checksum (RFC 1071).
 * --------------------------------------------------------------- */

/* Fixed 8-byte protocol header (wire format).
 *
 * Byte layout:
 *   [0]       version (high nibble) | type (low nibble)
 *   [1]       flags
 *   [2..3]    length  (total including header, network byte order)
 *   [4..7]    id      (packet identifier, network byte order)
 */
struct __attribute__((packed)) nfs_pkt_header {
    uint8_t  ver_type;   /* version:4 | type:4 */
    uint8_t  flags;
    uint16_t length;     /* network byte order */
    uint32_t id;         /* network byte order */
};

_Static_assert(sizeof(struct nfs_pkt_header) == 8,
               "Packet header must be 8 bytes");

#define NFS_PKT_HDR_SIZE  ((size_t)sizeof(struct nfs_pkt_header))
#define NFS_PKT_VERSION   1

/* --- Packet build / parse -------------------------------------- */

/* Serialize header + payload into buf (must be >= hdr_size + payload_len).
 * Multi-byte fields stored in network byte order (big-endian).
 * Returns total bytes written, or -1 on error. */
int nfs_pkt_build(uint8_t *buf, size_t buf_sz,
                  uint8_t version, uint8_t type, uint8_t flags,
                  uint32_t id,
                  const uint8_t *payload, size_t payload_len);

/* Parse buf into header fields + payload pointer + payload length.
 * Returns 0 on success, -1 on error (too short, length mismatch). */
int nfs_pkt_parse(const uint8_t *buf, size_t buf_len,
                  struct nfs_pkt_header *hdr,
                  const uint8_t **payload_out, size_t *payload_len_out);

/* --- Internet checksum (RFC 1071) ------------------------------ */

/* Compute the RFC 1071 Internet Checksum over data of len bytes.
 * Returns the 16-bit checksum in host byte order.
 * Key property: checksum(data || checksum) == 0. */
uint16_t nfs_inet_checksum(const uint8_t *data, size_t len);

/* --- Encapsulation / decapsulation ----------------------------- */

/* Wrap payload with a protocol header (encapsulation).
 * Returns total bytes written to buf, or -1 on error. */
int nfs_encapsulate(uint8_t *buf, size_t buf_sz,
                    uint8_t type, uint8_t flags, uint32_t id,
                    const uint8_t *payload, size_t payload_len);

/* Strip header and return pointer to payload (decapsulation).
 * Returns 0 on success, -1 on error. */
int nfs_decapsulate(const uint8_t *buf, size_t buf_len,
                    struct nfs_pkt_header *hdr,
                    const uint8_t **payload_out, size_t *payload_len_out);

#endif /* NFS_NETWORK_H */
