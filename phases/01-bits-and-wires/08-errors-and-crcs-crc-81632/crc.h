#ifndef NFS_CRC_H
#define NFS_CRC_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * CRC-8 / CRC-16 / CRC-32 — Table-driven implementations.
 *
 * CRC (Cyclic Redundancy Check) treats data as a polynomial over
 * GF(2) and computes the remainder after division by a generator
 * polynomial.  Each variant uses a different generator:
 *
 *   CRC-8:   x^8  + x^2  + x + 1          (poly 0x07)
 *   CRC-16:  x^16 + x^12 + x^5 + 1        (poly 0x1021, CCITT)
 *   CRC-32:  0x04C11DB7                    (Ethernet, ZIP, PNG)
 *
 * All functions operate on raw byte buffers and return the CRC
 * value.  No external dependencies beyond libc.
 * --------------------------------------------------------------- */

/* Standard CRC polynomials. */
#define NFS_CRC8_POLY  0x07u
#define NFS_CRC16_POLY 0x1021u
#define NFS_CRC32_POLY 0x04C11DB7u

/* ---------------------------------------------------------------
 * Table initialization
 *
 * Call these once before computing CRCs.  They populate internal
 * 256-entry lookup tables for O(1) per-byte processing.
 * --------------------------------------------------------------- */

void nfs_crc8_init_table(void);
void nfs_crc16_init_table(void);
void nfs_crc32_init_table(void);

/* ---------------------------------------------------------------
 * CRC computation
 * --------------------------------------------------------------- */

/* CRC-8: init 0x00, no final XOR. */
uint8_t nfs_crc8(const uint8_t *data, size_t len);

/* CRC-16/CCITT: init 0xFFFF, no final XOR. */
uint16_t nfs_crc16(const uint8_t *data, size_t len);

/* CRC-32 (Ethernet/zlib): init 0xFFFFFFFF, final XOR 0xFFFFFFFF. */
uint32_t nfs_crc32(const uint8_t *data, size_t len);

#endif /* NFS_CRC_H */
