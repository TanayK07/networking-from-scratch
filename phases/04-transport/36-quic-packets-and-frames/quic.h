#ifndef NFS_QUIC_H
#define NFS_QUIC_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * QUIC Packets and Frames (RFC 9000)
 *
 * Variable-Length Integer Encoding (Section 16):
 *   The 2 MSBs of the first byte encode the length:
 *     00 -> 1 byte  (6-bit value, max 63)
 *     01 -> 2 bytes (14-bit value, max 16383)
 *     10 -> 4 bytes (30-bit value, max 1073741823)
 *     11 -> 8 bytes (62-bit value, max 4611686018427387903)
 *
 * Long Header (first byte bit 7 = 1, bit 6 = 1):
 *   First byte | Version (4B) | DCID Len (1B) | DCID | SCID Len (1B) | SCID
 *
 * Short Header (first byte bit 7 = 0):
 *   First byte | DCID (known length)
 *   Bits: 0=header form, 1=fixed(1), 2=spin, 3=reserved, 4=reserved,
 *         5=key phase, 6-7=packet number length
 *
 * Frame Types:
 *   0x00 = PADDING, 0x01 = PING, 0x02 = ACK, 0x08-0x0f = STREAM
 * --------------------------------------------------------------- */

/* QUIC version */
#define NFS_QUIC_VERSION_1  0x00000001

/* Header form bit */
#define NFS_QUIC_HEADER_FORM_BIT  0x80
#define NFS_QUIC_FIXED_BIT        0x40

/* Long header packet types (bits 4-5 of first byte) */
#define NFS_QUIC_LONG_TYPE_INITIAL    0x00
#define NFS_QUIC_LONG_TYPE_0RTT       0x01
#define NFS_QUIC_LONG_TYPE_HANDSHAKE  0x02
#define NFS_QUIC_LONG_TYPE_RETRY      0x03

/* Short header bits */
#define NFS_QUIC_SHORT_SPIN_BIT      0x20
#define NFS_QUIC_SHORT_KEY_PHASE_BIT 0x04

/* Frame types */
#define NFS_QUIC_FRAME_PADDING  0x00
#define NFS_QUIC_FRAME_PING     0x01
#define NFS_QUIC_FRAME_ACK      0x02
#define NFS_QUIC_FRAME_ACK_ECN  0x03
#define NFS_QUIC_FRAME_STREAM   0x08  /* 0x08-0x0f */

/* Max connection ID length */
#define NFS_QUIC_MAX_CID_LEN  20

/* ---- Variable-length integer ---- */

/* Encode a variable-length integer into `out`.
 * Returns bytes written (1, 2, 4, or 8), or -1 if value too large or buffer too small. */
int nfs_quic_varint_encode(uint64_t value, uint8_t *out, size_t out_sz);

/* Decode a variable-length integer from `data`.
 * Returns bytes consumed (1, 2, 4, or 8), or -1 on error.
 * Stores decoded value in *value. */
int nfs_quic_varint_decode(const uint8_t *data, size_t len, uint64_t *value);

/* Return the encoded size needed for a value (1, 2, 4, or 8). */
int nfs_quic_varint_size(uint64_t value);

/* ---- Long header ---- */

struct nfs_quic_long_hdr {
    uint8_t  first_byte;
    uint32_t version;
    uint8_t  dcid_len;
    uint8_t  dcid[NFS_QUIC_MAX_CID_LEN];
    uint8_t  scid_len;
    uint8_t  scid[NFS_QUIC_MAX_CID_LEN];
};

/* Build a QUIC long header into `out`.
 * Returns bytes written, or -1 on error. */
int nfs_quic_long_hdr_build(const struct nfs_quic_long_hdr *hdr,
                            uint8_t *out, size_t out_sz);

/* Parse a QUIC long header from `data`.
 * Returns bytes consumed, or -1 on error. */
int nfs_quic_long_hdr_parse(const uint8_t *data, size_t len,
                            struct nfs_quic_long_hdr *hdr);

/* Get the long header packet type (0-3). */
uint8_t nfs_quic_long_hdr_type(uint8_t first_byte);

/* ---- Short header ---- */

struct nfs_quic_short_hdr {
    uint8_t  first_byte;
    uint8_t  dcid_len;     /* must be known from connection state */
    uint8_t  dcid[NFS_QUIC_MAX_CID_LEN];
};

/* Build a QUIC short header into `out`.
 * Returns bytes written, or -1 on error. */
int nfs_quic_short_hdr_build(const struct nfs_quic_short_hdr *hdr,
                             uint8_t *out, size_t out_sz);

/* Parse a QUIC short header from `data`.
 * `dcid_len` must be known from connection context.
 * Returns bytes consumed, or -1 on error. */
int nfs_quic_short_hdr_parse(const uint8_t *data, size_t len,
                             uint8_t dcid_len,
                             struct nfs_quic_short_hdr *hdr);

/* Short header accessors */
int nfs_quic_short_hdr_spin(uint8_t first_byte);
int nfs_quic_short_hdr_key_phase(uint8_t first_byte);

/* ---- Frame identification ---- */

/* Identify a QUIC frame type from its first byte.
 * Returns the frame type value, or -1 if data is empty. */
int nfs_quic_frame_type(const uint8_t *data, size_t len);

/* Check if a frame type is a STREAM frame (0x08-0x0f). */
int nfs_quic_is_stream_frame(uint8_t frame_type);

/* For STREAM frames, decode the sub-bits:
 *   bit 0 (0x01): FIN
 *   bit 1 (0x02): LEN (length field present)
 *   bit 2 (0x04): OFF (offset field present) */
int nfs_quic_stream_has_fin(uint8_t frame_type);
int nfs_quic_stream_has_len(uint8_t frame_type);
int nfs_quic_stream_has_off(uint8_t frame_type);

/* Return human-readable frame type name. */
const char *nfs_quic_frame_name(uint8_t frame_type);

/* Return human-readable long header packet type name. */
const char *nfs_quic_long_type_name(uint8_t ptype);

#endif /* NFS_QUIC_H */
