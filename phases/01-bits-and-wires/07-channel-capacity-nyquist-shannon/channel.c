/* Channel Capacity — Nyquist & Shannon implementation. */

#include "channel.h"
#include <math.h>

/* ---- Nyquist theorem ------------------------------------------ */

double nfs_nyquist_capacity(double bandwidth_hz, int signal_levels)
{
    if (bandwidth_hz <= 0.0 || signal_levels < 2)
        return -1.0;

    return 2.0 * bandwidth_hz * log2((double)signal_levels);
}

/* ---- Shannon-Hartley theorem ---------------------------------- */

double nfs_shannon_capacity(double bandwidth_hz, double snr_linear)
{
    if (bandwidth_hz <= 0.0 || snr_linear < 0.0)
        return -1.0;

    return bandwidth_hz * log2(1.0 + snr_linear);
}

double nfs_shannon_capacity_db(double bandwidth_hz, double snr_db)
{
    if (bandwidth_hz <= 0.0)
        return -1.0;

    double snr_linear = nfs_snr_db_to_linear(snr_db);
    return nfs_shannon_capacity(bandwidth_hz, snr_linear);
}

/* ---- SNR conversion ------------------------------------------- */

double nfs_snr_db_to_linear(double snr_db)
{
    return pow(10.0, snr_db / 10.0);
}

double nfs_snr_linear_to_db(double snr_linear)
{
    if (snr_linear <= 0.0)
        return -1.0;

    return 10.0 * log10(snr_linear);
}

/* ---- Spectral efficiency -------------------------------------- */

double nfs_spectral_efficiency(double snr_linear)
{
    if (snr_linear < 0.0)
        return -1.0;

    return log2(1.0 + snr_linear);
}

double nfs_spectral_efficiency_db(double snr_db)
{
    double snr_linear = nfs_snr_db_to_linear(snr_db);
    return nfs_spectral_efficiency(snr_linear);
}

/* ---- Inverse Shannon ------------------------------------------ */

double nfs_min_snr_for_rate(double bandwidth_hz, double target_bps)
{
    if (bandwidth_hz <= 0.0 || target_bps < 0.0)
        return -1.0;

    return pow(2.0, target_bps / bandwidth_hz) - 1.0;
}

double nfs_min_snr_for_rate_db(double bandwidth_hz, double target_bps)
{
    double snr_lin = nfs_min_snr_for_rate(bandwidth_hz, target_bps);
    if (snr_lin < 0.0)
        return -1.0;

    /* snr_lin can be 0 when target_bps == 0; log10(0) is -inf */
    if (snr_lin == 0.0)
        return -INFINITY;

    return nfs_snr_linear_to_db(snr_lin);
}

/* ---- Maximum signal levels ------------------------------------ */

int nfs_max_signal_levels(double snr_linear)
{
    if (snr_linear < 0.0)
        return -1;

    return (int)floor(sqrt(1.0 + snr_linear));
}
