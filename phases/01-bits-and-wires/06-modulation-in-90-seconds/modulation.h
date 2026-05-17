#ifndef NFS_MODULATION_H
#define NFS_MODULATION_H

#include <stdint.h>

/* ---------------------------------------------------------------
 * Digital modulation schemes used in physical-layer networking.
 *
 * OOK (On-Off Keying):
 *   Simplest scheme. 0 -> amplitude 0, 1 -> amplitude 1.
 *   Used in: infrared remotes, simple RF, 100BASE-TX idle.
 *
 * BPSK (Binary Phase Shift Keying):
 *   0 -> phase 0 deg -> (+1, 0)
 *   1 -> phase 180 deg -> (-1, 0)
 *   Most noise-resilient binary scheme.
 *   Used in: 802.11b (1 Mbps), deep-space comms, CDMA pilot.
 *
 * QPSK (Quadrature Phase Shift Keying):
 *   Maps 2-bit dibit to one of 4 phases (Gray coded):
 *     00 -> 45 deg   -> (+1/sqrt2, +1/sqrt2)
 *     01 -> 135 deg  -> (-1/sqrt2, +1/sqrt2)
 *     11 -> 225 deg  -> (-1/sqrt2, -1/sqrt2)
 *     10 -> 315 deg  -> (+1/sqrt2, -1/sqrt2)
 *   Used in: 802.11a/g (12 Mbps), DVB-S, LTE.
 *
 * 16-QAM (Quadrature Amplitude Modulation):
 *   Maps 4-bit nibble to I/Q grid point on 4x4 grid.
 *   I, Q in {-3, -1, +1, +3}. Gray-coded mapping:
 *     bits b3b2 -> I: 00->-3, 01->-1, 11->+1, 10->+3
 *     bits b1b0 -> Q: 00->-3, 01->-1, 11->+1, 10->+3
 *   Used in: 802.11a/g (24 Mbps), cable modems, LTE.
 * --------------------------------------------------------------- */

/* I/Q symbol representation */
typedef struct {
    double i; /* in-phase component */
    double q; /* quadrature component */
} nfs_symbol_t;

/* Modulation scheme enumeration */
typedef enum { NFS_MOD_OOK, NFS_MOD_BPSK, NFS_MOD_QPSK, NFS_MOD_QAM16 } nfs_mod_scheme_t;

/* ---- OOK (On-Off Keying) ---- */

/* Modulate a single bit (0 or 1) using OOK.
 * bit 0 -> (0, 0), bit 1 -> (1, 0). */
nfs_symbol_t nfs_ook_modulate(int bit);

/* Demodulate an OOK symbol back to a bit.
 * Threshold at amplitude 0.5. Returns 0 or 1. */
int nfs_ook_demodulate(nfs_symbol_t symbol);

/* ---- BPSK (Binary Phase Shift Keying) ---- */

/* Modulate a single bit using BPSK.
 * bit 0 -> (+1, 0), bit 1 -> (-1, 0). */
nfs_symbol_t nfs_bpsk_modulate(int bit);

/* Demodulate a BPSK symbol back to a bit.
 * Returns 0 if I >= 0, else 1. */
int nfs_bpsk_demodulate(nfs_symbol_t symbol);

/* ---- QPSK (Quadrature PSK) ---- */

/* Modulate a 2-bit dibit (0-3) using QPSK with Gray coding.
 * Returns symbol on success, or {0,0} if dibit > 3. */
nfs_symbol_t nfs_qpsk_modulate(int dibit);

/* Demodulate a QPSK symbol back to a dibit (0-3). */
int nfs_qpsk_demodulate(nfs_symbol_t symbol);

/* ---- 16-QAM ---- */

/* Modulate a 4-bit nibble (0-15) using 16-QAM with Gray coding.
 * Returns symbol on success, or {0,0} if nibble > 15. */
nfs_symbol_t nfs_qam16_modulate(int nibble);

/* Demodulate a 16-QAM symbol back to a nibble (0-15).
 * Returns -1 on error. */
int nfs_qam16_demodulate(nfs_symbol_t symbol);

/* ---- Utility functions ---- */

/* Returns the number of bits carried per symbol for a given scheme.
 * OOK=1, BPSK=1, QPSK=2, QAM16=4. Returns -1 for unknown scheme. */
int nfs_bits_per_symbol(nfs_mod_scheme_t scheme);

/* Returns the bitrate (bps) for a given baud rate and scheme.
 * bitrate = baud_rate * bits_per_symbol.
 * Returns -1 for unknown scheme. */
double nfs_bitrate(double baud_rate, nfs_mod_scheme_t scheme);

#endif /* NFS_MODULATION_H */
