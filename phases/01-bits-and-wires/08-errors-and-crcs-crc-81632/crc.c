/* CRC-8 / CRC-16 / CRC-32 — Table-driven implementations.
 *
 * Each CRC variant uses a 256-entry lookup table that is generated
 * once at startup.  The table maps each possible byte value to its
 * contribution to the CRC, eliminating the need for bit-by-bit
 * shifting during computation.
 *
 * References:
 *   - "A Painless Guide to CRC Error Detection Algorithms" (Ross Williams)
 *   - IEEE 802.3 (Ethernet FCS)
 *   - ITU-T V.41 (CRC-CCITT)
 */

#include "crc.h"

/* ---------------------------------------------------------------
 * CRC-8 (polynomial 0x07, init 0x00)
 * --------------------------------------------------------------- */

static uint8_t crc8_table[256];
static int crc8_table_ready = 0;

void nfs_crc8_init_table(void) {
    for (int i = 0; i < 256; i++) {
        uint8_t crc = (uint8_t)i;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (uint8_t)((crc << 1) ^ NFS_CRC8_POLY);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
        crc8_table[i] = crc;
    }
    crc8_table_ready = 1;
}

uint8_t nfs_crc8(const uint8_t *data, size_t len) {
    if (!crc8_table_ready) {
        nfs_crc8_init_table();
    }

    uint8_t crc = 0x00; /* init value */
    for (size_t i = 0; i < len; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

/* ---------------------------------------------------------------
 * CRC-16/CCITT (polynomial 0x1021, init 0xFFFF)
 * --------------------------------------------------------------- */

static uint16_t crc16_table[256];
static int crc16_table_ready = 0;

void nfs_crc16_init_table(void) {
    for (int i = 0; i < 256; i++) {
        uint16_t crc = (uint16_t)(i << 8);
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ NFS_CRC16_POLY);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
        crc16_table[i] = crc;
    }
    crc16_table_ready = 1;
}

uint16_t nfs_crc16(const uint8_t *data, size_t len) {
    if (!crc16_table_ready) {
        nfs_crc16_init_table();
    }

    uint16_t crc = 0xFFFF; /* init value */
    for (size_t i = 0; i < len; i++) {
        uint8_t idx = (uint8_t)((crc >> 8) ^ data[i]);
        crc = (uint16_t)((crc << 8) ^ crc16_table[idx]);
    }
    return crc;
}

/* ---------------------------------------------------------------
 * CRC-32 (polynomial 0x04C11DB7, init 0xFFFFFFFF, final XOR 0xFFFFFFFF)
 *
 * This is the standard CRC-32 used by Ethernet (IEEE 802.3),
 * zlib, gzip, PNG, and many other protocols/formats.
 *
 * The reflected algorithm processes bits LSB-first, which matches
 * how Ethernet transmits data.  The reflected polynomial is
 * 0xEDB88320.
 * --------------------------------------------------------------- */

static uint32_t crc32_table[256];
static int crc32_table_ready = 0;

/* Reflected polynomial: bits of 0x04C11DB7 reversed. */
#define NFS_CRC32_POLY_REFLECTED 0xEDB88320u

void nfs_crc32_init_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ NFS_CRC32_POLY_REFLECTED;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_ready = 1;
}

uint32_t nfs_crc32(const uint8_t *data, size_t len) {
    if (!crc32_table_ready) {
        nfs_crc32_init_table();
    }

    uint32_t crc = 0xFFFFFFFF; /* init value */
    for (size_t i = 0; i < len; i++) {
        uint8_t idx = (uint8_t)((crc ^ data[i]) & 0xFF);
        crc = (crc >> 8) ^ crc32_table[idx];
    }
    return crc ^ 0xFFFFFFFF; /* final XOR */
}
