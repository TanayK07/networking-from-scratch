/* Tests for IPv6 header parsing and building (RFC 8200). */

#include "../ipv6.h"

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

static void test_struct_size(void) {
    printf("  struct size...");
    ASSERT_EQ(sizeof(struct nfs_ipv6_hdr), 40);
    printf(" OK\n");
}

static void test_build_basic(void) {
    printf("  build basic...");
    uint8_t src[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01};
    uint8_t dst[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x02};
    uint8_t buf[40];

    int ret = nfs_ipv6_build(0, 0, 6, 64, src, dst, 100, buf, sizeof(buf));
    ASSERT_EQ(ret, 40);

    struct nfs_ipv6_hdr hdr;
    ASSERT_EQ(nfs_ipv6_parse(buf, 40, &hdr), 0);
    ASSERT_EQ(nfs_ipv6_version(&hdr), 6);
    ASSERT_EQ(hdr.next_hdr, 6);
    ASSERT_EQ(hdr.hop_limit, 64);
    printf(" OK\n");
}

static void test_build_parse_roundtrip(void) {
    printf("  build+parse roundtrip...");
    uint8_t src[16] = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01};
    uint8_t dst[16] = {0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01};
    uint8_t buf[40];

    ASSERT_EQ(nfs_ipv6_build(0xAB, 0x12345, 17, 128, src, dst, 500, buf, sizeof(buf)), 40);

    struct nfs_ipv6_hdr hdr;
    ASSERT_EQ(nfs_ipv6_parse(buf, 40, &hdr), 0);
    ASSERT_EQ(nfs_ipv6_version(&hdr), 6);
    ASSERT_EQ(nfs_ipv6_traffic_class(&hdr), 0xAB);
    ASSERT_EQ(nfs_ipv6_flow_label(&hdr), 0x12345);
    ASSERT_EQ(hdr.next_hdr, 17);
    ASSERT_EQ(hdr.hop_limit, 128);
    ASSERT_TRUE(memcmp(hdr.src, src, 16) == 0);
    ASSERT_TRUE(memcmp(hdr.dst, dst, 16) == 0);
    printf(" OK\n");
}

static void test_version_extraction(void) {
    printf("  version extraction...");
    uint8_t src[16] = {0};
    uint8_t dst[16] = {0};
    uint8_t buf[40];

    nfs_ipv6_build(0, 0, 0, 0, src, dst, 0, buf, sizeof(buf));
    struct nfs_ipv6_hdr hdr;
    nfs_ipv6_parse(buf, 40, &hdr);
    ASSERT_EQ(nfs_ipv6_version(&hdr), 6);
    printf(" OK\n");
}

static void test_traffic_class(void) {
    printf("  traffic class...");
    uint8_t src[16] = {0};
    uint8_t dst[16] = {0};
    uint8_t buf[40];

    nfs_ipv6_build(0x00, 0, 6, 64, src, dst, 0, buf, sizeof(buf));
    struct nfs_ipv6_hdr hdr;
    nfs_ipv6_parse(buf, 40, &hdr);
    ASSERT_EQ(nfs_ipv6_traffic_class(&hdr), 0x00);

    nfs_ipv6_build(0xFF, 0, 6, 64, src, dst, 0, buf, sizeof(buf));
    nfs_ipv6_parse(buf, 40, &hdr);
    ASSERT_EQ(nfs_ipv6_traffic_class(&hdr), 0xFF);

    nfs_ipv6_build(0x42, 0, 6, 64, src, dst, 0, buf, sizeof(buf));
    nfs_ipv6_parse(buf, 40, &hdr);
    ASSERT_EQ(nfs_ipv6_traffic_class(&hdr), 0x42);
    printf(" OK\n");
}

