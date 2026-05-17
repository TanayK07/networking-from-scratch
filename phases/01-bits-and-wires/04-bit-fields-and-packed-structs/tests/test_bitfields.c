/* Unit tests for nfs_ipv4_hdr field extraction via shift+mask. */

#include "../bitfields.h"
#include <arpa/inet.h>
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

/* Classic IPv4 header:
 * 45 00 00 3c 1c 46 40 00 40 06 a6 ec 0a 00 02 0f ac 10 01 65
 *
 * Version=4, IHL=5, TOS=0x00, Total Len=60,
 * ID=0x1c46, Flags=0x2 (DF), Frag Offset=0,
 * TTL=64, Protocol=6 (TCP), Checksum=0xa6ec,
 * Src=10.0.2.15, Dst=172.16.1.101
 */
static const uint8_t classic_hdr[] = {0x45, 0x00, 0x00, 0x3c, 0x1c, 0x46, 0x40, 0x00, 0x40, 0x06,
                                      0xa6, 0xec, 0x0a, 0x00, 0x02, 0x0f, 0xac, 0x10, 0x01, 0x65};

static void test_struct_size(void) {
    ASSERT_EQ(sizeof(struct nfs_ipv4_hdr), 20);
}

static void test_classic_version(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    ASSERT_EQ(nfs_ipv4_version(h), 4);
}

static void test_classic_ihl(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    ASSERT_EQ(nfs_ipv4_ihl(h), 5);
}

static void test_classic_dscp(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    ASSERT_EQ(nfs_ipv4_dscp(h), 0);
}

static void test_classic_ecn(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    ASSERT_EQ(nfs_ipv4_ecn(h), 0);
}

static void test_classic_total_len(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    ASSERT_EQ(ntohs(h->total_len), 60);
}

static void test_classic_ident(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    ASSERT_EQ(ntohs(h->ident), 0x1c46);
}

static void test_classic_flags(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    /* 0x4000 in network order = flags=0x2 (DF set) */
    ASSERT_EQ(nfs_ipv4_flags(h), 2);
}

static void test_classic_frag_offset(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    ASSERT_EQ(nfs_ipv4_frag_offset(h), 0);
}

static void test_classic_ttl(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    ASSERT_EQ(h->ttl, 64);
}

static void test_classic_protocol(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    ASSERT_EQ(h->protocol, 6);
}

static void test_classic_checksum(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    ASSERT_EQ(ntohs(h->checksum), 0xa6ec);
}

static void test_classic_src_addr(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    /* 10.0.2.15 = 0x0a00020f */
    ASSERT_EQ(ntohl(h->src_addr), 0x0a00020fu);
}

static void test_classic_dst_addr(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    /* 172.16.1.101 = 0xac100165 */
    ASSERT_EQ(ntohl(h->dst_addr), 0xac100165u);
}

static void test_set_ver_ihl(void) {
    struct nfs_ipv4_hdr h;
    memset(&h, 0, sizeof(h));

    nfs_ipv4_set_ver_ihl(&h, 4, 5);
    ASSERT_EQ(nfs_ipv4_version(&h), 4);
    ASSERT_EQ(nfs_ipv4_ihl(&h), 5);
    ASSERT_EQ(h.ver_ihl, 0x45);
}

static void test_set_ver_ihl_ipv6_like(void) {
    struct nfs_ipv4_hdr h;
    memset(&h, 0, sizeof(h));

    nfs_ipv4_set_ver_ihl(&h, 6, 0);
    ASSERT_EQ(nfs_ipv4_version(&h), 6);
    ASSERT_EQ(nfs_ipv4_ihl(&h), 0);
    ASSERT_EQ(h.ver_ihl, 0x60);
}

static void test_set_ver_ihl_max(void) {
    struct nfs_ipv4_hdr h;
    memset(&h, 0, sizeof(h));

    nfs_ipv4_set_ver_ihl(&h, 15, 15);
    ASSERT_EQ(nfs_ipv4_version(&h), 15);
    ASSERT_EQ(nfs_ipv4_ihl(&h), 15);
    ASSERT_EQ(h.ver_ihl, 0xFF);
}

static void test_dscp_ecn_extraction(void) {
    /* TOS byte = 0xFC → DSCP=63, ECN=0 */
    struct nfs_ipv4_hdr h;
    memset(&h, 0, sizeof(h));
    h.tos = 0xFC;
    ASSERT_EQ(nfs_ipv4_dscp(&h), 63);
    ASSERT_EQ(nfs_ipv4_ecn(&h), 0);

    /* TOS byte = 0x03 → DSCP=0, ECN=3 */
    h.tos = 0x03;
    ASSERT_EQ(nfs_ipv4_dscp(&h), 0);
    ASSERT_EQ(nfs_ipv4_ecn(&h), 3);

    /* TOS byte = 0xFF → DSCP=63, ECN=3 */
    h.tos = 0xFF;
    ASSERT_EQ(nfs_ipv4_dscp(&h), 63);
    ASSERT_EQ(nfs_ipv4_ecn(&h), 3);

    /* TOS byte = 0xB8 (EF DSCP = 46, ECN=0) → DSCP=46, ECN=0 */
    h.tos = 0xB8;
    ASSERT_EQ(nfs_ipv4_dscp(&h), 46);
    ASSERT_EQ(nfs_ipv4_ecn(&h), 0);
}

