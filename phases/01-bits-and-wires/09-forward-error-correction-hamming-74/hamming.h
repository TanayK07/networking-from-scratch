#ifndef NFS_HAMMING_H
#define NFS_HAMMING_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Hamming(7,4) — Forward Error Correction
 *
 * Encodes 4 data bits into a 7-bit codeword with 3 parity bits.
 * Can detect and correct any single-bit error.
 *
 * Bit positions (1-indexed):
 *   Position: 1    2    3    4    5    6    7
 *   Role:     p1   p2   d1   p3   d2   d3   d4
 *
 * Parity checks:
 *   p1 covers positions with bit 0 set in index: 1,3,5,7
 *   p2 covers positions with bit 1 set in index: 2,3,6,7
 *   p3 covers positions with bit 2 set in index: 4,5,6,7
 *
 * The return value uses the lower 7 bits (bits 6..0 correspond
 * to positions 7..1).
 * --------------------------------------------------------------- */

/* Encode 4 data bits (lower nibble of `data`) into a 7-bit codeword.
 * Only the lower 4 bits of `data` are used.
 * Returns the 7-bit codeword in the lower 7 bits. */
uint8_t nfs_hamming74_encode(uint8_t data);

/* Decode a 7-bit codeword, correcting a single-bit error if present.
 * Writes the recovered 4 data bits to *data_out.
 * Returns:
 *   0 = no error
 *   1 = single-bit error corrected
 *  -1 = uncorrectable (should not happen with single-bit errors in
 *       Hamming(7,4), but reserved for extension) */
int nfs_hamming74_decode(uint8_t codeword, uint8_t *data_out);

/* Compute the 3-bit syndrome for a 7-bit codeword.
 * Returns 0 if the codeword is valid, else the position (1-7) of
 * the erroneous bit. */
uint8_t nfs_hamming74_syndrome(uint8_t codeword);

/* Encode a byte buffer.  Each input byte produces 2 codewords
 * (high nibble first, then low nibble).
 * Returns the number of codeword bytes written. */
size_t nfs_hamming_encode_buf(const uint8_t *data, size_t len,
                              uint8_t *out, size_t out_sz);

/* Decode a buffer of codewords back to data bytes.
 * code_len must be even (2 codewords per output byte).
 * Returns the number of data bytes decoded, or -1 on error. */
int nfs_hamming_decode_buf(const uint8_t *codes, size_t code_len,
                           uint8_t *out, size_t out_sz);

#endif /* NFS_HAMMING_H */
