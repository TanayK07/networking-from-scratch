/* Unit tests for nfs_pmtud functions. */

#include "../pmtud.h"
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

/* ---- Lookup unknown ---- */

static void test_lookup_unknown(void) {
    struct nfs_pmtu_cache c;
    nfs_pmtu_cache_init(&c, 4);
    ASSERT_EQ(nfs_pmtu_cache_lookup(&c, 0x0A000001), 0);
    nfs_pmtu_cache_free(&c);
}

/* ---- Update and lookup ---- */

static void test_update_and_lookup(void) {
    struct nfs_pmtu_cache c;
    nfs_pmtu_cache_init(&c, 4);
    ASSERT_EQ(nfs_pmtu_cache_update(&c, 0x0A000001, 1500, 1.0), 0);
    ASSERT_EQ(nfs_pmtu_cache_lookup(&c, 0x0A000001), 1500);
    ASSERT_EQ(c.count, 1);
    nfs_pmtu_cache_free(&c);
}

/* ---- next_lower from 1500 -> 1492 (first plateau < 1500) ---- */

static void test_next_lower_1500(void) {
    ASSERT_EQ(nfs_pmtu_next_lower(1500), 1492);
}

/* ---- next_lower from 1006 -> 508 ---- */

static void test_next_lower_1006(void) {
    ASSERT_EQ(nfs_pmtu_next_lower(1006), 508);
}

/* ---- next_lower from 68 -> 68 (minimum) ---- */

static void test_next_lower_minimum(void) {
    ASSERT_EQ(nfs_pmtu_next_lower(68), 68);
}

/* ---- next_lower from below 68 -> 68 ---- */

static void test_next_lower_below_minimum(void) {
    ASSERT_EQ(nfs_pmtu_next_lower(50), 68);
}

/* ---- Probe timer: not yet ---- */

static void test_probe_not_ready(void) {
    struct nfs_pmtu_entry e = {.dest = 1, .pmtu = 508, .last_updated = 100.0};
    ASSERT_EQ(nfs_pmtu_should_probe(&e, 200.0, 600.0), 0);
}

/* ---- Probe timer: ready ---- */

static void test_probe_ready(void) {
    struct nfs_pmtu_entry e = {.dest = 1, .pmtu = 508, .last_updated = 100.0};
    ASSERT_EQ(nfs_pmtu_should_probe(&e, 800.0, 600.0), 1);
}

/* ---- Probe timer: exactly at boundary ---- */

static void test_probe_exact_boundary(void) {
    struct nfs_pmtu_entry e = {.dest = 1, .pmtu = 508, .last_updated = 100.0};
    ASSERT_EQ(nfs_pmtu_should_probe(&e, 700.0, 600.0), 1);
}

/* ---- Cache update overwrites ---- */

static void test_cache_overwrite(void) {
    struct nfs_pmtu_cache c;
    nfs_pmtu_cache_init(&c, 4);
    nfs_pmtu_cache_update(&c, 0x0A000001, 1500, 1.0);
    nfs_pmtu_cache_update(&c, 0x0A000001, 1006, 2.0);
    ASSERT_EQ(nfs_pmtu_cache_lookup(&c, 0x0A000001), 1006);
    ASSERT_EQ(c.count, 1); /* should not duplicate */
    nfs_pmtu_cache_free(&c);
}

/* ---- Multiple destinations ---- */

static void test_multiple_destinations(void) {
    struct nfs_pmtu_cache c;
    nfs_pmtu_cache_init(&c, 8);
    nfs_pmtu_cache_update(&c, 0x0A000001, 1500, 1.0);
    nfs_pmtu_cache_update(&c, 0x0A000002, 1006, 2.0);
    nfs_pmtu_cache_update(&c, 0x0A000003, 508, 3.0);
    ASSERT_EQ(nfs_pmtu_cache_lookup(&c, 0x0A000001), 1500);
    ASSERT_EQ(nfs_pmtu_cache_lookup(&c, 0x0A000002), 1006);
    ASSERT_EQ(nfs_pmtu_cache_lookup(&c, 0x0A000003), 508);
    ASSERT_EQ(c.count, 3);
    nfs_pmtu_cache_free(&c);
}

/* ---- Capacity limit ---- */

