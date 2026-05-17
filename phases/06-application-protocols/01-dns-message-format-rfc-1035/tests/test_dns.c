/* Unit tests for DNS message format (RFC 1035). */

#include "../dns.h"
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

/* ---- Header size ---- */

static void test_header_size(void) {
    ASSERT_EQ(sizeof(struct nfs_dns_header), 12);
}

/* ---- Name encoding ---- */

static void test_name_encode_www_example_com(void) {
    uint8_t buf[64];
    int n = nfs_dns_name_encode("www.example.com", buf, sizeof(buf));
    /* Expected: 03 77 77 77 07 65 78 61 6d 70 6c 65 03 63 6f 6d 00 */
    ASSERT_EQ(n, 17);
    ASSERT_EQ(buf[0], 3); /* "www" length */
    ASSERT_EQ(buf[1], 'w');
    ASSERT_EQ(buf[2], 'w');
    ASSERT_EQ(buf[3], 'w');
    ASSERT_EQ(buf[4], 7); /* "example" length */
    ASSERT_EQ(buf[5], 'e');
    ASSERT_EQ(buf[11], 'e'); /* last char of "example" */
    ASSERT_EQ(buf[12], 3);   /* "com" length */
    ASSERT_EQ(buf[13], 'c');
    ASSERT_EQ(buf[14], 'o');
    ASSERT_EQ(buf[15], 'm');
    ASSERT_EQ(buf[16], 0); /* terminator */
}

static void test_name_encode_root(void) {
    uint8_t buf[8];
    int n = nfs_dns_name_encode(".", buf, sizeof(buf));
    ASSERT_EQ(n, 1);
    ASSERT_EQ(buf[0], 0);
}

static void test_name_encode_single_label(void) {
    uint8_t buf[32];
    int n = nfs_dns_name_encode("localhost", buf, sizeof(buf));
    /* 09 6c 6f 63 61 6c 68 6f 73 74 00 */
    ASSERT_EQ(n, 11);
    ASSERT_EQ(buf[0], 9);
    ASSERT_EQ(buf[1], 'l');
    ASSERT_EQ(buf[10], 0);
}

static void test_name_encode_trailing_dot(void) {
    uint8_t buf[64];
    int n = nfs_dns_name_encode("example.com.", buf, sizeof(buf));
    /* Should produce same result as without trailing dot */
    uint8_t buf2[64];
    int n2 = nfs_dns_name_encode("example.com", buf2, sizeof(buf2));
    ASSERT_EQ(n, n2);
    ASSERT_TRUE(memcmp(buf, buf2, (size_t)n) == 0);
}

/* ---- Name decoding ---- */

static void test_name_decode_roundtrip(void) {
    uint8_t wire[64];
    int enc = nfs_dns_name_encode("www.example.com", wire, sizeof(wire));
    ASSERT_TRUE(enc > 0);

    char decoded[256];
    int consumed = nfs_dns_name_decode(wire, (size_t)enc, 0, decoded, sizeof(decoded));
    ASSERT_EQ(consumed, enc);
    ASSERT_TRUE(strcmp(decoded, "www.example.com") == 0);
}

static void test_name_decode_single_label(void) {
    uint8_t wire[32];
    int enc = nfs_dns_name_encode("test", wire, sizeof(wire));
    ASSERT_TRUE(enc > 0);

    char decoded[64];
    int consumed = nfs_dns_name_decode(wire, (size_t)enc, 0, decoded, sizeof(decoded));
    ASSERT_EQ(consumed, enc);
    ASSERT_TRUE(strcmp(decoded, "test") == 0);
}

static void test_name_decode_compression_pointer(void) {
    /* Simulate a DNS packet where a name uses a compression pointer.
     * Offset 0: 07 "example" 03 "com" 00
     * That's 13 bytes. Then at offset 13 we put: 03 "www" C0 00
     * which means "www" followed by pointer to offset 0 ("example.com"). */
    uint8_t pkt[32];
    memset(pkt, 0, sizeof(pkt));

    /* First name: example.com at offset 0 */
    int off = 0;
    pkt[off++] = 7;
    memcpy(pkt + off, "example", 7);
    off += 7;
    pkt[off++] = 3;
    memcpy(pkt + off, "com", 3);
    off += 3;
    pkt[off++] = 0;
    /* off is now 13 */

    /* Second name: www + pointer to offset 0 */
    int start = off;
    pkt[off++] = 3;
    memcpy(pkt + off, "www", 3);
    off += 3;
    pkt[off++] = 0xC0; /* compression pointer */
    pkt[off++] = 0x00; /* points to offset 0 */

    char decoded[256];
    int consumed = nfs_dns_name_decode(pkt, (size_t)off, (size_t)start, decoded, sizeof(decoded));
    /* Consumed: 3("www" label) + 1(len byte) + 2(pointer) = 6 */
    ASSERT_EQ(consumed, 6);
    ASSERT_TRUE(strcmp(decoded, "www.example.com") == 0);
}

