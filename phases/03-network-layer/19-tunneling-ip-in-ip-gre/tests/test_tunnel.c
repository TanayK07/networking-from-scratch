/* Unit tests for IP-in-IP and GRE tunneling. */

#include "../tunnel.h"
#include <arpa/inet.h>
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

/* Helper: build a minimal inner IPv4 packet */
static int build_inner(uint8_t *buf, size_t sz) {
    if (sz < 20)
        return -1;
    struct nfs_ipv4_hdr *h = (struct nfs_ipv4_hdr *)buf;
    memset(h, 0, 20);
    h->ver_ihl = 0x45;
    h->total_len = htons(20);
    h->ttl = 64;
    h->protocol = 6;
    h->src_addr = htonl(0xC0A80101);
    h->dst_addr = htonl(0xC0A80201);
    h->checksum = nfs_ip_checksum(h, 20);
    return 20;
}

/* ---- IP checksum tests ---- */

static void test_ip_checksum_known(void) {
    /* Build a header, compute checksum, verify it */
    uint8_t inner[20];
    build_inner(inner, sizeof(inner));
    /* Zero checksum and recompute */
    struct nfs_ipv4_hdr *h = (struct nfs_ipv4_hdr *)inner;
    uint16_t saved = h->checksum;
    h->checksum = 0;
    uint16_t computed = nfs_ip_checksum(h, 20);
    ASSERT_EQ(computed, saved);
}

static void test_ip_checksum_verify(void) {
    /* Checksum of valid header should yield 0 */
    uint8_t inner[20];
    build_inner(inner, sizeof(inner));
    uint16_t csum = nfs_ip_checksum(inner, 20);
    ASSERT_EQ(csum, 0);
}

/* ---- IP-in-IP tests ---- */

static void test_ipip_encapsulate_basic(void) {
    uint8_t inner[20], out[128];
    build_inner(inner, sizeof(inner));

    int n = nfs_ipip_encapsulate(htonl(0x0A000001), htonl(0x0A000002), 128, inner, 20, out,
                                 sizeof(out));
    ASSERT_EQ(n, 40); /* 20 outer + 20 inner */

    /* Check outer header */
    struct nfs_ipv4_hdr *oh = (struct nfs_ipv4_hdr *)out;
    ASSERT_EQ(oh->ver_ihl, 0x45);
    ASSERT_EQ(oh->protocol, NFS_IPPROTO_IPIP);
    ASSERT_EQ(oh->ttl, 128);
    ASSERT_EQ(ntohs(oh->total_len), 40);
}

static void test_ipip_encap_checksum(void) {
    uint8_t inner[20], out[128];
    build_inner(inner, sizeof(inner));
    nfs_ipip_encapsulate(htonl(0x0A000001), htonl(0x0A000002), 64, inner, 20, out, sizeof(out));
    /* Outer header checksum should verify to 0 */
    ASSERT_EQ(nfs_ip_checksum(out, 20), 0);
}

static void test_ipip_roundtrip(void) {
    uint8_t inner[20], out[128];
    build_inner(inner, sizeof(inner));

    int n =
        nfs_ipip_encapsulate(htonl(0x0A000001), htonl(0x0A000002), 64, inner, 20, out, sizeof(out));
    ASSERT_TRUE(n > 0);

    size_t dec_len;
    const uint8_t *dec = nfs_ipip_decapsulate(out, (size_t)n, &dec_len);
    ASSERT_TRUE(dec != NULL);
    ASSERT_EQ(dec_len, 20u);
    ASSERT_TRUE(memcmp(dec, inner, 20) == 0);
}

static void test_ipip_decap_bad_protocol(void) {
    uint8_t inner[20], out[128];
    build_inner(inner, sizeof(inner));
    nfs_ipip_encapsulate(htonl(0x0A000001), htonl(0x0A000002), 64, inner, 20, out, sizeof(out));
    /* Corrupt protocol */
    struct nfs_ipv4_hdr *oh = (struct nfs_ipv4_hdr *)out;
    oh->protocol = 6; /* TCP, not IPIP */

    size_t dec_len;
    ASSERT_TRUE(nfs_ipip_decapsulate(out, 40, &dec_len) == NULL);
}