static void test_capacity_limit(void) {
    struct nfs_pmtu_cache c;
    nfs_pmtu_cache_init(&c, 2);
    ASSERT_EQ(nfs_pmtu_cache_update(&c, 0x01, 1500, 0.0), 0);
    ASSERT_EQ(nfs_pmtu_cache_update(&c, 0x02, 1006, 0.0), 0);
    ASSERT_EQ(nfs_pmtu_cache_update(&c, 0x03, 508, 0.0), -1); /* full */
    ASSERT_EQ(c.count, 2);
    nfs_pmtu_cache_free(&c);
}

/* ---- Full plateau table traversal ---- */

static void test_full_plateau_traversal(void) {
    /* Walk the entire plateau table from 65535 down to 68. */
    uint32_t expected[] = {32000, 17914, 8166, 4352, 2002, 1492, 1006, 508, 296, 68};
    uint32_t mtu = 65535;
    for (int i = 0; i < 10; i++) {
        mtu = nfs_pmtu_next_lower(mtu);
        ASSERT_EQ(mtu, expected[i]);
    }
    /* One more: 68 -> 68 */
    ASSERT_EQ(nfs_pmtu_next_lower(68), 68);
}

/* ---- next_lower from exact plateau values ---- */

static void test_next_lower_exact_plateaus(void) {
    ASSERT_EQ(nfs_pmtu_next_lower(65535), 32000);
    ASSERT_EQ(nfs_pmtu_next_lower(32000), 17914);
    ASSERT_EQ(nfs_pmtu_next_lower(17914), 8166);
    ASSERT_EQ(nfs_pmtu_next_lower(8166), 4352);
    ASSERT_EQ(nfs_pmtu_next_lower(4352), 2002);
    ASSERT_EQ(nfs_pmtu_next_lower(2002), 1492);
    ASSERT_EQ(nfs_pmtu_next_lower(1492), 1006);
    ASSERT_EQ(nfs_pmtu_next_lower(296), 68);
}

/* ---- next_lower from non-plateau values ---- */

static void test_next_lower_non_plateau(void) {
    /* 9000 (jumbo frame) -> 8166 */
    ASSERT_EQ(nfs_pmtu_next_lower(9000), 8166);
    /* 1500 (Ethernet) -> 1492 (PPPoE plateau) */
    ASSERT_EQ(nfs_pmtu_next_lower(1500), 1492);
    /* 600 -> 508 */
    ASSERT_EQ(nfs_pmtu_next_lower(600), 508);
    /* 100 -> 68 */
    ASSERT_EQ(nfs_pmtu_next_lower(100), 68);
    /* 70000 (above max) -> 65535 */
    ASSERT_EQ(nfs_pmtu_next_lower(70000), 65535);
}

/* ---- Format output ---- */

static void test_format_output(void) {
    struct nfs_pmtu_cache c;
    nfs_pmtu_cache_init(&c, 4);
    nfs_pmtu_cache_update(&c, 0x0A000001, 1500, 1.0);

    char buf[512];
    nfs_pmtu_format(&c, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "1 entries") != NULL);
    ASSERT_TRUE(strstr(buf, "MTU 1500") != NULL);
    ASSERT_TRUE(strstr(buf, "10.0.0.1") != NULL);
    nfs_pmtu_cache_free(&c);
}

/* ---- Update timestamp ---- */

static void test_update_changes_timestamp(void) {
    struct nfs_pmtu_cache c;
    nfs_pmtu_cache_init(&c, 4);
    nfs_pmtu_cache_update(&c, 0x01, 1500, 10.0);
    nfs_pmtu_cache_update(&c, 0x01, 1006, 20.0);
    /* Verify the entry's timestamp was updated by checking probe logic */
    ASSERT_EQ(nfs_pmtu_should_probe(&c.entries[0], 25.0, 600.0), 0);
    ASSERT_EQ(nfs_pmtu_should_probe(&c.entries[0], 620.0, 600.0), 1);
    nfs_pmtu_cache_free(&c);
}

int main(void) {
    printf("Path MTU Discovery Tests\n");

    test_lookup_unknown();
    test_update_and_lookup();
    test_next_lower_1500();
    test_next_lower_1006();
    test_next_lower_minimum();
    test_next_lower_below_minimum();
    test_probe_not_ready();
    test_probe_ready();
    test_probe_exact_boundary();
    test_cache_overwrite();
    test_multiple_destinations();
    test_capacity_limit();
    test_full_plateau_traversal();
    test_next_lower_exact_plateaus();
    test_next_lower_non_plateau();
    test_format_output();
    test_update_changes_timestamp();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
