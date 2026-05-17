/* Unit tests for the ARP cache. */

#include "../arp_cache.h"

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

/* Helper: pack an IPv4 address. */
static uint32_t ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
}

/* ------------------------------------------------------------------ */
/*  Test: init + free                                                  */
/* ------------------------------------------------------------------ */
static void test_init_free(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 8);
    ASSERT_EQ(c.count, 0);
    ASSERT_EQ(c.capacity, 8);
    ASSERT_TRUE(c.entries != NULL);
    nfs_arp_cache_free(&c);
    ASSERT_TRUE(c.entries == NULL);
    ASSERT_EQ(c.count, 0);
    ASSERT_EQ(c.capacity, 0);
}

/* ------------------------------------------------------------------ */
/*  Test: insert + lookup                                              */
/* ------------------------------------------------------------------ */
static void test_insert_lookup(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 8);

    uint8_t mac1[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    ASSERT_EQ(nfs_arp_cache_insert(&c, ip(192, 168, 1, 1), mac1, NFS_ARP_REACHABLE), 0);
    ASSERT_EQ(c.count, 1);

    const struct nfs_arp_entry *e = nfs_arp_cache_lookup(&c, ip(192, 168, 1, 1));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->ip, ip(192, 168, 1, 1));
    ASSERT_TRUE(memcmp(e->mac, mac1, 6) == 0);
    ASSERT_EQ(e->state, NFS_ARP_REACHABLE);

    /* Lookup of non-existent IP. */
    ASSERT_TRUE(nfs_arp_cache_lookup(&c, ip(10, 0, 0, 1)) == NULL);

    nfs_arp_cache_free(&c);
}

/* ------------------------------------------------------------------ */
/*  Test: insert duplicate updates                                     */
/* ------------------------------------------------------------------ */
static void test_insert_duplicate(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 8);

    uint8_t mac1[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t mac2[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    ASSERT_EQ(nfs_arp_cache_insert(&c, ip(10, 0, 0, 1), mac1, NFS_ARP_INCOMPLETE), 0);
    ASSERT_EQ(c.count, 1);

    /* Re-insert same IP with new MAC and state. */
    ASSERT_EQ(nfs_arp_cache_insert(&c, ip(10, 0, 0, 1), mac2, NFS_ARP_REACHABLE), 0);
    ASSERT_EQ(c.count, 1); /* count should NOT increase */

    const struct nfs_arp_entry *e = nfs_arp_cache_lookup(&c, ip(10, 0, 0, 1));
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(memcmp(e->mac, mac2, 6) == 0);
    ASSERT_EQ(e->state, NFS_ARP_REACHABLE);

    nfs_arp_cache_free(&c);
}

/* ------------------------------------------------------------------ */
/*  Test: remove                                                       */
/* ------------------------------------------------------------------ */
static void test_remove(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 8);

    uint8_t mac[6] = {0};
    nfs_arp_cache_insert(&c, ip(192, 168, 1, 1), mac, NFS_ARP_REACHABLE);
    nfs_arp_cache_insert(&c, ip(192, 168, 1, 2), mac, NFS_ARP_REACHABLE);
    nfs_arp_cache_insert(&c, ip(192, 168, 1, 3), mac, NFS_ARP_REACHABLE);
    ASSERT_EQ(c.count, 3);

    ASSERT_EQ(nfs_arp_cache_remove(&c, ip(192, 168, 1, 2)), 0);
    ASSERT_EQ(c.count, 2);
    ASSERT_TRUE(nfs_arp_cache_lookup(&c, ip(192, 168, 1, 2)) == NULL);

    /* Other entries still present. */
    ASSERT_TRUE(nfs_arp_cache_lookup(&c, ip(192, 168, 1, 1)) != NULL);
    ASSERT_TRUE(nfs_arp_cache_lookup(&c, ip(192, 168, 1, 3)) != NULL);

    /* Remove non-existent. */
    ASSERT_EQ(nfs_arp_cache_remove(&c, ip(10, 0, 0, 99)), -1);

    nfs_arp_cache_free(&c);
}