static void test_ipip_decap_too_short(void) {
    uint8_t buf[10] = {0x45};
    size_t dec_len;
    ASSERT_TRUE(nfs_ipip_decapsulate(buf, 10, &dec_len) == NULL);
}

static void test_ipip_encap_null(void) {
    ASSERT_EQ(nfs_ipip_encapsulate(0, 0, 64, NULL, 20, NULL, 128), -1);
}

static void test_ipip_encap_buffer_too_small(void) {
    uint8_t inner[20], out[10];
    build_inner(inner, sizeof(inner));
    ASSERT_EQ(nfs_ipip_encapsulate(0, 0, 64, inner, 20, out, sizeof(out)), -1);
}

static void test_ipip_decap_bad_version(void) {
    uint8_t buf[40] = {0};
    buf[0] = 0x65; /* version 6, not 4 */
    size_t dec_len;
    ASSERT_TRUE(nfs_ipip_decapsulate(buf, 40, &dec_len) == NULL);
}

/* ---- GRE tests ---- */

static void test_gre_basic(void) {
    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t out[64];
    int n = nfs_gre_encapsulate(0, NFS_GRE_PROTO_IPV4, 0, 0, payload, sizeof(payload), out,
                                sizeof(out));
    ASSERT_EQ(n, 8); /* 4 GRE header + 4 payload */
    /* Check protocol */
    ASSERT_EQ(out[2], 0x08);
    ASSERT_EQ(out[3], 0x00);
}

static void test_gre_with_key(void) {
    uint8_t payload[] = {0xAB};
    uint8_t out[64];
    int n = nfs_gre_encapsulate(NFS_GRE_FLAG_K, NFS_GRE_PROTO_IPV4, 42, 0, payload, 1, out,
                                sizeof(out));
    ASSERT_EQ(n, 9); /* 4 + 4 (key) + 1 payload */

    struct nfs_gre_hdr hdr;
    ASSERT_EQ(nfs_gre_parse(out, (size_t)n, &hdr), 0);
    ASSERT_TRUE(hdr.has_key);
    ASSERT_EQ(hdr.key, 42u);
    ASSERT_TRUE(!hdr.has_seq);
    ASSERT_TRUE(!hdr.has_checksum);
}

static void test_gre_with_seq(void) {
    uint8_t out[64];
    int n = nfs_gre_encapsulate(NFS_GRE_FLAG_S, NFS_GRE_PROTO_IPV6, 0, 100, (const uint8_t *)"x", 1,
                                out, sizeof(out));
    ASSERT_EQ(n, 9); /* 4 + 4 (seq) + 1 */

    struct nfs_gre_hdr hdr;
    ASSERT_EQ(nfs_gre_parse(out, (size_t)n, &hdr), 0);
    ASSERT_TRUE(hdr.has_seq);
    ASSERT_EQ(hdr.seq, 100u);
    ASSERT_EQ(hdr.protocol, NFS_GRE_PROTO_IPV6);
}

static void test_gre_with_checksum(void) {
    uint8_t payload[] = {0x45, 0x00, 0x00, 0x14};
    uint8_t out[64];
    int n = nfs_gre_encapsulate(NFS_GRE_FLAG_C, NFS_GRE_PROTO_IPV4, 0, 0, payload, sizeof(payload),
                                out, sizeof(out));
    ASSERT_EQ(n, 12); /* 4 + 4 (csum+reserved) + 4 payload */

    struct nfs_gre_hdr hdr;
    ASSERT_EQ(nfs_gre_parse(out, (size_t)n, &hdr), 0);
    ASSERT_TRUE(hdr.has_checksum);
}

