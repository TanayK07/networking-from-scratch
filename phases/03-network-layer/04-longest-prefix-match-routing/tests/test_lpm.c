/*
 * test_lpm.c -- Tests for Longest-Prefix Match routing table
 */

#include "lpm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- test macros ---------------------------------------------------- */

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

/* ---- helpers -------------------------------------------------------- */

static uint32_t ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
}

/* ---- tests ---------------------------------------------------------- */

static void test_empty_table(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 8);

    const struct nfs_route *r = nfs_route_lookup(&t, ip(10, 0, 0, 1));
    ASSERT_TRUE(r == NULL);

    r = nfs_route_lookup(&t, ip(192, 168, 1, 1));
    ASSERT_TRUE(r == NULL);

    r = nfs_route_lookup(&t, ip(0, 0, 0, 0));
    ASSERT_TRUE(r == NULL);

    nfs_route_table_free(&t);
}

static void test_default_route(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 8);

    /* Default route 0.0.0.0/0 matches everything */
    ASSERT_EQ(nfs_route_add(&t, 0, 0, ip(10, 0, 0, 1), 0), 0);

    const struct nfs_route *r;
    r = nfs_route_lookup(&t, ip(1, 2, 3, 4));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->next_hop, ip(10, 0, 0, 1));
    ASSERT_EQ(r->prefix_len, 0);
    ASSERT_EQ(r->interface_id, 0);

    r = nfs_route_lookup(&t, ip(192, 168, 1, 1));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->next_hop, ip(10, 0, 0, 1));

    r = nfs_route_lookup(&t, ip(255, 255, 255, 255));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->next_hop, ip(10, 0, 0, 1));

    r = nfs_route_lookup(&t, ip(0, 0, 0, 0));
    ASSERT_TRUE(r != NULL);

    nfs_route_table_free(&t);
}

static void test_specific_beats_general(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 16);

    /* Add routes with increasing specificity */
    nfs_route_add(&t, ip(0, 0, 0, 0), 0, ip(10, 0, 0, 1), 0);   /* /0  default */
    nfs_route_add(&t, ip(10, 0, 0, 0), 8, ip(10, 0, 0, 2), 1);  /* /8  */
    nfs_route_add(&t, ip(10, 1, 0, 0), 16, ip(10, 1, 0, 2), 2); /* /16 */
    nfs_route_add(&t, ip(10, 1, 2, 0), 24, ip(10, 1, 2, 2), 3); /* /24 */

    const struct nfs_route *r;

    /* 10.1.2.42 should match /24 (longest) */
    r = nfs_route_lookup(&t, ip(10, 1, 2, 42));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 24);
    ASSERT_EQ(r->next_hop, ip(10, 1, 2, 2));
    ASSERT_EQ(r->interface_id, 3);

    /* 10.1.3.42 should match /16 */
    r = nfs_route_lookup(&t, ip(10, 1, 3, 42));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 16);
    ASSERT_EQ(r->next_hop, ip(10, 1, 0, 2));
    ASSERT_EQ(r->interface_id, 2);

    /* 10.2.0.1 should match /8 */
    r = nfs_route_lookup(&t, ip(10, 2, 0, 1));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 8);
    ASSERT_EQ(r->next_hop, ip(10, 0, 0, 2));
    ASSERT_EQ(r->interface_id, 1);

    /* 8.8.8.8 should match /0 (default) */
    r = nfs_route_lookup(&t, ip(8, 8, 8, 8));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 0);
    ASSERT_EQ(r->next_hop, ip(10, 0, 0, 1));
    ASSERT_EQ(r->interface_id, 0);

    nfs_route_table_free(&t);
}