/* ------------------------------------------------------------------ */
/*  Test: capacity limits                                              */
/* ------------------------------------------------------------------ */
static void test_capacity(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 3);

    uint8_t mac[6] = {0};
    ASSERT_EQ(nfs_arp_cache_insert(&c, ip(1, 0, 0, 1), mac, NFS_ARP_REACHABLE), 0);
    ASSERT_EQ(nfs_arp_cache_insert(&c, ip(1, 0, 0, 2), mac, NFS_ARP_REACHABLE), 0);
    ASSERT_EQ(nfs_arp_cache_insert(&c, ip(1, 0, 0, 3), mac, NFS_ARP_REACHABLE), 0);
    ASSERT_EQ(c.count, 3);

    /* Cache is full — new entry should fail. */
    ASSERT_EQ(nfs_arp_cache_insert(&c, ip(1, 0, 0, 4), mac, NFS_ARP_REACHABLE), -1);
    ASSERT_EQ(c.count, 3);

    /* Updating existing IP should still succeed. */
    uint8_t mac2[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ASSERT_EQ(nfs_arp_cache_insert(&c, ip(1, 0, 0, 2), mac2, NFS_ARP_STALE), 0);
    ASSERT_EQ(c.count, 3);

    const struct nfs_arp_entry *e = nfs_arp_cache_lookup(&c, ip(1, 0, 0, 2));
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(memcmp(e->mac, mac2, 6) == 0);

    nfs_arp_cache_free(&c);
}

/* ------------------------------------------------------------------ */
/*  Test: state transitions via insert                                 */
/* ------------------------------------------------------------------ */
static void test_state_transitions(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 8);

    uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

    /* Start incomplete, transition to reachable. */
    nfs_arp_cache_insert(&c, ip(10, 0, 0, 1), mac, NFS_ARP_INCOMPLETE);
    const struct nfs_arp_entry *e = nfs_arp_cache_lookup(&c, ip(10, 0, 0, 1));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->state, NFS_ARP_INCOMPLETE);

    nfs_arp_cache_insert(&c, ip(10, 0, 0, 1), mac, NFS_ARP_REACHABLE);
    e = nfs_arp_cache_lookup(&c, ip(10, 0, 0, 1));
    ASSERT_EQ(e->state, NFS_ARP_REACHABLE);

    nfs_arp_cache_insert(&c, ip(10, 0, 0, 1), mac, NFS_ARP_STALE);
    e = nfs_arp_cache_lookup(&c, ip(10, 0, 0, 1));
    ASSERT_EQ(e->state, NFS_ARP_STALE);

    nfs_arp_cache_insert(&c, ip(10, 0, 0, 1), mac, NFS_ARP_FAILED);
    e = nfs_arp_cache_lookup(&c, ip(10, 0, 0, 1));
    ASSERT_EQ(e->state, NFS_ARP_FAILED);

    nfs_arp_cache_free(&c);
}

/* ------------------------------------------------------------------ */
/*  Test: aging — REACHABLE -> STALE                                   */
/* ------------------------------------------------------------------ */
static void test_aging_reachable_to_stale(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 8);

    uint8_t mac[6] = {0};
    nfs_arp_cache_insert(&c, ip(192, 168, 1, 1), mac, NFS_ARP_REACHABLE);

    /* Fake the timestamp to be old. */
    c.entries[0].timestamp = time(NULL) - 120;

    int changed = nfs_arp_cache_age(&c, 60);
    ASSERT_TRUE(changed >= 1);
    ASSERT_EQ(c.count, 1);

    const struct nfs_arp_entry *e = nfs_arp_cache_lookup(&c, ip(192, 168, 1, 1));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->state, NFS_ARP_STALE);

    nfs_arp_cache_free(&c);
}

/* ------------------------------------------------------------------ */
/*  Test: aging — STALE -> removed                                     */
/* ------------------------------------------------------------------ */
static void test_aging_stale_removed(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 8);

    uint8_t mac[6] = {0};
    nfs_arp_cache_insert(&c, ip(192, 168, 1, 1), mac, NFS_ARP_STALE);
    c.entries[0].timestamp = time(NULL) - 120;

    int changed = nfs_arp_cache_age(&c, 60);
    ASSERT_TRUE(changed >= 1);
    ASSERT_EQ(c.count, 0);
    ASSERT_TRUE(nfs_arp_cache_lookup(&c, ip(192, 168, 1, 1)) == NULL);

    nfs_arp_cache_free(&c);
}