static void test_gre_all_options(void) {
    uint8_t payload[] = {0x01, 0x02};
    uint8_t out[64];
    int n =
        nfs_gre_encapsulate(NFS_GRE_FLAG_C | NFS_GRE_FLAG_K | NFS_GRE_FLAG_S, NFS_GRE_PROTO_IPV4,
                            99, 7, payload, sizeof(payload), out, sizeof(out));
    /* 4 + 4 (csum) + 4 (key) + 4 (seq) + 2 = 18 */
    ASSERT_EQ(n, 18);

    struct nfs_gre_hdr hdr;
    ASSERT_EQ(nfs_gre_parse(out, (size_t)n, &hdr), 0);
    ASSERT_TRUE(hdr.has_checksum);
    ASSERT_TRUE(hdr.has_key);
    ASSERT_TRUE(hdr.has_seq);
    ASSERT_EQ(hdr.key, 99u);
    ASSERT_EQ(hdr.seq, 7u);
    ASSERT_EQ(hdr.hdr_len, 16u);
}

static void test_gre_payload_access(void) {
    uint8_t data[] = {0xDE, 0xAD};
    uint8_t out[64];
    int n = nfs_gre_encapsulate(NFS_GRE_FLAG_K, NFS_GRE_PROTO_IPV4, 1, 0, data, sizeof(data), out,
                                sizeof(out));
    ASSERT_TRUE(n > 0);

    size_t plen;
    const uint8_t *p = nfs_gre_payload(out, (size_t)n, &plen);
    ASSERT_TRUE(p != NULL);
    ASSERT_EQ(plen, 2u);
    ASSERT_EQ(p[0], 0xDE);
    ASSERT_EQ(p[1], 0xAD);
}

static void test_gre_parse_too_short(void) {
    uint8_t buf[] = {0x00, 0x00};
    struct nfs_gre_hdr hdr;
    ASSERT_EQ(nfs_gre_parse(buf, 2, &hdr), -1);
}

static void test_gre_parse_bad_version(void) {
    /* Version bits set to non-zero */
    uint8_t buf[] = {0x00, 0x01, 0x08, 0x00};
    struct nfs_gre_hdr hdr;
    ASSERT_EQ(nfs_gre_parse(buf, 4, &hdr), -1);
}

static void test_gre_parse_null(void) {
    struct nfs_gre_hdr hdr;
    ASSERT_EQ(nfs_gre_parse(NULL, 10, &hdr), -1);
    ASSERT_EQ(nfs_gre_parse((const uint8_t *)"x", 4, NULL), -1);
}

static void test_gre_encap_buffer_small(void) {
    uint8_t out[2];
    ASSERT_EQ(
        nfs_gre_encapsulate(0, NFS_GRE_PROTO_IPV4, 0, 0, (const uint8_t *)"x", 1, out, sizeof(out)),
        -1);
}

static void test_gre_empty_payload(void) {
    uint8_t out[32];
    int n = nfs_gre_encapsulate(0, NFS_GRE_PROTO_IPV4, 0, 0, NULL, 0, out, sizeof(out));
    ASSERT_EQ(n, 4);
}

int main(void) {
    printf("IP-in-IP and GRE Tunneling Tests\n");

    test_ip_checksum_known();
    test_ip_checksum_verify();
    test_ipip_encapsulate_basic();
    test_ipip_encap_checksum();
    test_ipip_roundtrip();
    test_ipip_decap_bad_protocol();
    test_ipip_decap_too_short();
    test_ipip_encap_null();
    test_ipip_encap_buffer_too_small();
    test_ipip_decap_bad_version();
    test_gre_basic();
    test_gre_with_key();
    test_gre_with_seq();
    test_gre_with_checksum();
    test_gre_all_options();
    test_gre_payload_access();
    test_gre_parse_too_short();
    test_gre_parse_bad_version();
    test_gre_parse_null();
    test_gre_encap_buffer_small();
    test_gre_empty_payload();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
