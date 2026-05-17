/* Unit tests for nfs_rtable (IP routing table with LPM). */

#include "../rtable.h"
#include <stdio.h>
#include <stdlib.h>
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

/* ---- add + lookup ---- */

static void test_add_and_lookup(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    /* 192.168.1.0/24 via if0 */
    nfs_rtable_add(&rt, nfs_ip4(192, 168, 1, 0), 24, 0, 0, 0, NFS_RTF_UP);

    const struct nfs_rt_entry *e = nfs_rtable_lookup(&rt, nfs_ip4(192, 168, 1, 100));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->iface, 0);
    ASSERT_EQ(e->prefix_len, 24);
    ASSERT_EQ(e->dest, nfs_ip4(192, 168, 1, 0));

    nfs_rtable_free(&rt);
}

/* ---- LPM works ---- */

static void test_lpm(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    /* 10.0.0.0/8 -> if0 */
    nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 0), 8, 0, 0, 10, NFS_RTF_UP);
    /* 10.0.1.0/24 -> if1 */
    nfs_rtable_add(&rt, nfs_ip4(10, 0, 1, 0), 24, 0, 1, 10, NFS_RTF_UP);
    /* 10.0.1.128/25 -> if2 */
    nfs_rtable_add(&rt, nfs_ip4(10, 0, 1, 128), 25, 0, 2, 10, NFS_RTF_UP);

    const struct nfs_rt_entry *e;

    /* 10.0.1.200 matches /25 (most specific) */
    e = nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 1, 200));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->iface, 2);
    ASSERT_EQ(e->prefix_len, 25);

    /* 10.0.1.50 matches /24 but not /25 */
    e = nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 1, 50));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->iface, 1);
    ASSERT_EQ(e->prefix_len, 24);

    /* 10.0.2.1 matches /8 only */
    e = nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 2, 1));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->iface, 0);
    ASSERT_EQ(e->prefix_len, 8);

    nfs_rtable_free(&rt);
}

/* ---- metric tie-breaking ---- */

static void test_metric_tiebreak(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    /* Two routes for 10.0.0.0/8 with different metrics */
    nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 0), 8, nfs_ip4(192, 168, 1, 1), 0, 100,
                   NFS_RTF_UP | NFS_RTF_GATEWAY);
    nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 0), 8, nfs_ip4(192, 168, 1, 2), 1, 50,
                   NFS_RTF_UP | NFS_RTF_GATEWAY);

    const struct nfs_rt_entry *e = nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 0, 1));
    ASSERT_TRUE(e != NULL);
    /* Lower metric (50) should win */
    ASSERT_EQ(e->metric, 50);
    ASSERT_EQ(e->iface, 1);
    ASSERT_EQ(e->gateway, nfs_ip4(192, 168, 1, 2));

    nfs_rtable_free(&rt);
}

/* ---- gateway flag ---- */

static void test_gateway_flag(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 0), 8, nfs_ip4(192, 168, 1, 1), 0, 10,
                   NFS_RTF_UP | NFS_RTF_GATEWAY);

    const struct nfs_rt_entry *e = nfs_rtable_lookup(&rt, nfs_ip4(10, 1, 2, 3));
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(e->flags & NFS_RTF_GATEWAY);
    ASSERT_EQ(e->gateway, nfs_ip4(192, 168, 1, 1));

    nfs_rtable_free(&rt);
}

/* ---- host route /32 ---- */

static void test_host_route(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    /* /8 route */
    nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 0), 8, 0, 0, 10, NFS_RTF_UP);
    /* /32 host route */
    nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 1), 32, nfs_ip4(192, 168, 1, 1), 1, 10,
                   NFS_RTF_UP | NFS_RTF_GATEWAY | NFS_RTF_HOST);

    const struct nfs_rt_entry *e = nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 0, 1));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->prefix_len, 32);
    ASSERT_TRUE(e->flags & NFS_RTF_HOST);
    ASSERT_EQ(e->iface, 1);

    /* Other addresses in /8 still go via if0 */
    e = nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 0, 2));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->iface, 0);
    ASSERT_EQ(e->prefix_len, 8);

    nfs_rtable_free(&rt);
}

/* ---- remove ---- */

static void test_remove(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 0), 8, 0, 0, 10, NFS_RTF_UP);
    nfs_rtable_add(&rt, nfs_ip4(192, 168, 1, 0), 24, 0, 1, 10, NFS_RTF_UP);

    ASSERT_EQ(rt.count, 2);
    ASSERT_EQ(nfs_rtable_remove(&rt, nfs_ip4(10, 0, 0, 0), 8), 0);
    ASSERT_EQ(rt.count, 1);

    /* 10.x should no longer match */
    ASSERT_TRUE(nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 0, 1)) == NULL);
    /* 192.168.1.x still works */
    ASSERT_TRUE(nfs_rtable_lookup(&rt, nfs_ip4(192, 168, 1, 50)) != NULL);

    nfs_rtable_free(&rt);
}

/* ---- remove not found ---- */

static void test_remove_not_found(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    ASSERT_EQ(nfs_rtable_remove(&rt, nfs_ip4(10, 0, 0, 0), 8), -1);

    nfs_rtable_free(&rt);
}

/* ---- default route ---- */