/* ------------------------------------------------------------------ */
/*  Test: aging — fresh entries untouched                              */
/* ------------------------------------------------------------------ */
static void test_aging_fresh_untouched(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 8);

    uint8_t mac[6] = {0};
    nfs_arp_cache_insert(&c, ip(10, 0, 0, 1), mac, NFS_ARP_REACHABLE);
    /* timestamp is "now", so a 60-second timeout won't affect it. */

    int changed = nfs_arp_cache_age(&c, 60);
    ASSERT_EQ(changed, 0);
    ASSERT_EQ(c.count, 1);

    const struct nfs_arp_entry *e = nfs_arp_cache_lookup(&c, ip(10, 0, 0, 1));
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(e->state, NFS_ARP_REACHABLE);

    nfs_arp_cache_free(&c);
}

/* ------------------------------------------------------------------ */
/*  Test: state names                                                  */
/* ------------------------------------------------------------------ */
static void test_state_names(void) {
    ASSERT_TRUE(strcmp(nfs_arp_state_name(NFS_ARP_INCOMPLETE), "INCOMPLETE") == 0);
    ASSERT_TRUE(strcmp(nfs_arp_state_name(NFS_ARP_REACHABLE), "REACHABLE") == 0);
    ASSERT_TRUE(strcmp(nfs_arp_state_name(NFS_ARP_STALE), "STALE") == 0);
    ASSERT_TRUE(strcmp(nfs_arp_state_name(NFS_ARP_FAILED), "FAILED") == 0);
    ASSERT_TRUE(strcmp(nfs_arp_state_name(99), "UNKNOWN") == 0);
}

/* ------------------------------------------------------------------ */
/*  Test: format                                                       */
/* ------------------------------------------------------------------ */
static void test_format(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 8);

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    nfs_arp_cache_insert(&c, ip(192, 168, 1, 1), mac, NFS_ARP_REACHABLE);

    char buf[256];
    nfs_arp_cache_format(&c, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "192.168.1.1") != NULL);
    ASSERT_TRUE(strstr(buf, "aa:bb:cc:dd:ee:ff") != NULL);
    ASSERT_TRUE(strstr(buf, "REACHABLE") != NULL);

    nfs_arp_cache_free(&c);
}

/* ------------------------------------------------------------------ */
/*  Test: multiple inserts and lookups                                 */
/* ------------------------------------------------------------------ */
static void test_multiple(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 16);

    uint8_t mac[6] = {0};
    for (int i = 1; i <= 10; i++) {
        mac[5] = (uint8_t)i;
        ASSERT_EQ(nfs_arp_cache_insert(&c, ip(10, 0, 0, (uint8_t)i), mac, NFS_ARP_REACHABLE), 0);
    }
    ASSERT_EQ(c.count, 10);

    for (int i = 1; i <= 10; i++) {
        const struct nfs_arp_entry *e = nfs_arp_cache_lookup(&c, ip(10, 0, 0, (uint8_t)i));
        ASSERT_TRUE(e != NULL);
        ASSERT_EQ(e->mac[5], (uint8_t)i);
    }

    nfs_arp_cache_free(&c);
}

/* ------------------------------------------------------------------ */
/*  Test: remove then re-insert                                        */
/* ------------------------------------------------------------------ */
static void test_remove_reinsert(void) {
    struct nfs_arp_cache c;
    nfs_arp_cache_init(&c, 4);

    uint8_t mac1[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t mac2[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    nfs_arp_cache_insert(&c, ip(10, 0, 0, 1), mac1, NFS_ARP_REACHABLE);
    nfs_arp_cache_remove(&c, ip(10, 0, 0, 1));
    ASSERT_EQ(c.count, 0);

    nfs_arp_cache_insert(&c, ip(10, 0, 0, 1), mac2, NFS_ARP_STALE);
    ASSERT_EQ(c.count, 1);
    const struct nfs_arp_entry *e = nfs_arp_cache_lookup(&c, ip(10, 0, 0, 1));
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(memcmp(e->mac, mac2, 6) == 0);

    nfs_arp_cache_free(&c);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_init_free();
    test_insert_lookup();
    test_insert_duplicate();
    test_remove();
    test_capacity();
    test_state_transitions();
    test_aging_reachable_to_stale();
    test_aging_stale_removed();
    test_aging_fresh_untouched();
    test_state_names();
    test_format();
    test_multiple();
    test_remove_reinsert();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
