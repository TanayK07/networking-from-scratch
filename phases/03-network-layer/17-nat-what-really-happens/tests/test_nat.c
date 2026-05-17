/* Tests for NAT translation table. */

#include "../nat.h"

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

#define INT_IP  0xC0A80001 /* 192.168.0.1 */
#define INT_IP2 0xC0A80002 /* 192.168.0.2 */
#define EXT_IP  0x0A000001 /* 10.0.0.1 */
#define REM_IP  0x08080808 /* 8.8.8.8 */
#define REM_IP2 0x01010101 /* 1.1.1.1 */

static void test_outbound_basic(void) {
    printf("  outbound basic...");
    struct nfs_nat_table t;
    nfs_nat_init(&t, 8, EXT_IP, 10000);

    uint32_t new_ip;
    uint16_t new_port;
    ASSERT_EQ(nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 80, NFS_PROTO_TCP, &new_ip, &new_port), 0);
    ASSERT_EQ(new_ip, EXT_IP);
    ASSERT_EQ(new_port, 10000);
    ASSERT_EQ(t.count, 1);

    nfs_nat_free(&t);
    printf(" OK\n");
}

static void test_outbound_reuse(void) {
    printf("  outbound reuse same flow...");
    struct nfs_nat_table t;
    nfs_nat_init(&t, 8, EXT_IP, 10000);

    uint32_t ip1, ip2;
    uint16_t p1, p2;
    nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 80, NFS_PROTO_TCP, &ip1, &p1);
    nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 80, NFS_PROTO_TCP, &ip2, &p2);
    ASSERT_EQ(p1, p2);
    ASSERT_EQ(t.count, 1);

    nfs_nat_free(&t);
    printf(" OK\n");
}

static void test_outbound_different_flows(void) {
    printf("  different flows get different ports...");
    struct nfs_nat_table t;
    nfs_nat_init(&t, 8, EXT_IP, 10000);

    uint32_t ip;
    uint16_t p1, p2;
    nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 80, NFS_PROTO_TCP, &ip, &p1);
    nfs_nat_outbound(&t, INT_IP, 5001, REM_IP, 80, NFS_PROTO_TCP, &ip, &p2);
    ASSERT_TRUE(p1 != p2);
    ASSERT_EQ(t.count, 2);

    nfs_nat_free(&t);
    printf(" OK\n");
}

static void test_inbound_reverse(void) {
    printf("  inbound reverse lookup...");
    struct nfs_nat_table t;
    nfs_nat_init(&t, 8, EXT_IP, 10000);

    uint32_t new_ip;
    uint16_t new_port;
    nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 80, NFS_PROTO_TCP, &new_ip, &new_port);

    uint32_t orig_ip;
    uint16_t orig_port;
    ASSERT_EQ(nfs_nat_inbound(&t, REM_IP, 80, new_port, NFS_PROTO_TCP, &orig_ip, &orig_port), 0);
    ASSERT_EQ(orig_ip, INT_IP);
    ASSERT_EQ(orig_port, 5000);

    nfs_nat_free(&t);
    printf(" OK\n");
}

static void test_inbound_miss(void) {
    printf("  inbound miss...");
    struct nfs_nat_table t;
    nfs_nat_init(&t, 8, EXT_IP, 10000);

    uint32_t ip;
    uint16_t port;
    ASSERT_EQ(nfs_nat_inbound(&t, REM_IP, 80, 9999, NFS_PROTO_TCP, &ip, &port), -1);

    nfs_nat_free(&t);
    printf(" OK\n");
}

static void test_capacity_full(void) {
    printf("  capacity limit...");
    struct nfs_nat_table t;
    nfs_nat_init(&t, 2, EXT_IP, 10000);

    uint32_t ip;
    uint16_t port;
    ASSERT_EQ(nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 80, NFS_PROTO_TCP, &ip, &port), 0);
    ASSERT_EQ(nfs_nat_outbound(&t, INT_IP, 5001, REM_IP, 80, NFS_PROTO_TCP, &ip, &port), 0);
    ASSERT_EQ(nfs_nat_outbound(&t, INT_IP, 5002, REM_IP, 80, NFS_PROTO_TCP, &ip, &port), -1);

    nfs_nat_free(&t);
    printf(" OK\n");
}

