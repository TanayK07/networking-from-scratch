#ifndef NFS_H2FRAME_H
#define NFS_H2FRAME_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * HTTP/2 Binary Framing (RFC 7540)
 *
 * Frame layout (9-byte header + payload):
 *   +-----------------------------------------------+
 *   |                 Length (24 bits)               |
 *   +---------------+---------------+---------------+
 *   |   Type (8)    |   Flags (8)   |
 *   +-+-------------+---------------+---------------+
 *   |R|         Stream Identifier (31 bits)         |
 *   +=+=============================================+
 *   |                Frame Payload ...               |
 *   +-----------------------------------------------+
 * --------------------------------------------------------------- */

#define NFS_H2_FRAME_HEADER_SIZE  9

/* Frame types */
#define NFS_H2_DATA           0
#define NFS_H2_HEADERS        1
#define NFS_H2_PRIORITY       2
#define NFS_H2_RST_STREAM     3
#define NFS_H2_SETTINGS       4
#define NFS_H2_PUSH_PROMISE   5
#define NFS_H2_PING           6
#define NFS_H2_GOAWAY         7
#define NFS_H2_WINDOW_UPDATE  8
#define NFS_H2_CONTINUATION   9

/* Common flags */
#define NFS_H2_FLAG_END_STREAM   0x01
#define NFS_H2_FLAG_END_HEADERS  0x04
#define NFS_H2_FLAG_PADDED       0x08
#define NFS_H2_FLAG_PRIORITY     0x20
#define NFS_H2_FLAG_ACK          0x01

/* Settings identifiers */
#define NFS_H2_SETTINGS_HEADER_TABLE_SIZE      0x01
#define NFS_H2_SETTINGS_ENABLE_PUSH            0x02
#define NFS_H2_SETTINGS_MAX_CONCURRENT_STREAMS 0x03
#define NFS_H2_SETTINGS_INITIAL_WINDOW_SIZE    0x04
#define NFS_H2_SETTINGS_MAX_FRAME_SIZE         0x05
#define NFS_H2_SETTINGS_MAX_HEADER_LIST_SIZE   0x06

/* Maximum payload length (default) */
#define NFS_H2_DEFAULT_MAX_FRAME_SIZE  16384

/* Parsed frame structure */
struct nfs_h2_frame {
    uint32_t length;      /* payload length (24-bit) */
    uint8_t  type;        /* frame type */
    uint8_t  flags;       /* frame flags */
    uint32_t stream_id;   /* 31-bit stream identifier (R bit masked) */
    const uint8_t *payload; /* pointer into original buffer */
};

/* Single SETTINGS entry */
struct nfs_h2_setting {
    uint16_t id;
    uint32_t value;
};

/* Parse a frame from wire bytes.
 * buf/len: input buffer.
 * out: parsed frame (payload points into buf).
 * Returns 0 on success, -1 on error (truncated, etc.). */
int nfs_h2_frame_parse(const uint8_t *buf, size_t len,
                       struct nfs_h2_frame *out);

/* Build a frame into buf (max out_sz bytes).
 * Returns total bytes written (header + payload), or -1 on error. */
int nfs_h2_frame_build(uint8_t *buf, size_t out_sz,
                       uint8_t type, uint8_t flags, uint32_t stream_id,
                       const uint8_t *payload, uint32_t payload_len);

/* Return human-readable name for a frame type, e.g. "DATA", "HEADERS".
 * Returns "UNKNOWN" for unrecognized types. */
const char *nfs_h2_frame_type_name(uint8_t type);

/* Parse SETTINGS payload into array of setting entries.
 * payload/len: the frame payload (must be multiple of 6 bytes).
 * out: array to fill.  max: max entries in out.
 * Returns number of entries parsed, or -1 on error. */
int nfs_h2_settings_parse(const uint8_t *payload, size_t len,
                          struct nfs_h2_setting *out, size_t max);

/* Build a SETTINGS payload from array of setting entries.
 * Returns bytes written to buf, or -1 on error. */
int nfs_h2_settings_build(uint8_t *buf, size_t out_sz,
                          const struct nfs_h2_setting *settings, size_t count);

#endif /* NFS_H2FRAME_H */
