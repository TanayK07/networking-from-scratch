#include "modulation.h"
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void demo_ook(void) {
    printf("=== OOK (On-Off Keying) ===\n");
    printf("1 bit per symbol. Used in: infrared remotes, simple RF.\n\n");
    for (int bit = 0; bit <= 1; bit++) {
        nfs_symbol_t s = nfs_ook_modulate(bit);
        int dec = nfs_ook_demodulate(s);
        printf("  bit %d -> symbol (I=%.1f, Q=%.1f) -> demod %d %s\n", bit, s.i, s.q, dec,
               (dec == bit) ? "OK" : "FAIL");
    }
    printf("\n");
}

static void demo_bpsk(void) {
    printf("=== BPSK (Binary Phase Shift Keying) ===\n");
    printf("1 bit per symbol. Most noise-resilient binary scheme.\n");
    printf("Used in: 802.11b (1 Mbps), deep-space comms.\n\n");
    for (int bit = 0; bit <= 1; bit++) {
        nfs_symbol_t s = nfs_bpsk_modulate(bit);
        int dec = nfs_bpsk_demodulate(s);
        printf("  bit %d -> symbol (I=%+.1f, Q=%.1f) -> demod %d %s\n", bit, s.i, s.q, dec,
               (dec == bit) ? "OK" : "FAIL");
    }
    printf("\n");
}

static void demo_qpsk(void) {
    printf("=== QPSK (Quadrature PSK) ===\n");
    printf("2 bits per symbol. Gray coded for minimal BER.\n");
    printf("Used in: 802.11a/g (12 Mbps), DVB-S, LTE.\n\n");
    const char *dibit_str[] = {"00", "01", "10", "11"};
    for (int d = 0; d < 4; d++) {
        nfs_symbol_t s = nfs_qpsk_modulate(d);
        int dec = nfs_qpsk_demodulate(s);
        double phase = atan2(s.q, s.i) * 180.0 / M_PI;
        if (phase < 0)
            phase += 360.0;
        printf("  dibit %s (=%d) -> (I=%+.4f, Q=%+.4f) phase=%5.1f deg -> demod %d %s\n",
               dibit_str[d], d, s.i, s.q, phase, dec, (dec == d) ? "OK" : "FAIL");
    }
    printf("\n");
}

static void demo_qam16(void) {
    printf("=== 16-QAM (Quadrature Amplitude Modulation) ===\n");
    printf("4 bits per symbol. 16 constellation points on 4x4 grid.\n");
    printf("Used in: 802.11a/g (24 Mbps), cable modems, LTE.\n\n");
    printf("  Nibble  Bits   I    Q   Roundtrip\n");
    printf("  ------  ----  ---  ---  ---------\n");
    for (int n = 0; n < 16; n++) {
        nfs_symbol_t s = nfs_qam16_modulate(n);
        int dec = nfs_qam16_demodulate(s);
        printf("    %2d    %d%d%d%d  %+2.0f  %+2.0f     %s\n", n, (n >> 3) & 1, (n >> 2) & 1,
               (n >> 1) & 1, n & 1, s.i, s.q, (dec == n) ? "OK" : "FAIL");
    }
    printf("\n");
}

static void demo_bitrate(void) {
    printf("=== Bitrate Comparison (at 1000 baud) ===\n");
    const nfs_mod_scheme_t schemes[] = {NFS_MOD_OOK, NFS_MOD_BPSK, NFS_MOD_QPSK, NFS_MOD_QAM16};
    const char *names[] = {"OOK", "BPSK", "QPSK", "16-QAM"};
    double baud = 1000.0;

    for (int i = 0; i < 4; i++) {
        int bps = nfs_bits_per_symbol(schemes[i]);
        double br = nfs_bitrate(baud, schemes[i]);
        printf("  %-7s  %d bit/symbol  -> %6.0f bps\n", names[i], bps, br);
    }
    printf("\n");
}

int main(void) {
    printf("Modulation in 90 Seconds\n");
    printf("========================\n\n");

    demo_ook();
    demo_bpsk();
    demo_qpsk();
    demo_qam16();
    demo_bitrate();

    return 0;
}