static void test_default_route(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    /* Default route 0.0.0.0/0 */
    nfs_rtable_add(&rt, 0, 0, nfs_ip4(192, 168, 1, 1), 0, 100, NFS_RTF_UP | NFS_RTF_GATEWAY);

    const struct nfs_rt_entry *e = nfs_rtable_lookup(&rt, nfs_ip4(8, 8, 8, 8));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->prefix_len, 0);
    ASSERT_EQ(e->gateway, nfs_ip4(192, 168, 1, 1));

    nfs_rtable_free(&rt);
}

/* ---- empty table ---- */

static void test_empty_table(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    ASSERT_TRUE(nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 0, 1)) == NULL);

    nfs_rtable_free(&rt);
}

/* ---- format ---- */

static void test_format(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    nfs_rtable_add(&rt, 0, 0, nfs_ip4(192, 168, 1, 1), 0, 100, NFS_RTF_UP | NFS_RTF_GATEWAY);
    nfs_rtable_add(&rt, nfs_ip4(192, 168, 1, 0), 24, 0, 0, 0, NFS_RTF_UP);

    char buf[1024];
    int n = nfs_rtable_format(&rt, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strstr(buf, "default") != NULL);
    ASSERT_TRUE(strstr(buf, "192.168.1.0/24") != NULL);

    nfs_rtable_free(&rt);
}

/* ---- capacity full ---- */

static void test_capacity(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 2);

    ASSERT_EQ(nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 0), 8, 0, 0, 10, NFS_RTF_UP), 0);
    ASSERT_EQ(nfs_rtable_add(&rt, nfs_ip4(172, 16, 0, 0), 12, 0, 1, 10, NFS_RTF_UP), 0);
    ASSERT_EQ(nfs_rtable_add(&rt, nfs_ip4(192, 168, 0, 0), 16, 0, 2, 10, NFS_RTF_UP), -1);

    nfs_rtable_free(&rt);
}

/* ---- down route not matched ---- */

static void test_down_route_not_matched(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    /* Route without NFS_RTF_UP */
    nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 0), 8, 0, 0, 10, 0);

    ASSERT_TRUE(nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 0, 1)) == NULL);

    nfs_rtable_free(&rt);
}

/* ---- prefix mask helper ---- */

static void test_prefix_mask(void) {
    ASSERT_EQ(nfs_prefix_mask(0), 0x00000000);
    ASSERT_EQ(nfs_prefix_mask(8), 0xFF000000);
    ASSERT_EQ(nfs_prefix_mask(16), 0xFFFF0000);
    ASSERT_EQ(nfs_prefix_mask(24), 0xFFFFFF00);
    ASSERT_EQ(nfs_prefix_mask(32), 0xFFFFFFFF);
    ASSERT_EQ(nfs_prefix_mask(25), 0xFFFFFF80);
}

/* ---- ip4 helper ---- */

static void test_ip4_helper(void) {
    ASSERT_EQ(nfs_ip4(192, 168, 1, 1), 0xC0A80101);
    ASSERT_EQ(nfs_ip4(10, 0, 0, 1), 0x0A000001);
    ASSERT_EQ(nfs_ip4(0, 0, 0, 0), 0x00000000);
    ASSERT_EQ(nfs_ip4(255, 255, 255, 255), 0xFFFFFFFF);
}

/* ---- dest is masked on add ---- */

static void test_dest_masked(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    /* Adding 10.0.1.50/8 should store as 10.0.0.0/8 */
    nfs_rtable_add(&rt, nfs_ip4(10, 0, 1, 50), 8, 0, 0, 10, NFS_RTF_UP);

    const struct nfs_rt_entry *e = nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 0, 1));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->dest, nfs_ip4(10, 0, 0, 0));

    nfs_rtable_free(&rt);
}

/* ---- multiple overlapping prefixes ---- */

static void test_overlapping_prefixes(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 32);

    nfs_rtable_add(&rt, 0, 0, nfs_ip4(1, 1, 1, 1), 0, 100, NFS_RTF_UP | NFS_RTF_GATEWAY);
    nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 0), 8, nfs_ip4(2, 2, 2, 2), 1, 50,
                   NFS_RTF_UP | NFS_RTF_GATEWAY);
    nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 0), 16, nfs_ip4(3, 3, 3, 3), 2, 10,
                   NFS_RTF_UP | NFS_RTF_GATEWAY);

    const struct nfs_rt_entry *e;

    /* 10.0.5.1 -> /16 */
    e = nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 5, 1));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->prefix_len, 16);
    ASSERT_EQ(e->gateway, nfs_ip4(3, 3, 3, 3));

    /* 10.1.0.1 -> /8 */
    e = nfs_rtable_lookup(&rt, nfs_ip4(10, 1, 0, 1));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->prefix_len, 8);

    /* 8.8.8.8 -> default */
    e = nfs_rtable_lookup(&rt, nfs_ip4(8, 8, 8, 8));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->prefix_len, 0);

    nfs_rtable_free(&rt);
}

int main(void) {
    printf("Routing Table Tests\n");

    test_add_and_lookup();
    test_lpm();
    test_metric_tiebreak();
    test_gateway_flag();
    test_host_route();
    test_remove();
    test_remove_not_found();
    test_default_route();
    test_empty_table();
    test_format();
    test_capacity();
    test_down_route_not_matched();
    test_prefix_mask();
    test_ip4_helper();
    test_dest_masked();
    test_overlapping_prefixes();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
