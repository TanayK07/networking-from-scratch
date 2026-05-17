#ifndef NFS_ENCODING_H
#define NFS_ENCODING_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Line encoding schemes used in physical-layer networking.
 *
 * Manchester (IEEE 802.3):
 *   Each data bit becomes two symbols (clock periods):
 *     1 → low-high (0, 1)
 *     0 → high-low (1, 0)
 *   Guarantees a transition in every bit period for clock recovery.
 *
 * NRZI (Non-Return to Zero Inverted):
 *   1 → transition (toggle level)
 *   0 → no transition (same level)
 *   Used in USB, FDDI. Start level assumed 0.
 *
 * 4B/5B:
 *   Maps each 4-bit nibble to a 5-bit code chosen to guarantee
 *   no more than 3 consecutive zeros (better for NRZI).
 *   Standard table from FDDI / 100BASE-TX.
 * --------------------------------------------------------------- */

/* ---- Manchester encoding ---- */

/* Encode `len` bytes into Manchester symbols.
 * Output: 2 symbols per input bit = 16 symbols per byte.
 * Each output byte is 0 or 1.
 * Returns number of symbols written, or 0 on error. */
size_t nfs_manchester_encode(const uint8_t *data, size_t len, uint8_t *out, size_t out_sz);

/* Decode Manchester symbols back to data bytes.
 * sym_len must be a multiple of 16 (2 symbols/bit, 8 bits/byte).
 * Returns bytes decoded, or -1 on invalid transition. */
int nfs_manchester_decode(const uint8_t *symbols, size_t sym_len, uint8_t *out, size_t out_sz);

/* ---- NRZI encoding ---- */

/* Encode `len` bytes into NRZI symbols (1 symbol per bit).
 * Output: 8 symbols per byte, each 0 or 1. Initial level = 0.
 * Returns number of symbols written. */
size_t nfs_nrzi_encode(const uint8_t *data, size_t len, uint8_t *out, size_t out_sz);

/* Decode NRZI symbols back to data bytes.
 * sym_len must be a multiple of 8. Initial level = 0.
 * Returns bytes decoded, or -1 on error. */
int nfs_nrzi_decode(const uint8_t *symbols, size_t sym_len, uint8_t *out, size_t out_sz);

/* ---- 4B/5B encoding ---- */

/* Encode a 4-bit nibble (0x0..0xF) to its 5-bit code.
 * Returns the 5-bit code (0..31), or -1 if nibble > 0xF. */
int nfs_4b5b_encode(uint8_t nibble);

/* Decode a 5-bit code back to a 4-bit nibble.
 * Returns the nibble (0..15), or -1 for invalid/control codes. */
int nfs_4b5b_decode(uint8_t code);

#endif /* NFS_ENCODING_H */