static void test_add_and_remove(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 8);

    ASSERT_EQ(nfs_route_add(&t, ip(10, 0, 0, 0), 8, ip(10, 0, 0, 1), 1), 0);
    ASSERT_EQ(t.count, 1);

    ASSERT_EQ(nfs_route_add(&t, ip(10, 1, 0, 0), 16, ip(10, 1, 0, 1), 2), 0);
    ASSERT_EQ(t.count, 2);

    /* Remove /8 route */
    ASSERT_EQ(nfs_route_remove(&t, ip(10, 0, 0, 0), 8), 0);
    ASSERT_EQ(t.count, 1);

    /* Lookup should only find /16 now */
    const struct nfs_route *r = nfs_route_lookup(&t, ip(10, 1, 0, 42));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 16);

    /* 10.2.0.1 no longer matches (no /8, no default) */
    r = nfs_route_lookup(&t, ip(10, 2, 0, 1));
    ASSERT_TRUE(r == NULL);

    /* Remove nonexistent route fails */
    ASSERT_EQ(nfs_route_remove(&t, ip(172, 16, 0, 0), 12), -1);

    nfs_route_table_free(&t);
}

static void test_no_default_no_match(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 8);

    /* Only add a specific route */
    nfs_route_add(&t, ip(192, 168, 1, 0), 24, ip(192, 168, 1, 1), 1);

    /* An address outside that subnet should not match */
    const struct nfs_route *r = nfs_route_lookup(&t, ip(192, 168, 2, 1));
    ASSERT_TRUE(r == NULL);

    r = nfs_route_lookup(&t, ip(10, 0, 0, 1));
    ASSERT_TRUE(r == NULL);

    /* But an address inside should match */
    r = nfs_route_lookup(&t, ip(192, 168, 1, 100));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 24);

    nfs_route_table_free(&t);
}

static void test_host_route_32(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 8);

    /* Add a /24 and a /32 host route */
    nfs_route_add(&t, ip(10, 1, 2, 0), 24, ip(10, 1, 2, 1), 1);
    nfs_route_add(&t, ip(10, 1, 2, 42), 32, ip(10, 1, 2, 99), 2);

    /* Exact host match should win */
    const struct nfs_route *r = nfs_route_lookup(&t, ip(10, 1, 2, 42));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 32);
    ASSERT_EQ(r->next_hop, ip(10, 1, 2, 99));
    ASSERT_EQ(r->interface_id, 2);

    /* Other addresses in /24 use the /24 route */
    r = nfs_route_lookup(&t, ip(10, 1, 2, 43));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 24);
    ASSERT_EQ(r->next_hop, ip(10, 1, 2, 1));

    nfs_route_table_free(&t);
}

static void test_duplicate_prefix_replacement(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 8);

    /* Add a route */
    nfs_route_add(&t, ip(10, 0, 0, 0), 8, ip(10, 0, 0, 1), 1);
    ASSERT_EQ(t.count, 1);

    /* Add another route with same prefix/len -- should replace */
    nfs_route_add(&t, ip(10, 0, 0, 0), 8, ip(10, 0, 0, 99), 5);
    ASSERT_EQ(t.count, 1); /* count unchanged */

    const struct nfs_route *r = nfs_route_lookup(&t, ip(10, 1, 2, 3));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->next_hop, ip(10, 0, 0, 99));
    ASSERT_EQ(r->interface_id, 5);

    nfs_route_table_free(&t);
}

static void test_table_full(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 2);

    ASSERT_EQ(nfs_route_add(&t, ip(10, 0, 0, 0), 8, ip(10, 0, 0, 1), 1), 0);
    ASSERT_EQ(nfs_route_add(&t, ip(172, 16, 0, 0), 12, ip(172, 16, 0, 1), 2), 0);
    /* Table full -- should fail */
    ASSERT_EQ(nfs_route_add(&t, ip(192, 168, 0, 0), 16, ip(192, 168, 0, 1), 3), -1);
    ASSERT_EQ(t.count, 2);

    nfs_route_table_free(&t);
}