static void test_flags_all_set(void) {
    struct nfs_ipv4_hdr h;
    memset(&h, 0, sizeof(h));
    /* flags=7 (all 3 bits set), frag_offset=0 → host=0xE000 */
    h.flags_frag = htons(0xE000);
    ASSERT_EQ(nfs_ipv4_flags(&h), 7);
    ASSERT_EQ(nfs_ipv4_frag_offset(&h), 0);
}

static void test_frag_offset_max(void) {
    struct nfs_ipv4_hdr h;
    memset(&h, 0, sizeof(h));
    /* flags=0, frag_offset=0x1FFF (max 13-bit value) */
    h.flags_frag = htons(0x1FFF);
    ASSERT_EQ(nfs_ipv4_flags(&h), 0);
    ASSERT_EQ(nfs_ipv4_frag_offset(&h), 0x1FFF);
}

static void test_flags_and_frag_combined(void) {
    struct nfs_ipv4_hdr h;
    memset(&h, 0, sizeof(h));
    /* flags=5 (101), frag_offset=1234 */
    uint16_t val = (uint16_t)((5u << 13) | 1234u);
    h.flags_frag = htons(val);
    ASSERT_EQ(nfs_ipv4_flags(&h), 5);
    ASSERT_EQ(nfs_ipv4_frag_offset(&h), 1234);
}

static void test_format_output(void) {
    const struct nfs_ipv4_hdr *h = (const struct nfs_ipv4_hdr *)classic_hdr;
    char buf[512];
    nfs_ipv4_format(h, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "ver=4") != NULL);
    ASSERT_TRUE(strstr(buf, "ihl=5") != NULL);
    ASSERT_TRUE(strstr(buf, "ttl=64") != NULL);
    ASSERT_TRUE(strstr(buf, "proto=6") != NULL);
    ASSERT_TRUE(strstr(buf, "10.0.2.15") != NULL);
    ASSERT_TRUE(strstr(buf, "172.16.1.101") != NULL);
}

static void test_set_get_roundtrip(void) {
    /* Build a header from scratch, verify all fields */
    struct nfs_ipv4_hdr h;
    memset(&h, 0, sizeof(h));

    nfs_ipv4_set_ver_ihl(&h, 4, 5);
    h.tos = (uint8_t)((10 << 2) | 1); /* DSCP=10, ECN=1 */
    h.total_len = htons(100);
    h.ident = htons(0xABCD);
    h.flags_frag = htons((uint16_t)((2u << 13) | 500u)); /* DF, offset=500 */
    h.ttl = 128;
    h.protocol = 17; /* UDP */
    h.checksum = htons(0x1234);
    h.src_addr = htonl(0xC0A80001u); /* 192.168.0.1 */
    h.dst_addr = htonl(0xC0A800FEu); /* 192.168.0.254 */

    ASSERT_EQ(nfs_ipv4_version(&h), 4);
    ASSERT_EQ(nfs_ipv4_ihl(&h), 5);
    ASSERT_EQ(nfs_ipv4_dscp(&h), 10);
    ASSERT_EQ(nfs_ipv4_ecn(&h), 1);
    ASSERT_EQ(ntohs(h.total_len), 100);
    ASSERT_EQ(ntohs(h.ident), 0xABCD);
    ASSERT_EQ(nfs_ipv4_flags(&h), 2);
    ASSERT_EQ(nfs_ipv4_frag_offset(&h), 500);
    ASSERT_EQ(h.ttl, 128);
    ASSERT_EQ(h.protocol, 17);
    ASSERT_EQ(ntohs(h.checksum), 0x1234);
    ASSERT_EQ(ntohl(h.src_addr), 0xC0A80001u);
    ASSERT_EQ(ntohl(h.dst_addr), 0xC0A800FEu);
}

int main(void) {
    printf("IPv4 Bitfield Tests\n");

    test_struct_size();
    test_classic_version();
    test_classic_ihl();
    test_classic_dscp();
    test_classic_ecn();
    test_classic_total_len();
    test_classic_ident();
    test_classic_flags();
    test_classic_frag_offset();
    test_classic_ttl();
    test_classic_protocol();
    test_classic_checksum();
    test_classic_src_addr();
    test_classic_dst_addr();
    test_set_ver_ihl();
    test_set_ver_ihl_ipv6_like();
    test_set_ver_ihl_max();
    test_dscp_ecn_extraction();
    test_flags_all_set();
    test_frag_offset_max();
    test_flags_and_frag_combined();
    test_format_output();
    test_set_get_roundtrip();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