static void test_flow_label(void) {
    printf("  flow label...");
    uint8_t src[16] = {0};
    uint8_t dst[16] = {0};
    uint8_t buf[40];

    nfs_ipv6_build(0, 0x00000, 6, 64, src, dst, 0, buf, sizeof(buf));
    struct nfs_ipv6_hdr hdr;
    nfs_ipv6_parse(buf, 40, &hdr);
    ASSERT_EQ(nfs_ipv6_flow_label(&hdr), 0);

    nfs_ipv6_build(0, 0xFFFFF, 6, 64, src, dst, 0, buf, sizeof(buf));
    nfs_ipv6_parse(buf, 40, &hdr);
    ASSERT_EQ(nfs_ipv6_flow_label(&hdr), 0xFFFFF);

    nfs_ipv6_build(0, 0xABCDE, 6, 64, src, dst, 0, buf, sizeof(buf));
    nfs_ipv6_parse(buf, 40, &hdr);
    ASSERT_EQ(nfs_ipv6_flow_label(&hdr), 0xABCDE);

    /* Flow label > 20 bits should be masked */
    nfs_ipv6_build(0, 0x3ABCDE, 6, 64, src, dst, 0, buf, sizeof(buf));
    nfs_ipv6_parse(buf, 40, &hdr);
    ASSERT_EQ(nfs_ipv6_flow_label(&hdr), 0xABCDE);
    printf(" OK\n");
}

static void test_parse_rejects_short(void) {
    printf("  reject short data...");
    struct nfs_ipv6_hdr hdr;
    uint8_t buf[39] = {0};
    ASSERT_EQ(nfs_ipv6_parse(buf, 39, &hdr), -1);
    ASSERT_EQ(nfs_ipv6_parse(buf, 0, &hdr), -1);
    ASSERT_EQ(nfs_ipv6_parse(NULL, 40, &hdr), -1);
    printf(" OK\n");
}

static void test_parse_rejects_wrong_version(void) {
    printf("  reject wrong version...");
    uint8_t buf[40] = {0};
    /* Set version=4 instead of 6 */
    buf[0] = 0x40; /* version 4, tc=0, flow=0 */
    struct nfs_ipv6_hdr hdr;
    ASSERT_EQ(nfs_ipv6_parse(buf, 40, &hdr), -1);
    printf(" OK\n");
}

static void test_build_rejects_small_buf(void) {
    printf("  build rejects small buf...");
    uint8_t src[16] = {0};
    uint8_t dst[16] = {0};
    uint8_t buf[39];
    ASSERT_EQ(nfs_ipv6_build(0, 0, 6, 64, src, dst, 0, buf, 39), -1);
    ASSERT_EQ(nfs_ipv6_build(0, 0, 6, 64, src, dst, 0, NULL, 40), -1);
    printf(" OK\n");
}

