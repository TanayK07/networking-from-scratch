/* Channel Capacity — unit tests. */

#include "channel.h"
#include <stdio.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(expr, expected) \
    do { \
        tests_run++; \
        long long _got = (long long)(expr); \
        long long _exp = (long long)(expected); \
        if (_got != _exp) { \
            fprintf(stderr, "  FAIL %s:%d: %s == 0x%llx, want 0x%llx\n", \
                    __FILE__, __LINE__, #expr, _got, _exp); \
            return; \
        } \
        tests_passed++; \
    } while (0)

#define ASSERT_TRUE(expr) \
    do { \
        tests_run++; \
        if (!(expr)) { \
            fprintf(stderr, "  FAIL %s:%d: %s is false\n", \
                    __FILE__, __LINE__, #expr); \
            return; \
        } \
        tests_passed++; \
    } while (0)

#define ASSERT_NEAR(expr, expected, tol) \
    do { \
        tests_run++; \
        double _got = (double)(expr); \
        double _exp = (double)(expected); \
        if (fabs(_got - _exp) > (tol)) { \
            fprintf(stderr, "  FAIL %s:%d: %s == %.6f, want %.6f (tol %.6f)\n", \
                    __FILE__, __LINE__, #expr, _got, _exp, (double)(tol)); \
            return; \
        } \
        tests_passed++; \
    } while (0)

/* ================================================================
 * Nyquist tests
 * ================================================================ */

static void test_nyquist_voice_2_levels(void)
{
    /* B=3100 Hz, M=2: C = 2*3100*log2(2) = 6200 bps */
    double c = nfs_nyquist_capacity(3100.0, 2);
    ASSERT_NEAR(c, 6200.0, 1.0);
}

static void test_nyquist_voice_4_levels(void)
{
    /* B=3100 Hz, M=4: C = 2*3100*log2(4) = 12400 bps */
    double c = nfs_nyquist_capacity(3100.0, 4);
    ASSERT_NEAR(c, 12400.0, 1.0);
}

static void test_nyquist_voice_16_levels(void)
{
    /* B=3100 Hz, M=16: C = 2*3100*log2(16) = 24800 bps */
    double c = nfs_nyquist_capacity(3100.0, 16);
    ASSERT_NEAR(c, 24800.0, 1.0);
}

static void test_nyquist_invalid_m1(void)
{
    /* M=1: can't encode anything */
    double c = nfs_nyquist_capacity(3100.0, 1);
    ASSERT_NEAR(c, -1.0, 1e-9);
}

static void test_nyquist_invalid_bw_zero(void)
{
    double c = nfs_nyquist_capacity(0.0, 2);
    ASSERT_NEAR(c, -1.0, 1e-9);
}

static void test_nyquist_invalid_bw_neg(void)
{
    double c = nfs_nyquist_capacity(-1000.0, 2);
    ASSERT_NEAR(c, -1.0, 1e-9);
}

/* ================================================================
 * Shannon tests
 * ================================================================ */

static void test_shannon_voice_30db(void)
{
    /* B=3100, SNR=30dB -> linear=1000, C = 3100*log2(1001) */
    double snr = nfs_snr_db_to_linear(30.0);
    double c = nfs_shannon_capacity(3100.0, snr);
    /* log2(1001) ~ 9.96722..., C ~ 30898.4 */
    ASSERT_NEAR(c, 30898.4, 1.0);
}

static void test_shannon_voice_20db(void)
{
    /* B=3100, SNR=20dB -> linear=100, C = 3100*log2(101) */
    double snr = nfs_snr_db_to_linear(20.0);
    double c = nfs_shannon_capacity(3100.0, snr);
    /* log2(101) ~ 6.65821..., C ~ 20640.5 */
    ASSERT_NEAR(c, 20640.5, 1.0);
}

static void test_shannon_wifi(void)
{
    /* B=22 MHz, SNR=20dB -> C = 22e6*log2(101) ~ 146.48M bps */
    double snr = nfs_snr_db_to_linear(20.0);
    double c = nfs_shannon_capacity(22.0e6, snr);
    ASSERT_NEAR(c, 146480655.0, 1000.0);
}

static void test_shannon_zero_snr(void)
{
    /* B=3100, SNR=0 (linear=1): C = 3100*log2(2) = 3100 */
    double c = nfs_shannon_capacity(3100.0, 1.0);
    ASSERT_NEAR(c, 3100.0, 1.0);
}

static void test_shannon_very_high_snr(void)
{
    /* B=1, SNR=1000000: C = log2(1000001) ~ 19.93 */
    double c = nfs_shannon_capacity(1.0, 1000000.0);
    ASSERT_NEAR(c, 19.93, 0.01);
}

static void test_shannon_invalid_neg_snr(void)
{
    double c = nfs_shannon_capacity(3100.0, -1.0);
    ASSERT_NEAR(c, -1.0, 1e-9);
}

/* ================================================================
 * SNR conversion tests
 * ================================================================ */

static void test_snr_0db(void)
{
    /* 0 dB = linear 1 */
    ASSERT_NEAR(nfs_snr_db_to_linear(0.0), 1.0, 1e-3);
}

static void test_snr_10db(void)
{
    /* 10 dB = linear 10 */
    ASSERT_NEAR(nfs_snr_db_to_linear(10.0), 10.0, 1e-3);
}

static void test_snr_20db(void)
{
    /* 20 dB = linear 100 */
    ASSERT_NEAR(nfs_snr_db_to_linear(20.0), 100.0, 1e-3);
}

static void test_snr_30db(void)
{
    /* 30 dB = linear 1000 */
    ASSERT_NEAR(nfs_snr_db_to_linear(30.0), 1000.0, 1e-3);
}

static void test_snr_3db(void)
{
    /* 3 dB ~ linear 1.99526 */
    ASSERT_NEAR(nfs_snr_db_to_linear(3.0), 1.99526, 1e-3);
}

static void test_snr_roundtrip(void)
{
    /* db_to_linear(linear_to_db(42.0)) ~ 42.0 */
    double rt = nfs_snr_db_to_linear(nfs_snr_linear_to_db(42.0));
    ASSERT_NEAR(rt, 42.0, 1e-3);
}

/* ================================================================
 * Spectral efficiency tests
 * ================================================================ */

static void test_spectral_eff_0db(void)
{
    /* SNR=1 linear (0 dB): eta = log2(2) = 1.0 */
    ASSERT_NEAR(nfs_spectral_efficiency(1.0), 1.0, 1e-6);
}

static void test_spectral_eff_10db(void)
{
    /* SNR=10 linear: eta = log2(11) ~ 3.459 */
    ASSERT_NEAR(nfs_spectral_efficiency(10.0), 3.459, 1e-3);
}

/* ================================================================
 * Inverse Shannon tests
 * ================================================================ */

static void test_inverse_shannon_6200(void)
{
    /* 6200 bps in 3100 Hz: SNR = 2^(6200/3100) - 1 = 2^2 - 1 = 3.0 */
    double snr = nfs_min_snr_for_rate(3100.0, 6200.0);
    ASSERT_NEAR(snr, 3.0, 1e-3);
}

static void test_inverse_shannon_6200_db(void)
{
    /* 3.0 linear = 10*log10(3) ~ 4.77 dB */
    double snr_db = nfs_min_snr_for_rate_db(3100.0, 6200.0);
    ASSERT_NEAR(snr_db, 4.77, 0.01);
}

static void test_inverse_shannon_3100(void)
{
    /* 3100 bps in 3100 Hz: SNR = 2^1 - 1 = 1.0 */
    double snr = nfs_min_snr_for_rate(3100.0, 3100.0);
    ASSERT_NEAR(snr, 1.0, 1e-3);
}

static void test_inverse_shannon_3100_db(void)
{
    /* 1.0 linear = 0 dB */
    double snr_db = nfs_min_snr_for_rate_db(3100.0, 3100.0);
    ASSERT_NEAR(snr_db, 0.0, 1e-3);
}

/* ================================================================
 * Max signal levels tests
 * ================================================================ */

static void test_max_levels_30db(void)
{
    /* SNR=1000: M = floor(sqrt(1001)) = floor(31.64) = 31 */
    ASSERT_EQ(nfs_max_signal_levels(1000.0), 31);
}

static void test_max_levels_20db(void)
{
    /* SNR=100: M = floor(sqrt(101)) = floor(10.05) = 10 */
    ASSERT_EQ(nfs_max_signal_levels(100.0), 10);
}

/* ================================================================
 * Cross-validation: Nyquist == Shannon when M = sqrt(1 + SNR)
 * ================================================================ */

static void test_cross_validation(void)
{
    double bw = 3100.0;
    double snr = 1000.0;  /* 30 dB linear */

    /* M = sqrt(1 + SNR) */
    double m_exact = sqrt(1.0 + snr);

    /* Nyquist capacity with exact M (real-valued, not floored) */
    double c_nyquist = 2.0 * bw * log2(m_exact);

    /* Shannon capacity */
    double c_shannon = nfs_shannon_capacity(bw, snr);

    /* They should be equal:
     * 2B*log2(sqrt(1+SNR)) = 2B*(0.5*log2(1+SNR)) = B*log2(1+SNR) */
    ASSERT_NEAR(c_nyquist, c_shannon, 1.0);
}

/* ================================================================ */

int main(void)
{
    printf("=== Channel Capacity Tests ===\n\n");

    /* Nyquist */
    printf("Nyquist tests:\n");
    test_nyquist_voice_2_levels();
    test_nyquist_voice_4_levels();
    test_nyquist_voice_16_levels();
    test_nyquist_invalid_m1();
    test_nyquist_invalid_bw_zero();
    test_nyquist_invalid_bw_neg();

    /* Shannon */
    printf("Shannon tests:\n");
    test_shannon_voice_30db();
    test_shannon_voice_20db();
    test_shannon_wifi();
    test_shannon_zero_snr();
    test_shannon_very_high_snr();
    test_shannon_invalid_neg_snr();

    /* SNR conversions */
    printf("SNR conversion tests:\n");
    test_snr_0db();
    test_snr_10db();
    test_snr_20db();
    test_snr_30db();
    test_snr_3db();
    test_snr_roundtrip();

    /* Spectral efficiency */
    printf("Spectral efficiency tests:\n");
    test_spectral_eff_0db();
    test_spectral_eff_10db();

    /* Inverse Shannon */
    printf("Inverse Shannon tests:\n");
    test_inverse_shannon_6200();
    test_inverse_shannon_6200_db();
    test_inverse_shannon_3100();
    test_inverse_shannon_3100_db();

    /* Max signal levels */
    printf("Max signal levels tests:\n");
    test_max_levels_30db();
    test_max_levels_20db();

    /* Cross-validation */
    printf("Cross-validation tests:\n");
    test_cross_validation();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) { printf("PASS\n"); return 0; }
    printf("FAIL\n"); return 1;
}