static void test_expire(void) {
    printf("  expire stale entries...");
    struct nfs_nat_table t;
    nfs_nat_init(&t, 8, EXT_IP, 10000);

    uint32_t ip;
    uint16_t port;
    nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 80, NFS_PROTO_TCP, &ip, &port);
    nfs_nat_outbound(&t, INT_IP2, 6000, REM_IP, 443, NFS_PROTO_TCP, &ip, &port);
    ASSERT_EQ(t.count, 2);

    int removed = nfs_nat_expire(&t, 500.0, 300.0);
    ASSERT_EQ(removed, 2);
    ASSERT_EQ(t.count, 0);

    nfs_nat_free(&t);
    printf(" OK\n");
}

static void test_expire_selective(void) {
    printf("  expire only stale...");
    struct nfs_nat_table t;
    nfs_nat_init(&t, 8, EXT_IP, 10000);

    uint32_t ip;
    uint16_t port;
    nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 80, NFS_PROTO_TCP, &ip, &port);
    t.entries[0].last_used = 100.0;

    nfs_nat_outbound(&t, INT_IP2, 6000, REM_IP, 443, NFS_PROTO_TCP, &ip, &port);
    t.entries[1].last_used = 400.0;

    int removed = nfs_nat_expire(&t, 500.0, 300.0);
    ASSERT_EQ(removed, 1);
    ASSERT_EQ(t.count, 1);
    ASSERT_EQ(t.entries[0].internal_ip, INT_IP2);

    nfs_nat_free(&t);
    printf(" OK\n");
}

static void test_protocol_separation(void) {
    printf("  TCP and UDP separate flows...");
    struct nfs_nat_table t;
    nfs_nat_init(&t, 8, EXT_IP, 10000);

    uint32_t ip;
    uint16_t p_tcp, p_udp;
    nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 53, NFS_PROTO_TCP, &ip, &p_tcp);
    nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 53, NFS_PROTO_UDP, &ip, &p_udp);
    ASSERT_TRUE(p_tcp != p_udp);
    ASSERT_EQ(t.count, 2);

    nfs_nat_free(&t);
    printf(" OK\n");
}

static void test_multiple_internal_hosts(void) {
    printf("  multiple internal hosts...");
    struct nfs_nat_table t;
    nfs_nat_init(&t, 8, EXT_IP, 10000);

    uint32_t ip;
    uint16_t p1, p2;
    nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 80, NFS_PROTO_TCP, &ip, &p1);
    nfs_nat_outbound(&t, INT_IP2, 5000, REM_IP, 80, NFS_PROTO_TCP, &ip, &p2);
    ASSERT_TRUE(p1 != p2);

    uint32_t orig_ip;
    uint16_t orig_port;
    nfs_nat_inbound(&t, REM_IP, 80, p2, NFS_PROTO_TCP, &orig_ip, &orig_port);
    ASSERT_EQ(orig_ip, INT_IP2);
    ASSERT_EQ(orig_port, 5000);

    nfs_nat_free(&t);
    printf(" OK\n");
}

static void test_format(void) {
    printf("  format output...");
    struct nfs_nat_table t;
    nfs_nat_init(&t, 8, EXT_IP, 10000);

    uint32_t ip;
    uint16_t port;
    nfs_nat_outbound(&t, INT_IP, 5000, REM_IP, 80, NFS_PROTO_TCP, &ip, &port);

    char buf[512];
    nfs_nat_format(&t, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "1/8") != NULL);
    ASSERT_TRUE(strstr(buf, "192.168.0.1") != NULL);
    ASSERT_TRUE(strstr(buf, "10.0.0.1") != NULL);

    nfs_nat_free(&t);
    printf(" OK\n");
}

/* ------------------------------------------------------------------ */

int main(void) {
    printf("NAT translation table test suite\n");
    test_outbound_basic();
    test_outbound_reuse();
    test_outbound_different_flows();
    test_inbound_reverse();
    test_inbound_miss();
    test_capacity_full();
    test_expire();
    test_expire_selective();
    test_protocol_separation();
    test_multiple_internal_hosts();
    test_format();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
