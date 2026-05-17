/* Unit tests for nfs_fdb (MAC forwarding database). */

#include "../mac_table.h"
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

/* ---- learn + lookup ---- */

static void test_learn_and_lookup(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mac1[] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01};
    uint8_t mac2[] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x02};

    ASSERT_EQ(nfs_fdb_learn(&fdb, mac1, 1), 0);
    ASSERT_EQ(nfs_fdb_learn(&fdb, mac2, 3), 0);

    ASSERT_EQ(nfs_fdb_lookup(&fdb, mac1), 1);
    ASSERT_EQ(nfs_fdb_lookup(&fdb, mac2), 3);
    ASSERT_EQ(fdb.count, 2);

    nfs_fdb_free(&fdb);
}

/* ---- update port for same MAC ---- */

static void test_update_port(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mac[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    ASSERT_EQ(nfs_fdb_learn(&fdb, mac, 1), 0);
    ASSERT_EQ(nfs_fdb_lookup(&fdb, mac), 1);

    /* MAC moves to port 5 */
    ASSERT_EQ(nfs_fdb_learn(&fdb, mac, 5), 0);
    ASSERT_EQ(nfs_fdb_lookup(&fdb, mac), 5);

    /* Count should still be 1 -- not a duplicate */
    ASSERT_EQ(fdb.count, 1);

    nfs_fdb_free(&fdb);
}

/* ---- unknown MAC returns -1 ---- */

static void test_unknown_mac(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    ASSERT_EQ(nfs_fdb_lookup(&fdb, mac), -1);

    nfs_fdb_free(&fdb);
}

/* ---- forward known unicast ---- */

static void test_forward_known_unicast(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mac_dst[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    nfs_fdb_learn(&fdb, mac_dst, 3);

    int out[8];
    int n = nfs_fdb_forward(&fdb, mac_dst, 1, out, 8);

    ASSERT_EQ(n, 1);
    ASSERT_EQ(out[0], 3);

    nfs_fdb_free(&fdb);
}

/* ---- forward unknown floods ---- */

static void test_forward_unknown_floods(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mac_unknown[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};

    int out[8];
    int n = nfs_fdb_forward(&fdb, mac_unknown, 2, out, 4);

    /* Should flood to ports 0, 1, 3 (all except src_port 2) */
    ASSERT_EQ(n, 3);

    /* Verify src port is not in the output */
    int found_src = 0;
    for (int i = 0; i < n; i++) {
        if (out[i] == 2)
            found_src = 1;
    }
    ASSERT_EQ(found_src, 0);

    nfs_fdb_free(&fdb);
}

/* ---- don't forward back to src port ---- */

static void test_no_forward_to_src(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mac[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    nfs_fdb_learn(&fdb, mac, 3);

    /* Frame arrives on port 3, destined for MAC that is also on port 3 */
    int out[8];
    int n = nfs_fdb_forward(&fdb, mac, 3, out, 8);

    ASSERT_EQ(n, 0);

    nfs_fdb_free(&fdb);
}

/* ---- broadcast floods ---- */

static void test_broadcast_floods(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    int out[8];
    int n = nfs_fdb_forward(&fdb, bcast, 0, out, 4);

    /* Should flood to ports 1, 2, 3 (all except src_port 0) */
    ASSERT_EQ(n, 3);
    ASSERT_EQ(out[0], 1);
    ASSERT_EQ(out[1], 2);
    ASSERT_EQ(out[2], 3);

    nfs_fdb_free(&fdb);
}

/* ---- aging removes old entries ---- */

static void test_aging(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mac1[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x01};
    uint8_t mac2[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x02};

    nfs_fdb_learn(&fdb, mac1, 1);
    nfs_fdb_learn(&fdb, mac2, 2);

    /* Artificially age mac1 by setting its last_seen to the past */
    fdb.entries[0].last_seen = time(NULL) - 400;

    int removed = nfs_fdb_age(&fdb, 300);
    ASSERT_EQ(removed, 1);
    ASSERT_EQ(fdb.count, 1);

    /* mac2 should still be there */
    ASSERT_EQ(nfs_fdb_lookup(&fdb, mac2), 2);
    /* mac1 should be gone */
    ASSERT_EQ(nfs_fdb_lookup(&fdb, mac1), -1);

    nfs_fdb_free(&fdb);
}

/* ---- capacity limit ---- */

static void test_capacity_limit(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 2);

    uint8_t mac1[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    uint8_t mac2[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
    uint8_t mac3[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x03};

    ASSERT_EQ(nfs_fdb_learn(&fdb, mac1, 1), 0);
    ASSERT_EQ(nfs_fdb_learn(&fdb, mac2, 2), 0);
    ASSERT_EQ(nfs_fdb_learn(&fdb, mac3, 3), -1); /* Full */

    ASSERT_EQ(fdb.count, 2);

    nfs_fdb_free(&fdb);
}

/* ---- multiple learns, same MAC ---- */

static void test_learn_idempotent(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    ASSERT_EQ(nfs_fdb_learn(&fdb, mac, 1), 0);
    ASSERT_EQ(nfs_fdb_learn(&fdb, mac, 1), 0);
    ASSERT_EQ(nfs_fdb_learn(&fdb, mac, 1), 0);
    ASSERT_EQ(fdb.count, 1);

    nfs_fdb_free(&fdb);
}

/* ---- format outputs something sensible ---- */

static void test_format(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    nfs_fdb_learn(&fdb, mac, 7);

    char buf[1024];
    int n = nfs_fdb_format(&fdb, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strstr(buf, "aa:bb:cc:dd:ee:ff") != NULL);
    ASSERT_TRUE(strstr(buf, "7") != NULL);

    nfs_fdb_free(&fdb);
}

/* ---- multicast MAC not learned ---- */

static void test_multicast_not_learned(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    /* Multicast: bit 0 of first byte is set */
    uint8_t mcast[] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};
    ASSERT_EQ(nfs_fdb_learn(&fdb, mcast, 1), 0);
    ASSERT_EQ(fdb.count, 0); /* Not stored */
    ASSERT_EQ(nfs_fdb_lookup(&fdb, mcast), -1);

    nfs_fdb_free(&fdb);
}

/* ---- broadcast not learned ---- */

static void test_broadcast_not_learned(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t bcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ASSERT_EQ(nfs_fdb_learn(&fdb, bcast, 1), 0);
    ASSERT_EQ(fdb.count, 0);

    nfs_fdb_free(&fdb);
}

/* ---- empty table lookup ---- */

static void test_empty_lookup(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mac[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    ASSERT_EQ(nfs_fdb_lookup(&fdb, mac), -1);

    nfs_fdb_free(&fdb);
}

/* ---- age with no old entries removes nothing ---- */

static void test_age_nothing_removed(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mac[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    nfs_fdb_learn(&fdb, mac, 1);

    int removed = nfs_fdb_age(&fdb, 300);
    ASSERT_EQ(removed, 0);
    ASSERT_EQ(fdb.count, 1);

    nfs_fdb_free(&fdb);
}

/* ---- multicast dst floods ---- */

static void test_multicast_dst_floods(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, 16);

    uint8_t mcast[] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};
    int out[8];
    int n = nfs_fdb_forward(&fdb, mcast, 0, out, 4);

    /* Multicast should flood to all ports except src */
    ASSERT_EQ(n, 3);

    nfs_fdb_free(&fdb);
}

int main(void) {
    printf("MAC Forwarding Database Tests\n");

    test_learn_and_lookup();
    test_update_port();
    test_unknown_mac();
    test_forward_known_unicast();
    test_forward_unknown_floods();
    test_no_forward_to_src();
    test_broadcast_floods();
    test_aging();
    test_capacity_limit();
    test_learn_idempotent();
    test_format();
    test_multicast_not_learned();
    test_broadcast_not_learned();
    test_empty_lookup();
    test_age_nothing_removed();
    test_multicast_dst_floods();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