static void test_various_prefix_lengths(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 16);

    /* /1 route: covers the top half of IP space (0.0.0.0 - 127.255.255.255) */
    nfs_route_add(&t, ip(0, 0, 0, 0), 1, ip(1, 1, 1, 1), 1);

    const struct nfs_route *r;
    r = nfs_route_lookup(&t, ip(127, 255, 255, 255));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 1);

    r = nfs_route_lookup(&t, ip(128, 0, 0, 0));
    ASSERT_TRUE(r == NULL); /* top bit is 1, doesn't match 0/1 */

    /* /31 route (point-to-point link) */
    nfs_route_add(&t, ip(10, 0, 0, 4), 31, ip(10, 0, 0, 5), 2);
    r = nfs_route_lookup(&t, ip(10, 0, 0, 4));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 31);

    r = nfs_route_lookup(&t, ip(10, 0, 0, 5));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 31);

    r = nfs_route_lookup(&t, ip(10, 0, 0, 6));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->prefix_len, 1); /* falls back to /1 */

    nfs_route_table_free(&t);
}

static void test_format(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 4);

    nfs_route_add(&t, ip(10, 0, 0, 0), 8, ip(10, 0, 0, 1), 1);

    char buf[512];
    nfs_route_table_format(&t, buf, sizeof(buf));
    ASSERT_TRUE(strlen(buf) > 0);
    ASSERT_TRUE(strstr(buf, "10.0.0.0") != NULL);

    nfs_route_table_free(&t);
}

static void test_remove_and_readd(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 8);

    nfs_route_add(&t, ip(10, 0, 0, 0), 8, ip(10, 0, 0, 1), 1);
    nfs_route_add(&t, ip(10, 1, 0, 0), 16, ip(10, 1, 0, 1), 2);
    ASSERT_EQ(t.count, 2);

    /* Remove and re-add with different next_hop */
    ASSERT_EQ(nfs_route_remove(&t, ip(10, 0, 0, 0), 8), 0);
    ASSERT_EQ(t.count, 1);

    ASSERT_EQ(nfs_route_add(&t, ip(10, 0, 0, 0), 8, ip(10, 0, 0, 99), 9), 0);
    ASSERT_EQ(t.count, 2);

    const struct nfs_route *r = nfs_route_lookup(&t, ip(10, 2, 0, 1));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->next_hop, ip(10, 0, 0, 99));
    ASSERT_EQ(r->interface_id, 9);

    nfs_route_table_free(&t);
}

static void test_overlapping_routes(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 16);

    /* Two unrelated /24 networks */
    nfs_route_add(&t, ip(10, 1, 1, 0), 24, ip(10, 1, 1, 1), 1);
    nfs_route_add(&t, ip(10, 1, 2, 0), 24, ip(10, 1, 2, 1), 2);

    const struct nfs_route *r;
    r = nfs_route_lookup(&t, ip(10, 1, 1, 50));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->interface_id, 1);

    r = nfs_route_lookup(&t, ip(10, 1, 2, 50));
    ASSERT_TRUE(r != NULL);
    ASSERT_EQ(r->interface_id, 2);

    /* Address in neither subnet */
    r = nfs_route_lookup(&t, ip(10, 1, 3, 50));
    ASSERT_TRUE(r == NULL);

    nfs_route_table_free(&t);
}

static void test_invalid_prefix_len(void) {
    struct nfs_route_table t;
    nfs_route_table_init(&t, 8);

    /* prefix_len > 32 should fail */
    ASSERT_EQ(nfs_route_add(&t, ip(10, 0, 0, 0), 33, ip(10, 0, 0, 1), 1), -1);
    ASSERT_EQ(t.count, 0);

    nfs_route_table_free(&t);
}

/* ---- main ----------------------------------------------------------- */

int main(void) {
    printf("Running LPM routing tests...\n");

    test_empty_table();
    test_default_route();
    test_specific_beats_general();
    test_add_and_remove();
    test_no_default_no_match();
    test_host_route_32();
    test_duplicate_prefix_replacement();
    test_table_full();
    test_various_prefix_lengths();
    test_format();
    test_remove_and_readd();
    test_overlapping_routes();
    test_invalid_prefix_len();

    printf("%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
