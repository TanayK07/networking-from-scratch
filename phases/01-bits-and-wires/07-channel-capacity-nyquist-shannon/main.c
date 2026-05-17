/* Channel Capacity — demo program.
 *
 * Demonstrates Nyquist and Shannon channel capacity calculations
 * with real-world examples.
 */

#include "channel.h"
#include <stdio.h>

int main(void)
{
    printf("=== Channel Capacity: Nyquist & Shannon ===\n\n");

    /* --- 1. Nyquist: noiseless channel capacity --- */
    printf("1) Nyquist Theorem (noiseless channel)\n");
    printf("   C = 2 * B * log2(M)\n\n");

    double bw = 3100.0;  /* voice-band telephone line */

    double c2 = nfs_nyquist_capacity(bw, 2);
    printf("   Voice-band (%g Hz), 2 levels:  %8.0f bps\n", bw, c2);

    double c4 = nfs_nyquist_capacity(bw, 4);
    printf("   Voice-band (%g Hz), 4 levels:  %8.0f bps\n", bw, c4);

    double c16 = nfs_nyquist_capacity(bw, 16);
    printf("   Voice-band (%g Hz), 16 levels: %8.0f bps\n", bw, c16);

    /* --- 2. Shannon: noisy channel capacity --- */
    printf("\n2) Shannon-Hartley Theorem (noisy channel)\n");
    printf("   C = B * log2(1 + S/N)\n\n");

    double snr_30db = nfs_snr_db_to_linear(30.0);
    double cs30 = nfs_shannon_capacity(bw, snr_30db);
    printf("   Voice-band, SNR=30 dB (%g linear): %.1f bps\n", snr_30db, cs30);

    double snr_20db = nfs_snr_db_to_linear(20.0);
    double cs20 = nfs_shannon_capacity(bw, snr_20db);
    printf("   Voice-band, SNR=20 dB (%g linear): %.1f bps\n", snr_20db, cs20);

    /* 802.11b example */
    double bw_wifi = 22.0e6;
    double cw = nfs_shannon_capacity(bw_wifi, snr_20db);
    printf("   802.11b (22 MHz), SNR=20 dB:       %.1f Mbps\n", cw / 1.0e6);

    /* --- 3. SNR conversions --- */
    printf("\n3) SNR Conversions\n");
    printf("   0 dB  = %.1f linear\n", nfs_snr_db_to_linear(0.0));
    printf("   10 dB = %.1f linear\n", nfs_snr_db_to_linear(10.0));
    printf("   20 dB = %.1f linear\n", nfs_snr_db_to_linear(20.0));
    printf("   30 dB = %.1f linear\n", nfs_snr_db_to_linear(30.0));
    printf("   3 dB  = %.5f linear\n", nfs_snr_db_to_linear(3.0));

    /* --- 4. Spectral efficiency --- */
    printf("\n4) Spectral Efficiency (bits/sec/Hz)\n");
    printf("   SNR=0 dB:  %.3f bits/s/Hz\n", nfs_spectral_efficiency(1.0));
    printf("   SNR=10 dB: %.3f bits/s/Hz\n", nfs_spectral_efficiency(10.0));
    printf("   SNR=20 dB: %.3f bits/s/Hz\n", nfs_spectral_efficiency(100.0));
    printf("   SNR=30 dB: %.3f bits/s/Hz\n", nfs_spectral_efficiency(1000.0));

    /* --- 5. Inverse Shannon --- */
    printf("\n5) Minimum SNR for Target Rate\n");
    double min_snr = nfs_min_snr_for_rate(bw, 6200.0);
    double min_snr_db = nfs_min_snr_for_rate_db(bw, 6200.0);
    printf("   6200 bps in 3100 Hz: SNR >= %.1f (%.2f dB)\n", min_snr, min_snr_db);

    min_snr = nfs_min_snr_for_rate(bw, 3100.0);
    min_snr_db = nfs_min_snr_for_rate_db(bw, 3100.0);
    printf("   3100 bps in 3100 Hz: SNR >= %.1f (%.2f dB)\n", min_snr, min_snr_db);

    /* --- 6. Max signal levels --- */
    printf("\n6) Maximum Signal Levels (Nyquist limited by Shannon)\n");
    printf("   SNR=30 dB (1000): M_max = %d\n", nfs_max_signal_levels(1000.0));
    printf("   SNR=20 dB (100):  M_max = %d\n", nfs_max_signal_levels(100.0));
    printf("   SNR=10 dB (10):   M_max = %d\n", nfs_max_signal_levels(10.0));

    return 0;
}
