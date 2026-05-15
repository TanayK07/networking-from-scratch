#ifndef NFS_FRAMING_H
#define NFS_FRAMING_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Framing techniques: byte stuffing, bit stuffing, and COBS.
 *
 * Byte stuffing (PPP, RFC 1662):
 *   Flag byte = 0x7E marks frame boundaries.
 *   Escape byte = 0x7D; the byte following is XORed with 0x20.
 *   0x7E in payload → 0x7D 0x5E
 *   0x7D in payload → 0x7D 0x5D
 *   Frame: [0x7E] [escaped payload] [0x7E]
 *
 * Bit stuffing (HDLC):
 *   Flag pattern 01111110 (0x7E) marks frame boundaries.
 *   After transmitting five consecutive 1-bits in the payload,
 *   a 0-bit is inserted. The receiver removes any 0-bit that
 *   follows five consecutive 1-bits.
 *
 * COBS (Consistent Overhead Byte Stuffing):
 *   Eliminates 0x00 bytes from payload. Each overhead byte
 *   indicates the distance to the next zero (or end of block).
 *   Maximum overhead is 1 byte per 254 data bytes.
 * --------------------------------------------------------------- */

#define NFS_PPP_FLAG   0x7E
#define NFS_PPP_ESCAPE 0x7D
#define NFS_PPP_XOR    0x20

/* ---- Byte stuffing (PPP-style) ---- */

/* Byte-stuff `data` into a PPP frame with leading+trailing 0x7E flags.
 * Returns total frame length, or -1 if out_sz is too small. */
int nfs_byte_stuff(const uint8_t *data, size_t len,
                   uint8_t *out, size_t out_sz);

/* Unstuff a PPP frame: strip flags, unescape.
 * `frame` must start and end with 0x7E.
 * Returns payload length, or -1 on error. */
int nfs_byte_unstuff(const uint8_t *frame, size_t frame_len,
                     uint8_t *out, size_t out_sz);

/* ---- Bit stuffing (HDLC-style) ---- */

/* Bit-stuff `nbits` bits from data[]. Insert a 0 after every run
 * of 5 consecutive 1-bits. Bits are packed MSB-first.
 * Returns total bits in output, or -1 on error. */
int nfs_bit_stuff(const uint8_t *data, size_t nbits,
                  uint8_t *out, size_t out_sz);

/* Remove stuffed 0-bits (reverse of nfs_bit_stuff).
 * Returns number of data bits recovered, or -1 on error. */
int nfs_bit_unstuff(const uint8_t *data, size_t nbits,
                    uint8_t *out, size_t out_sz);

/* ---- COBS (Consistent Overhead Byte Stuffing) ---- */

/* COBS-encode `data` (len bytes). Output does not contain 0x00.
 * Returns encoded length, or -1 if out_sz too small. */
int nfs_cobs_encode(const uint8_t *data, size_t len,
                    uint8_t *out, size_t out_sz);

/* COBS-decode `data` (len bytes) back to original.
 * Returns decoded length, or -1 on error. */
int nfs_cobs_decode(const uint8_t *data, size_t len,
                    uint8_t *out, size_t out_sz);

#endif /* NFS_FRAMING_H */
