/* Tests for TCP Keepalive (RFC 1122 Section 4.2.3.6). */

#include "../keepalive.h"

#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(expr, expected)                                                                  \
    do {                                                                                           \
        tests_run++;                                                                               \
        long long _got = (long long)(expr);                                                        \
        long long _exp = (long long)(expected);                                                    \
        if (_got != _exp) {                                                                        \
            fprintf(stderr, "  FAIL %s:%d: %s == 0x%llx, want 0x%llx\n", __FILE__, __LINE__,       \
                    #expr, _got, _exp);                                                            \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

#define ASSERT_TRUE(expr)                                                                          \
    do {                                                                                           \
        tests_run++;                                                                               \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "  FAIL %s:%d: %s is false\n", __FILE__, __LINE__, #expr);             \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

/* ------------------------------------------------------------------ */

static void test_init_disabled(void) {
    printf("  init disabled by default...");
    struct nfs_keepalive ka;
    nfs_keepalive_init(&ka, 7200.0, 75.0, 9);
    ASSERT_EQ(ka.enabled, 0);
    ASSERT_EQ(ka.probes_sent, 0);
    printf(" OK\n");
}

static void test_enable_disable(void) {
    printf("  enable/disable...");
    struct nfs_keepalive ka;
    nfs_keepalive_init(&ka, 7200.0, 75.0, 9);
    nfs_keepalive_enable(&ka);
    ASSERT_EQ(ka.enabled, 1);
    nfs_keepalive_disable(&ka);
    ASSERT_EQ(ka.enabled, 0);
    printf(" OK\n");
}

static void test_no_probe_when_disabled(void) {
    printf("  no probe when disabled...");
    struct nfs_keepalive ka;
    nfs_keepalive_init(&ka, 100.0, 10.0, 3);
    nfs_keepalive_data_received(&ka, 0.0);
    ASSERT_EQ(nfs_keepalive_check(&ka, 200.0), 0);
    printf(" OK\n");
}

static void test_no_probe_before_idle(void) {
    printf("  no probe before idle timeout...");
    struct nfs_keepalive ka;
    nfs_keepalive_init(&ka, 100.0, 10.0, 3);
    nfs_keepalive_enable(&ka);
    nfs_keepalive_data_received(&ka, 0.0);
    ASSERT_EQ(nfs_keepalive_check(&ka, 50.0), 0);
    ASSERT_EQ(nfs_keepalive_check(&ka, 99.0), 0);
    printf(" OK\n");
}

static void test_first_probe_at_idle(void) {
    printf("  first probe at idle timeout...");
    struct nfs_keepalive ka;
    nfs_keepalive_init(&ka, 100.0, 10.0, 3);
    nfs_keepalive_enable(&ka);
    nfs_keepalive_data_received(&ka, 0.0);
    ASSERT_EQ(nfs_keepalive_check(&ka, 100.0), 1);
    ASSERT_EQ(ka.probes_sent, 1);
    printf(" OK\n");
}

static void test_subsequent_probes(void) {
    printf("  subsequent probes at intervals...");
    struct nfs_keepalive ka;
    nfs_keepalive_init(&ka, 100.0, 10.0, 3);
    nfs_keepalive_enable(&ka);
    nfs_keepalive_data_received(&ka, 0.0);

    ASSERT_EQ(nfs_keepalive_check(&ka, 100.0), 1);
    ASSERT_EQ(ka.probes_sent, 1);

    ASSERT_EQ(nfs_keepalive_check(&ka, 110.0), 1);
    ASSERT_EQ(ka.probes_sent, 2);
    printf(" OK\n");
}

static void test_dead_after_max_probes(void) {
    printf("  dead after max probes...");
    struct nfs_keepalive ka;
    nfs_keepalive_init(&ka, 100.0, 10.0, 3);
    nfs_keepalive_enable(&ka);
    nfs_keepalive_data_received(&ka, 0.0);

    ASSERT_EQ(nfs_keepalive_check(&ka, 100.0), 1);
    ASSERT_EQ(nfs_keepalive_check(&ka, 110.0), 1);
    ASSERT_EQ(nfs_keepalive_check(&ka, 120.0), -1);
    ASSERT_EQ(nfs_keepalive_is_dead(&ka), 1);
    printf(" OK\n");
}

static void test_data_resets_probes(void) {
    printf("  data received resets probes...");
    struct nfs_keepalive ka;
    nfs_keepalive_init(&ka, 100.0, 10.0, 3);
    nfs_keepalive_enable(&ka);
    nfs_keepalive_data_received(&ka, 0.0);

    ASSERT_EQ(nfs_keepalive_check(&ka, 100.0), 1);
    ASSERT_EQ(ka.probes_sent, 1);

    nfs_keepalive_data_received(&ka, 105.0);
    ASSERT_EQ(ka.probes_sent, 0);
    ASSERT_EQ(nfs_keepalive_check(&ka, 150.0), 0);
    ASSERT_EQ(nfs_keepalive_check(&ka, 205.0), 1);
    printf(" OK\n");
}

static void test_is_dead(void) {
    printf("  is_dead...");
    struct nfs_keepalive ka;
    nfs_keepalive_init(&ka, 100.0, 10.0, 2);
    ASSERT_EQ(nfs_keepalive_is_dead(&ka), 0);
    ka.probes_sent = 2;
    ASSERT_EQ(nfs_keepalive_is_dead(&ka), 1);
    printf(" OK\n");
}

static void test_build_probe(void) {
    printf("  build probe...");
    uint8_t out[20];
    ASSERT_EQ(nfs_keepalive_build_probe(0xAABBCCDD, out, sizeof(out)), 20);
    ASSERT_EQ(out[4], 0xAA);
    ASSERT_EQ(out[5], 0xBB);
    ASSERT_EQ(out[6], 0xCC);
    ASSERT_EQ(out[7], 0xDD);
    ASSERT_EQ(out[12], 0x50);
    ASSERT_EQ(out[13], 0x10);
    printf(" OK\n");
}

static void test_build_probe_rejects_small(void) {
    printf("  build probe rejects small buf...");
    uint8_t out[10];
    ASSERT_EQ(nfs_keepalive_build_probe(0, out, 10), -1);
    ASSERT_EQ(nfs_keepalive_build_probe(0, NULL, 20), -1);
    printf(" OK\n");
}

static void test_format(void) {
    printf("  format output...");
    struct nfs_keepalive ka;
    nfs_keepalive_init(&ka, 7200.0, 75.0, 9);
    nfs_keepalive_enable(&ka);

    char buf[256];
    nfs_keepalive_format(&ka, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "enabled") != NULL);
    ASSERT_TRUE(strstr(buf, "7200") != NULL);
    ASSERT_TRUE(strstr(buf, "0/9") != NULL);
    printf(" OK\n");
}

static void test_defaults(void) {
    printf("  default constants...");
    ASSERT_TRUE(NFS_KA_DEFAULT_IDLE == 7200.0);
    ASSERT_TRUE(NFS_KA_DEFAULT_INTERVAL == 75.0);
    ASSERT_EQ(NFS_KA_DEFAULT_PROBES, 9);
    printf(" OK\n");
}

/* ------------------------------------------------------------------ */

int main(void) {
    printf("TCP Keepalive test suite\n");
    test_init_disabled();
    test_enable_disable();
    test_no_probe_when_disabled();
    test_no_probe_before_idle();
    test_first_probe_at_idle();
    test_subsequent_probes();
    test_dead_after_max_probes();
    test_data_resets_probes();
    test_is_dead();
    test_build_probe();
    test_build_probe_rejects_small();
    test_format();
    test_defaults();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
