#include "modulation.h"
#include <math.h>

/* 1/sqrt(2) = 0.70710678118654752... */
static const double INV_SQRT2 = 0.70710678118654752;

/* Gray-coded mapping indexed by raw 2-bit value:
 * 00 (0) -> -3, 01 (1) -> -1, 10 (2) -> +3, 11 (3) -> +1 */
static const double gray_to_level[4] = {-3.0, -1.0, +3.0, +1.0};

/* Reverse: find the Gray code for a given QAM level.
 * -3 -> 00, -1 -> 01, +1 -> 11, +3 -> 10 */
static int level_to_gray(double level) {
    if (level < -2.0)
        return 0; /* -3 -> 00 */
    if (level < 0.0)
        return 1; /* -1 -> 01 */
    if (level < 2.0)
        return 3; /* +1 -> 11 */
    return 2;     /* +3 -> 10 */
}

/* ---- OOK ---- */

nfs_symbol_t nfs_ook_modulate(int bit) {
    nfs_symbol_t s;
    s.i = (bit != 0) ? 1.0 : 0.0;
    s.q = 0.0;
    return s;
}

int nfs_ook_demodulate(nfs_symbol_t symbol) {
    double amplitude = fabs(symbol.i);
    return (amplitude >= 0.5) ? 1 : 0;
}

/* ---- BPSK ---- */

nfs_symbol_t nfs_bpsk_modulate(int bit) {
    nfs_symbol_t s;
    s.i = (bit != 0) ? -1.0 : 1.0;
    s.q = 0.0;
    return s;
}

int nfs_bpsk_demodulate(nfs_symbol_t symbol) {
    return (symbol.i >= 0.0) ? 0 : 1;
}

/* ---- QPSK ---- */

nfs_symbol_t nfs_qpsk_modulate(int dibit) {
    nfs_symbol_t s = {0.0, 0.0};
    if (dibit < 0 || dibit > 3)
        return s;

    /* Gray-coded QPSK constellation:
     * 00 ->  45 deg -> (+1/sqrt2, +1/sqrt2)
     * 01 -> 135 deg -> (-1/sqrt2, +1/sqrt2)
     * 11 -> 225 deg -> (-1/sqrt2, -1/sqrt2)
     * 10 -> 315 deg -> (+1/sqrt2, -1/sqrt2) */
    switch (dibit) {
    case 0:
        s.i = +INV_SQRT2;
        s.q = +INV_SQRT2;
        break;
    case 1:
        s.i = -INV_SQRT2;
        s.q = +INV_SQRT2;
        break;
    case 3:
        s.i = -INV_SQRT2;
        s.q = -INV_SQRT2;
        break;
    case 2:
        s.i = +INV_SQRT2;
        s.q = -INV_SQRT2;
        break;
    }
    return s;
}

int nfs_qpsk_demodulate(nfs_symbol_t symbol) {
    /* Determine quadrant (Gray coded):
     * I >= 0, Q >= 0 -> 00 (45 deg)
     * I <  0, Q >= 0 -> 01 (135 deg)
     * I <  0, Q <  0 -> 11 (225 deg)
     * I >= 0, Q <  0 -> 10 (315 deg)
     *
     * bit1 (MSB) = (Q < 0), bit0 (LSB) = (I < 0) */
    int bit1 = (symbol.q < 0.0) ? 1 : 0;
    int bit0 = (symbol.i < 0.0) ? 1 : 0;
    return (bit1 << 1) | bit0;
}

/* ---- 16-QAM ---- */

nfs_symbol_t nfs_qam16_modulate(int nibble) {
    nfs_symbol_t s = {0.0, 0.0};
    if (nibble < 0 || nibble > 15)
        return s;

    /* Upper 2 bits (b3b2) -> I level, lower 2 bits (b1b0) -> Q level.
     * Gray coding: 00->-3, 01->-1, 11->+1, 10->+3 */
    int i_bits = (nibble >> 2) & 0x3;
    int q_bits = nibble & 0x3;

    s.i = gray_to_level[i_bits];
    s.q = gray_to_level[q_bits];
    return s;
}

int nfs_qam16_demodulate(nfs_symbol_t symbol) {
    /* Quantize I and Q to nearest level in {-3, -1, +1, +3},
     * then map back via Gray code. */
    double qi = round(symbol.i);
    double qq = round(symbol.q);

    /* Clamp to nearest odd value in {-3, -1, +1, +3} */
    /* Round to nearest of the 4 levels */
    if (qi <= -2.0)
        qi = -3.0;
    else if (qi <= 0.0)
        qi = -1.0;
    else if (qi <= 2.0)
        qi = 1.0;
    else
        qi = 3.0;

    if (qq <= -2.0)
        qq = -3.0;
    else if (qq <= 0.0)
        qq = -1.0;
    else if (qq <= 2.0)
        qq = 1.0;
    else
        qq = 3.0;

    int i_gray = level_to_gray(qi);
    int q_gray = level_to_gray(qq);

    return (i_gray << 2) | q_gray;
}

/* ---- Utility functions ---- */

int nfs_bits_per_symbol(nfs_mod_scheme_t scheme) {
    switch (scheme) {
    case NFS_MOD_OOK:
        return 1;
    case NFS_MOD_BPSK:
        return 1;
    case NFS_MOD_QPSK:
        return 2;
    case NFS_MOD_QAM16:
        return 4;
    default:
        return -1;
    }
}

double nfs_bitrate(double baud_rate, nfs_mod_scheme_t scheme) {
    int bps = nfs_bits_per_symbol(scheme);
    if (bps < 0)
        return -1.0;
    return baud_rate * (double)bps;
}
