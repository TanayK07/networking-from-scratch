#ifndef NFS_CHANNEL_H
#define NFS_CHANNEL_H

/* ---------------------------------------------------------------
 * Channel Capacity — Nyquist & Shannon theorems.
 *
 * Nyquist (1928):  C = 2 * B * log2(M)       (noiseless channel)
 * Shannon (1948):  C = B * log2(1 + S/N)      (noisy channel)
 * --------------------------------------------------------------- */

/* --- Nyquist theorem (noiseless channel) ----------------------- */

/* Maximum bit rate on a noiseless channel.
 *   bandwidth_hz : channel bandwidth in Hz (must be > 0)
 *   signal_levels: number of discrete signal levels (must be >= 2)
 * Returns bits/sec, or -1.0 on invalid input. */
double nfs_nyquist_capacity(double bandwidth_hz, int signal_levels);

/* --- Shannon-Hartley theorem (noisy channel) ------------------- */

/* Channel capacity with SNR as a linear ratio.
 *   bandwidth_hz : channel bandwidth in Hz (must be > 0)
 *   snr_linear   : signal-to-noise ratio, linear (must be >= 0)
 * Returns bits/sec, or -1.0 on invalid input. */
double nfs_shannon_capacity(double bandwidth_hz, double snr_linear);

/* Convenience: Shannon capacity with SNR in decibels. */
double nfs_shannon_capacity_db(double bandwidth_hz, double snr_db);

/* --- SNR conversion -------------------------------------------- */

/* Convert SNR from decibels to linear ratio. */
double nfs_snr_db_to_linear(double snr_db);

/* Convert SNR from linear ratio to decibels.
 * Returns -1.0 if snr_linear <= 0. */
double nfs_snr_linear_to_db(double snr_linear);

/* --- Spectral efficiency --------------------------------------- */

/* Spectral efficiency: bits/sec per Hz of bandwidth.
 *   eta = log2(1 + S/N)
 * snr_linear must be >= 0; returns -1.0 on invalid input. */
double nfs_spectral_efficiency(double snr_linear);

/* Convenience: spectral efficiency with SNR in decibels. */
double nfs_spectral_efficiency_db(double snr_db);

/* --- Inverse Shannon (minimum SNR for target rate) ------------- */

/* Minimum linear SNR required to achieve target_bps in bandwidth_hz.
 *   SNR = 2^(C/B) - 1
 * Returns linear SNR, or -1.0 on invalid input. */
double nfs_min_snr_for_rate(double bandwidth_hz, double target_bps);

/* Same, but returns the result in decibels. */
double nfs_min_snr_for_rate_db(double bandwidth_hz, double target_bps);

/* --- Maximum signal levels (Nyquist limited by Shannon) -------- */

/* Combining Nyquist and Shannon: M_max = floor(sqrt(1 + S/N)).
 * Returns the maximum usable signal levels, or -1 on invalid input. */
int nfs_max_signal_levels(double snr_linear);

#endif /* NFS_CHANNEL_H */