/* ---- Build query and parse header ---- */

static void test_build_query_type_a(void) {
    uint8_t buf[512];
    int total = nfs_dns_build_query("www.example.com", NFS_DNS_TYPE_A, 1, 0xABCD, buf, sizeof(buf));
    ASSERT_TRUE(total > 12);

    struct nfs_dns_header hdr;
    ASSERT_EQ(nfs_dns_parse_header(buf, (size_t)total, &hdr), 0);
    ASSERT_EQ(ntohs(hdr.id), 0xABCD);
    ASSERT_TRUE(nfs_dns_is_query(&hdr));
    ASSERT_EQ(ntohs(hdr.qdcount), 1);
    ASSERT_EQ(ntohs(hdr.ancount), 0);
    ASSERT_EQ(ntohs(hdr.nscount), 0);
    ASSERT_EQ(ntohs(hdr.arcount), 0);
    ASSERT_EQ(nfs_dns_rcode(&hdr), NFS_DNS_RCODE_NOERROR);

    /* Verify QTYPE = A (1) after the name */
    char name[256];
    int name_consumed = nfs_dns_name_decode(buf, (size_t)total, 12, name, sizeof(name));
    ASSERT_TRUE(name_consumed > 0);

    size_t qtype_off = 12 + (size_t)name_consumed;
    uint16_t qtype;
    memcpy(&qtype, buf + qtype_off, 2);
    ASSERT_EQ(ntohs(qtype), NFS_DNS_TYPE_A);

    uint16_t qclass;
    memcpy(&qclass, buf + qtype_off + 2, 2);
    ASSERT_EQ(ntohs(qclass), 1);
}

static void test_build_query_type_aaaa(void) {
    uint8_t buf[512];
    int total =
        nfs_dns_build_query("ipv6.example.org", NFS_DNS_TYPE_AAAA, 1, 0x1234, buf, sizeof(buf));
    ASSERT_TRUE(total > 12);

    /* Verify QTYPE = AAAA (28) */
    char name[256];
    int nc = nfs_dns_name_decode(buf, (size_t)total, 12, name, sizeof(name));
    ASSERT_TRUE(nc > 0);
    ASSERT_TRUE(strcmp(name, "ipv6.example.org") == 0);

    size_t qtype_off = 12 + (size_t)nc;
    uint16_t qtype;
    memcpy(&qtype, buf + qtype_off, 2);
    ASSERT_EQ(ntohs(qtype), NFS_DNS_TYPE_AAAA);
}

/* ---- QR flag ---- */

static void test_qr_flag_query(void) {
    uint8_t buf[512];
    int total = nfs_dns_build_query("test.com", NFS_DNS_TYPE_A, 1, 1, buf, sizeof(buf));
    ASSERT_TRUE(total > 0);

    struct nfs_dns_header hdr;
    nfs_dns_parse_header(buf, (size_t)total, &hdr);
    ASSERT_TRUE(nfs_dns_is_query(&hdr));
    ASSERT_TRUE(!nfs_dns_is_response(&hdr));
}

static void test_qr_flag_response(void) {
    /* Manually craft a response header */
    struct nfs_dns_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.id = htons(0x1234);
    /* QR=1 (response), RD=1, RA=1, RCODE=0 */
    hdr.flags = htons(0x8180);
    hdr.qdcount = htons(1);
    hdr.ancount = htons(1);

    ASSERT_TRUE(nfs_dns_is_response(&hdr));
    ASSERT_TRUE(!nfs_dns_is_query(&hdr));
    ASSERT_EQ(nfs_dns_rcode(&hdr), 0);
}

/* ---- RCODE extraction ---- */

static void test_rcode_nxdomain(void) {
    struct nfs_dns_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    /* QR=1, RCODE=3 (NXDOMAIN) */
    hdr.flags = htons(0x8183);
    ASSERT_EQ(nfs_dns_rcode(&hdr), NFS_DNS_RCODE_NXDOMAIN);
}

static void test_rcode_servfail(void) {
    struct nfs_dns_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.flags = htons(0x8182);
    ASSERT_EQ(nfs_dns_rcode(&hdr), NFS_DNS_RCODE_SERVFAIL);
}

