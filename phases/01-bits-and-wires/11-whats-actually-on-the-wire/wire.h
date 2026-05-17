#ifndef NFS_WIRE_H
#define NFS_WIRE_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * What's Actually on the Wire — Ethernet physical-layer constructs.
 *
 * This module implements the physical-layer framing that surrounds
 * every Ethernet frame on the wire:
 *
 * 1. Preamble & SFD (IEEE 802.3)
 *    7 bytes of 0xAA + 1 byte SFD 0xAB for clock synchronization.
 *
 * 2. Interframe Gap (IFG)
 *    Minimum 12 bytes (96 bit times) of idle between frames.
 *
 * 3. 8b/10b encoding concepts (IEEE 802.3z, Gigabit Ethernet)
 *    Running disparity tracking, DC balance, K28.5 comma detection.
 *
 * 4. Frame overhead and wire efficiency calculations.
 *
 * 5. Bit time and frame transmission time.
 * --------------------------------------------------------------- */

/* ---- Preamble & SFD constants ---- */
#define NFS_PREAMBLE_BYTE   0xAA
#define NFS_SFD_BYTE        0xAB
#define NFS_PREAMBLE_LEN    7
#define NFS_SFD_LEN         1
#define NFS_PREAMBLE_TOTAL  8

/* ---- Interframe Gap ---- */
#define NFS_IFG_BYTES       12

/* ---- 8b/10b K28.5 comma symbols (10-bit values) ---- */
#define NFS_K28_5_RDN       0x0FA   /* 00 1111 1010 */
#define NFS_K28_5_RDP       0x305   /* 11 0000 0101 */

/* ---- Ethernet frame size constants ---- */
#define NFS_MIN_PAYLOAD     46
#define NFS_MAX_PAYLOAD     1500
#define NFS_ETH_HEADER      14
#define NFS_FCS_SIZE         4

/* ---- Preamble ---- */

/* Generate the 8-byte Ethernet preamble (7x 0xAA + 1x 0xAB SFD).
 * Returns 8 (bytes written), or 0 if out_sz < 8. */
int nfs_preamble_generate(uint8_t *out, size_t out_sz);

/* Find the SFD byte (0xAB) preceded by preamble bytes (0xAA) in data.
 * Returns offset of the SFD byte, or -1 if not found. */
int nfs_preamble_detect(const uint8_t *data, size_t len);

/* ---- Interframe Gap ---- */

/* Returns the minimum IFG in bytes (always 12, per IEEE 802.3). */
int nfs_ifg_bytes(int speed_mbps);

/* Returns the IFG duration in nanoseconds for a given speed.
 * 10 Mbps: 9600 ns, 100 Mbps: 960 ns, 1 Gbps: 96 ns, 10 Gbps: 10 ns. */
int nfs_ifg_ns(int speed_mbps);

/* ---- 8b/10b encoding concepts ---- */

/* Update running disparity based on a 10-bit symbol.
 * If ones > 5: return +1. If ones < 5: return -1. If ones == 5: unchanged. */
int nfs_rd_update(int current_rd, uint16_t symbol_10bit);

/* Check if a 10-bit symbol is DC-balanced (exactly 5 ones).
 * Returns 1 if balanced, 0 otherwise. */
int nfs_8b10b_dc_balanced(uint16_t symbol);

/* Check if a 10-bit symbol is a K28.5 comma (0x0FA or 0x305).
 * Returns 1 if comma, 0 otherwise. */
int nfs_is_comma(uint16_t symbol);

/* ---- Frame overhead and efficiency ---- */

/* Returns total non-payload overhead per frame on the wire:
 * preamble(7) + SFD(1) + header(14) + FCS(4) + IFG(12) = 38 bytes. */
int nfs_wire_frame_overhead(void);

/* Returns wire efficiency as a fraction (0.0 to 1.0).
 * efficiency = payload / (payload + 38). */
double nfs_wire_efficiency(size_t payload_size);

/* Returns minimum Ethernet frame size (without preamble/SFD/IFG): 64 bytes. */
int nfs_min_frame_size(void);

/* Returns maximum standard Ethernet frame size: 1518 bytes. */
int nfs_max_frame_size(void);

/* ---- Bit time and frame timing ---- */

/* Returns nanoseconds per bit at the given speed.
 * 10 Mbps: 100, 100 Mbps: 10, 1000 Mbps: 1, 10000 Mbps: 0. */
int nfs_bit_time_ns(int speed_mbps);

/* Returns total frame transmission time in nanoseconds.
 * Includes preamble+SFD (8 bytes) and IFG (12 bytes).
 * = (frame_bytes + 8 + 12) * 8 * bit_time_ns. */
long long nfs_frame_time_ns(size_t frame_bytes, int speed_mbps);

#endif /* NFS_WIRE_H */
