/* Unit tests for nfs_ipv6_addr (IPv6 address operations). */

#include "../ipv6_addr.h"
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

/* ---- loopback ---- */

static void test_loopback(void) {
    uint8_t lo[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_EQ(nfs_ipv6_addr_is_loopback(lo), 1);
    ASSERT_EQ(nfs_ipv6_addr_is_unspecified(lo), 0);
    ASSERT_EQ(nfs_ipv6_addr_is_multicast(lo), 0);
    ASSERT_TRUE(strcmp(nfs_ipv6_addr_type_str(lo), "loopback") == 0);
}

/* ---- unspecified ---- */

static void test_unspecified(void) {
    uint8_t unspec[16] = {0};
    ASSERT_EQ(nfs_ipv6_addr_is_unspecified(unspec), 1);
    ASSERT_EQ(nfs_ipv6_addr_is_loopback(unspec), 0);
    ASSERT_TRUE(strcmp(nfs_ipv6_addr_type_str(unspec), "unspecified") == 0);
}

/* ---- link-local ---- */

static void test_link_local(void) {
    uint8_t ll[16] = {0xFE, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_EQ(nfs_ipv6_addr_is_link_local(ll), 1);
    ASSERT_EQ(nfs_ipv6_addr_is_multicast(ll), 0);
    ASSERT_EQ(nfs_ipv6_addr_is_global_unicast(ll), 0);
    ASSERT_TRUE(strcmp(nfs_ipv6_addr_type_str(ll), "link-local") == 0);

    /* fe80 with some bits set in second byte (within /10) */
    uint8_t ll2[16] = {0xFE, 0xBF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_EQ(nfs_ipv6_addr_is_link_local(ll2), 1);

    /* feb0 -> still link-local (bits 6-7 of second byte = 10) */
    uint8_t ll3[16] = {0xFE, 0xB0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_EQ(nfs_ipv6_addr_is_link_local(ll3), 1);
}

/* ---- not link-local ---- */

static void test_not_link_local(void) {
    /* fec0:: is NOT link-local (it would be the old site-local) */
    uint8_t not_ll[16] = {0xFE, 0xC0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_EQ(nfs_ipv6_addr_is_link_local(not_ll), 0);
}

/* ---- multicast ---- */

static void test_multicast(void) {
    uint8_t mcast[16] = {0xFF, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_EQ(nfs_ipv6_addr_is_multicast(mcast), 1);
    ASSERT_EQ(nfs_ipv6_addr_is_loopback(mcast), 0);
    ASSERT_TRUE(strcmp(nfs_ipv6_addr_type_str(mcast), "multicast") == 0);

    /* ff00:: is also multicast */
    uint8_t mcast2[16] = {0xFF, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ASSERT_EQ(nfs_ipv6_addr_is_multicast(mcast2), 1);
}

/* ---- global unicast ---- */

static void test_global_unicast(void) {
    uint8_t gu[16] = {0x20, 0x01, 0x0D, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_EQ(nfs_ipv6_addr_is_global_unicast(gu), 1);
    ASSERT_EQ(nfs_ipv6_addr_is_link_local(gu), 0);
    ASSERT_EQ(nfs_ipv6_addr_is_ula(gu), 0);
    ASSERT_TRUE(strcmp(nfs_ipv6_addr_type_str(gu), "global unicast") == 0);

    /* 3fff:: is also in 2000::/3 */
    uint8_t gu2[16] = {0x3F, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_EQ(nfs_ipv6_addr_is_global_unicast(gu2), 1);
}

/* ---- ULA ---- */

static void test_ula(void) {
    uint8_t ula_fc[16] = {0xFC, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_EQ(nfs_ipv6_addr_is_ula(ula_fc), 1);
    ASSERT_TRUE(strcmp(nfs_ipv6_addr_type_str(ula_fc), "unique local") == 0);

    uint8_t ula_fd[16] = {0xFD, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_EQ(nfs_ipv6_addr_is_ula(ula_fd), 1);

    /* fe00:: is NOT ULA */
    uint8_t not_ula[16] = {0xFE, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_EQ(nfs_ipv6_addr_is_ula(not_ula), 0);
}

/* ---- EUI-64 construction ---- */

static void test_eui64(void) {
    /* MAC: 00:1A:2B:3C:4D:5E
     * EUI-64: 02:1A:2B:FF:FE:3C:4D:5E  (bit 6 flipped: 00 -> 02) */
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t prefix[8] = {0xFE, 0x80, 0, 0, 0, 0, 0, 0};
    uint8_t addr[16];

    nfs_ipv6_addr_from_eui64(mac, prefix, addr);

    /* Check prefix */
    ASSERT_EQ(addr[0], 0xFE);
    ASSERT_EQ(addr[1], 0x80);
    ASSERT_EQ(addr[2], 0x00);
    ASSERT_EQ(addr[3], 0x00);
    ASSERT_EQ(addr[4], 0x00);
    ASSERT_EQ(addr[5], 0x00);
    ASSERT_EQ(addr[6], 0x00);
    ASSERT_EQ(addr[7], 0x00);

    /* Check interface ID */
    ASSERT_EQ(addr[8], 0x02); /* 0x00 ^ 0x02 */
    ASSERT_EQ(addr[9], 0x1A);
    ASSERT_EQ(addr[10], 0x2B);
    ASSERT_EQ(addr[11], 0xFF);
    ASSERT_EQ(addr[12], 0xFE);
    ASSERT_EQ(addr[13], 0x3C);
    ASSERT_EQ(addr[14], 0x4D);
    ASSERT_EQ(addr[15], 0x5E);

    ASSERT_EQ(nfs_ipv6_addr_is_link_local(addr), 1);
}

/* ---- EUI-64 with different MAC ---- */

static void test_eui64_bit_flip(void) {
    /* MAC: AA:BB:CC:DD:EE:FF
     * First byte 0xAA = 1010 1010, bit 6 flipped -> 1010 1000 = 0xA8 */
    uint8_t mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t prefix[8] = {0x20, 0x01, 0x0D, 0xB8, 0, 0, 0, 0};
    uint8_t addr[16];

    nfs_ipv6_addr_from_eui64(mac, prefix, addr);

    ASSERT_EQ(addr[8], 0xA8); /* 0xAA ^ 0x02 = 0xA8 */
    ASSERT_EQ(addr[9], 0xBB);
    ASSERT_EQ(addr[10], 0xCC);
    ASSERT_EQ(addr[11], 0xFF);
    ASSERT_EQ(addr[12], 0xFE);
    ASSERT_EQ(addr[13], 0xDD);
    ASSERT_EQ(addr[14], 0xEE);
    ASSERT_EQ(addr[15], 0xFF);
}

/* ---- solicited-node multicast ---- */

static void test_solicited_node(void) {
    /* For addr ending in ...3C:4D:5E, solicited-node is ff02::1:ff3c:4d5e */
    uint8_t addr[16] = {0xFE, 0x80, 0,    0,    0,    0,    0,    0,
                        0x02, 0x1A, 0x2B, 0xFF, 0xFE, 0x3C, 0x4D, 0x5E};
    uint8_t snmc[16];

    ASSERT_EQ(nfs_ipv6_addr_solicited_node(addr, snmc), 0);

    ASSERT_EQ(snmc[0], 0xFF);
    ASSERT_EQ(snmc[1], 0x02);
    /* bytes 2..10 zero */
    for (int i = 2; i <= 10; i++)
        ASSERT_EQ(snmc[i], 0x00);
    ASSERT_EQ(snmc[11], 0x01);
    ASSERT_EQ(snmc[12], 0xFF);
    ASSERT_EQ(snmc[13], 0x3C);
    ASSERT_EQ(snmc[14], 0x4D);
    ASSERT_EQ(snmc[15], 0x5E);

    ASSERT_EQ(nfs_ipv6_addr_is_multicast(snmc), 1);
}

/* ---- format + parse roundtrip ---- */

static void test_format_parse_roundtrip(void) {
    uint8_t addr[16] = {0x20, 0x01, 0x0D, 0xB8, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    char buf[64];
    ASSERT_EQ(nfs_ipv6_addr_format(addr, buf, sizeof(buf)), 0);
    ASSERT_TRUE(strcmp(buf, "2001:0db8:0000:0000:0000:0000:0000:0001") == 0);

    uint8_t parsed[16];
    ASSERT_EQ(nfs_ipv6_addr_parse(buf, parsed), 0);
    ASSERT_TRUE(memcmp(addr, parsed, 16) == 0);
}

/* ---- format loopback ---- */

static void test_format_loopback(void) {
    uint8_t lo[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    char buf[64];
    ASSERT_EQ(nfs_ipv6_addr_format(lo, buf, sizeof(buf)), 0);
    ASSERT_TRUE(strcmp(buf, "0000:0000:0000:0000:0000:0000:0000:0001") == 0);
}

/* ---- parse abbreviated groups ---- */

static void test_parse_abbreviated(void) {
    /* Allow groups with fewer than 4 digits */
    uint8_t addr[16];
    ASSERT_EQ(nfs_ipv6_addr_parse("2001:db8:0:0:0:0:0:1", addr), 0);
    ASSERT_EQ(addr[0], 0x20);
    ASSERT_EQ(addr[1], 0x01);
    ASSERT_EQ(addr[2], 0x0D);
    ASSERT_EQ(addr[3], 0xB8);
    ASSERT_EQ(addr[15], 0x01);
}

/* ---- parse invalid ---- */

static void test_parse_invalid(void) {
    uint8_t addr[16];
    ASSERT_EQ(nfs_ipv6_addr_parse(NULL, addr), -1);
    ASSERT_EQ(nfs_ipv6_addr_parse("", addr), -1);
    ASSERT_EQ(nfs_ipv6_addr_parse("zzzz:0:0:0:0:0:0:0", addr), -1);
    ASSERT_EQ(nfs_ipv6_addr_parse("2001:db8", addr), -1);
    /* Too many groups */
    ASSERT_EQ(nfs_ipv6_addr_parse("1:2:3:4:5:6:7:8:9", addr), -1);
}

/* ---- format buffer too small ---- */

static void test_format_small_buf(void) {
    uint8_t addr[16] = {0};
    char buf[10];
    ASSERT_EQ(nfs_ipv6_addr_format(addr, buf, sizeof(buf)), -1);
}

/* ---- type_str other ---- */

static void test_type_str_other(void) {
    /* 0100:: doesn't match any known prefix */
    uint8_t addr[16] = {0x01, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ASSERT_TRUE(strcmp(nfs_ipv6_addr_type_str(addr), "other") == 0);
}

/* ---- solicited-node null args ---- */

static void test_solicited_node_null(void) {
    uint8_t out[16];
    ASSERT_EQ(nfs_ipv6_addr_solicited_node(NULL, out), -1);

    uint8_t addr[16] = {0};
    ASSERT_EQ(nfs_ipv6_addr_solicited_node(addr, NULL), -1);
}

int main(void) {
    printf("IPv6 Addressing Tests\n");

    test_loopback();
    test_unspecified();
    test_link_local();
    test_not_link_local();
    test_multicast();
    test_global_unicast();
    test_ula();
    test_eui64();
    test_eui64_bit_flip();
    test_solicited_node();
    test_format_parse_roundtrip();
    test_format_loopback();
    test_parse_abbreviated();
    test_parse_invalid();
    test_format_small_buf();
    test_type_str_other();
    test_solicited_node_null();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