static void test_rcode_refused(void) {
    struct nfs_dns_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.flags = htons(0x8185);
    ASSERT_EQ(nfs_dns_rcode(&hdr), NFS_DNS_RCODE_REFUSED);
}

/* ---- Parse header edge cases ---- */

static void test_parse_header_too_short(void) {
    uint8_t buf[8] = {0};
    struct nfs_dns_header hdr;
    ASSERT_EQ(nfs_dns_parse_header(buf, 8, &hdr), -1);
}

static void test_parse_header_exact_size(void) {
    uint8_t buf[12] = {0};
    buf[0] = 0xAB;
    buf[1] = 0xCD; /* ID */
    struct nfs_dns_header hdr;
    ASSERT_EQ(nfs_dns_parse_header(buf, 12, &hdr), 0);
    ASSERT_EQ(ntohs(hdr.id), 0xABCD);
}

/* ---- Name encoding edge cases ---- */

static void test_name_encode_empty(void) {
    uint8_t buf[8];
    int n = nfs_dns_name_encode("", buf, sizeof(buf));
    /* Empty string treated as root */
    ASSERT_EQ(n, 1);
    ASSERT_EQ(buf[0], 0);
}

static void test_name_encode_buffer_too_small(void) {
    uint8_t buf[4];
    int n = nfs_dns_name_encode("www.example.com", buf, sizeof(buf));
    ASSERT_EQ(n, -1);
}

/* ---- Type constants ---- */

static void test_type_constants(void) {
    ASSERT_EQ(NFS_DNS_TYPE_A, 1);
    ASSERT_EQ(NFS_DNS_TYPE_NS, 2);
    ASSERT_EQ(NFS_DNS_TYPE_CNAME, 5);
    ASSERT_EQ(NFS_DNS_TYPE_SOA, 6);
    ASSERT_EQ(NFS_DNS_TYPE_PTR, 12);
    ASSERT_EQ(NFS_DNS_TYPE_MX, 15);
    ASSERT_EQ(NFS_DNS_TYPE_TXT, 16);
    ASSERT_EQ(NFS_DNS_TYPE_AAAA, 28);
}

/* ---- Build query with various names ---- */

static void test_build_query_single_label(void) {
    uint8_t buf[512];
    int total = nfs_dns_build_query("localhost", NFS_DNS_TYPE_A, 1, 0x0001, buf, sizeof(buf));
    ASSERT_TRUE(total > 0);

    char name[256];
    int nc = nfs_dns_name_decode(buf, (size_t)total, 12, name, sizeof(name));
    ASSERT_TRUE(nc > 0);
    ASSERT_TRUE(strcmp(name, "localhost") == 0);
}

static void test_build_query_deep_subdomain(void) {
    uint8_t buf[512];
    int total =
        nfs_dns_build_query("a.b.c.d.e.example.com", NFS_DNS_TYPE_A, 1, 0x9999, buf, sizeof(buf));
    ASSERT_TRUE(total > 0);

    char name[256];
    int nc = nfs_dns_name_decode(buf, (size_t)total, 12, name, sizeof(name));
    ASSERT_TRUE(nc > 0);
    ASSERT_TRUE(strcmp(name, "a.b.c.d.e.example.com") == 0);
}

/* ---- RD flag check ---- */

static void test_query_rd_flag_set(void) {
    uint8_t buf[512];
    int total = nfs_dns_build_query("test.com", NFS_DNS_TYPE_A, 1, 0x42, buf, sizeof(buf));
    ASSERT_TRUE(total > 0);

    struct nfs_dns_header hdr;
    nfs_dns_parse_header(buf, (size_t)total, &hdr);
    uint16_t flags = ntohs(hdr.flags);
    /* RD bit is bit 8 */
    ASSERT_TRUE((flags & 0x0100) != 0);
}

int main(void) {
    printf("DNS Message Format Tests (RFC 1035)\n");

    test_header_size();
    test_name_encode_www_example_com();
    test_name_encode_root();
    test_name_encode_single_label();
    test_name_encode_trailing_dot();
    test_name_decode_roundtrip();
    test_name_decode_single_label();
    test_name_decode_compression_pointer();
    test_build_query_type_a();
    test_build_query_type_aaaa();
    test_qr_flag_query();
    test_qr_flag_response();
    test_rcode_nxdomain();
    test_rcode_servfail();
    test_rcode_refused();
    test_parse_header_too_short();
    test_parse_header_exact_size();
    test_name_encode_empty();
    test_name_encode_buffer_too_small();
    test_type_constants();
    test_build_query_single_label();
    test_build_query_deep_subdomain();
    test_query_rd_flag_set();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