static void test_addr_format(void) {
    printf("  addr format...");
    uint8_t addr[16] = {0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    char buf[64];
    int n = nfs_ipv6_addr_format(addr, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(strcmp(buf, "2001:0db8:0000:0000:0000:0000:0000:0001"), 0);

    /* All zeros */
    uint8_t zeros[16] = {0};
    nfs_ipv6_addr_format(zeros, buf, sizeof(buf));
    ASSERT_EQ(strcmp(buf, "0000:0000:0000:0000:0000:0000:0000:0000"), 0);

    /* All FF */
    uint8_t ffs[16];
    memset(ffs, 0xFF, 16);
    nfs_ipv6_addr_format(ffs, buf, sizeof(buf));
    ASSERT_EQ(strcmp(buf, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"), 0);
    printf(" OK\n");
}

static void test_addr_parse(void) {
    printf("  addr parse...");
    uint8_t addr[16];
    ASSERT_EQ(nfs_ipv6_addr_parse("2001:0db8:0000:0000:0000:0000:0000:0001", addr), 0);
    ASSERT_EQ(addr[0], 0x20);
    ASSERT_EQ(addr[1], 0x01);
    ASSERT_EQ(addr[2], 0x0d);
    ASSERT_EQ(addr[3], 0xb8);
    ASSERT_EQ(addr[14], 0x00);
    ASSERT_EQ(addr[15], 0x01);
    printf(" OK\n");
}

static void test_addr_roundtrip(void) {
    printf("  addr format+parse roundtrip...");
    uint8_t orig[16] = {0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x02, 0x1a, 0xa0, 0xff, 0xfe, 0x3b, 0x4c, 0x5d};
    char buf[64];
    nfs_ipv6_addr_format(orig, buf, sizeof(buf));

    uint8_t parsed[16];
    ASSERT_EQ(nfs_ipv6_addr_parse(buf, parsed), 0);
    ASSERT_TRUE(memcmp(orig, parsed, 16) == 0);
    printf(" OK\n");
}

static void test_addr_parse_rejects(void) {
    printf("  addr parse rejects...");
    uint8_t addr[16];
    ASSERT_EQ(nfs_ipv6_addr_parse(NULL, addr), -1);
    ASSERT_EQ(nfs_ipv6_addr_parse("", addr), -1);
    ASSERT_EQ(nfs_ipv6_addr_parse("not-an-address", addr), -1);
    ASSERT_EQ(nfs_ipv6_addr_parse("2001:0db8:0000:0000:0000:0000:0000", addr), -1);
    printf(" OK\n");
}

static void test_next_hdr_names(void) {
    printf("  next header names...");
    ASSERT_EQ(strcmp(nfs_ipv6_next_hdr_name(0), "Hop-by-Hop"), 0);
    ASSERT_EQ(strcmp(nfs_ipv6_next_hdr_name(6), "TCP"), 0);
    ASSERT_EQ(strcmp(nfs_ipv6_next_hdr_name(17), "UDP"), 0);
    ASSERT_EQ(strcmp(nfs_ipv6_next_hdr_name(43), "Routing"), 0);
    ASSERT_EQ(strcmp(nfs_ipv6_next_hdr_name(44), "Fragment"), 0);
    ASSERT_EQ(strcmp(nfs_ipv6_next_hdr_name(58), "ICMPv6"), 0);
    ASSERT_EQ(strcmp(nfs_ipv6_next_hdr_name(59), "No Next Header"), 0);
    ASSERT_EQ(strcmp(nfs_ipv6_next_hdr_name(255), "Unknown"), 0);
    printf(" OK\n");
}

static void test_tc_and_flow_combined(void) {
    printf("  TC + flow combined...");
    uint8_t src[16] = {0};
    uint8_t dst[16] = {0};
    uint8_t buf[40];

    /* TC=0xEF, flow=0x54321 — verify they don't interfere */
    nfs_ipv6_build(0xEF, 0x54321, 58, 255, src, dst, 1200, buf, sizeof(buf));
    struct nfs_ipv6_hdr hdr;
    ASSERT_EQ(nfs_ipv6_parse(buf, 40, &hdr), 0);
    ASSERT_EQ(nfs_ipv6_version(&hdr), 6);
    ASSERT_EQ(nfs_ipv6_traffic_class(&hdr), 0xEF);
    ASSERT_EQ(nfs_ipv6_flow_label(&hdr), 0x54321);
    ASSERT_EQ(hdr.hop_limit, 255);
    printf(" OK\n");
}

/* ------------------------------------------------------------------ */

int main(void) {
    printf("IPv6 header (RFC 8200) test suite\n");
    test_struct_size();
    test_build_basic();
    test_build_parse_roundtrip();
    test_version_extraction();
    test_traffic_class();
    test_flow_label();
    test_parse_rejects_short();
    test_parse_rejects_wrong_version();
    test_build_rejects_small_buf();
    test_addr_format();
    test_addr_parse();
    test_addr_roundtrip();
    test_addr_parse_rejects();
    test_next_hdr_names();
    test_tc_and_flow_combined();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
