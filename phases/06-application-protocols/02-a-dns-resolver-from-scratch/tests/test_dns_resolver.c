/* Unit tests for DNS resolver. */

#include "../dns_resolver.h"
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

/* ---- Name encoding tests ---- */

static void test_name_encode_simple(void) {
    uint8_t out[64];
    int n = nfs_dns_name_encode("www.google.com", out, sizeof(out));
    /* \x03www\x06google\x03com\x00 = 3+3+1+6+6+1+3+3+1+1 = 16 bytes */
    ASSERT_EQ(n, 16);
    ASSERT_EQ(out[0], 3);
    ASSERT_TRUE(memcmp(out + 1, "www", 3) == 0);
    ASSERT_EQ(out[4], 6);
    ASSERT_TRUE(memcmp(out + 5, "google", 6) == 0);
    ASSERT_EQ(out[11], 3);
    ASSERT_TRUE(memcmp(out + 12, "com", 3) == 0);
    ASSERT_EQ(out[15], 0);
}

static void test_name_encode_root(void) {
    uint8_t out[4];
    int n = nfs_dns_name_encode(".", out, sizeof(out));
    ASSERT_EQ(n, 1);
    ASSERT_EQ(out[0], 0);
}

static void test_name_encode_empty(void) {
    uint8_t out[4];
    int n = nfs_dns_name_encode("", out, sizeof(out));
    ASSERT_EQ(n, 1);
    ASSERT_EQ(out[0], 0);
}

static void test_name_encode_trailing_dot(void) {
    uint8_t out[64];
    int n1 = nfs_dns_name_encode("example.com.", out, sizeof(out));
    uint8_t out2[64];
    int n2 = nfs_dns_name_encode("example.com", out2, sizeof(out2));
    ASSERT_EQ(n1, n2);
    ASSERT_TRUE(memcmp(out, out2, (size_t)n1) == 0);
}

static void test_name_encode_single_label(void) {
    uint8_t out[32];
    int n = nfs_dns_name_encode("localhost", out, sizeof(out));
    ASSERT_EQ(n, 11); /* \x09localhost\x00 */
    ASSERT_EQ(out[0], 9);
    ASSERT_TRUE(memcmp(out + 1, "localhost", 9) == 0);
    ASSERT_EQ(out[10], 0);
}

static void test_name_encode_null_input(void) {
    uint8_t out[32];
    ASSERT_EQ(nfs_dns_name_encode(NULL, out, sizeof(out)), -1);
}

static void test_name_encode_buffer_too_small(void) {
    uint8_t out[4];
    ASSERT_EQ(nfs_dns_name_encode("www.example.com", out, sizeof(out)), -1);
}

/* ---- Name decoding tests ---- */

static void test_name_decode_simple(void) {
    /* \x03www\x06google\x03com\x00 */
    uint8_t wire[] = {3, 'w', 'w', 'w', 6, 'g', 'o', 'o', 'g', 'l', 'e', 3, 'c', 'o', 'm', 0};
    char out[256];
    int consumed = nfs_dns_name_decode(wire, sizeof(wire), 0, out, sizeof(out));
    ASSERT_EQ(consumed, 16);
    ASSERT_TRUE(strcmp(out, "www.google.com") == 0);
}

static void test_name_decode_root(void) {
    uint8_t wire[] = {0};
    char out[256];
    int consumed = nfs_dns_name_decode(wire, sizeof(wire), 0, out, sizeof(out));
    ASSERT_EQ(consumed, 1);
    ASSERT_TRUE(strcmp(out, "") == 0);
}

static void test_name_decode_compression(void) {
    /* Build: \x03www\x07example\x03com\x00 at offset 0 (17 bytes)
     * Then at offset 17: \x03api\xC0\x04 (pointer to "example.com") */
    uint8_t wire[32];
    memset(wire, 0, sizeof(wire));
    /* www.example.com at offset 0 */
    uint8_t name1[] = {3, 'w', 'w', 'w', 7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0};
    memcpy(wire, name1, sizeof(name1));
    /* api + pointer to offset 4 ("example.com") */
    wire[17] = 3;
    wire[18] = 'a';
    wire[19] = 'p';
    wire[20] = 'i';
    wire[21] = 0xC0;
    wire[22] = 0x04; /* pointer to offset 4 */

    char out[256];
    int consumed = nfs_dns_name_decode(wire, 23, 17, out, sizeof(out));
    ASSERT_EQ(consumed, 6); /* 1(len) + 3(api) + 2(pointer) = 6 consumed from offset 17 */
    ASSERT_TRUE(strcmp(out, "api.example.com") == 0);
}

static void test_name_decode_null_input(void) {
    char out[64];
    ASSERT_EQ(nfs_dns_name_decode(NULL, 10, 0, out, sizeof(out)), -1);
}

/* ---- Query build tests ---- */

static void test_query_build_basic(void) {
    uint8_t buf[512];
    int len = nfs_dns_query_build("www.example.com", NFS_DNS_TYPE_A, 0xABCD, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);

    /* Verify header */
    struct nfs_dns_header hdr;
    memcpy(&hdr, buf, 12);
    ASSERT_EQ(ntohs(hdr.id), 0xABCD);
    ASSERT_TRUE(nfs_dns_is_query(&hdr));
    ASSERT_EQ(ntohs(hdr.qdcount), 1);
    ASSERT_EQ(ntohs(hdr.ancount), 0);

    /* Verify RD flag is set */
    uint16_t flags = ntohs(hdr.flags);
    ASSERT_TRUE((flags & 0x0100) != 0); /* RD bit */
}

static void test_query_build_aaaa(void) {
    uint8_t buf[512];
    int len = nfs_dns_query_build("ipv6.google.com", NFS_DNS_TYPE_AAAA, 0x5678, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);

    /* Skip header (12) + name encoding, read QTYPE */
    uint8_t encoded[64];
    int nlen = nfs_dns_name_encode("ipv6.google.com", encoded, sizeof(encoded));
    size_t qtype_off = 12 + (size_t)nlen;
    uint16_t qtype;
    memcpy(&qtype, buf + qtype_off, 2);
    ASSERT_EQ(ntohs(qtype), NFS_DNS_TYPE_AAAA);
}

static void test_query_build_null(void) {
    uint8_t buf[512];
    ASSERT_EQ(nfs_dns_query_build(NULL, NFS_DNS_TYPE_A, 1, buf, sizeof(buf)), -1);
}

static void test_query_build_buffer_too_small(void) {
    uint8_t buf[8]; /* too small for header */
    ASSERT_EQ(nfs_dns_query_build("x.com", NFS_DNS_TYPE_A, 1, buf, sizeof(buf)), -1);
}

/* ---- Response parse tests ---- */

/* Helper: build a minimal DNS response in a buffer */
static size_t build_test_response(uint8_t *buf, size_t buf_sz, uint16_t id, uint8_t rcode,
                                  const char *qname, uint16_t qtype, const uint8_t *rdata,
                                  uint16_t rdlen, uint32_t ttl) {
    memset(buf, 0, buf_sz);
    struct nfs_dns_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.id = htons(id);
    /* QR=1, RD=1, RA=1, RCODE */
    hdr.flags = htons((uint16_t)(0x8180 | (rcode & 0x0F)));
    hdr.qdcount = htons(1);
    hdr.ancount = htons(rdata ? 1 : 0);
    memcpy(buf, &hdr, 12);

    size_t off = 12;

    /* Question section */
    int nlen = nfs_dns_name_encode(qname, buf + off, buf_sz - off);
    if (nlen < 0)
        return 0;
    off += (size_t)nlen;
    uint16_t tmp = htons(qtype);
    memcpy(buf + off, &tmp, 2);
    off += 2;
    tmp = htons(NFS_DNS_CLASS_IN);
    memcpy(buf + off, &tmp, 2);
    off += 2;

    if (!rdata)
        return off;

    /* Answer section */
    nlen = nfs_dns_name_encode(qname, buf + off, buf_sz - off);
    if (nlen < 0)
        return 0;
    off += (size_t)nlen;
    tmp = htons(qtype);
    memcpy(buf + off, &tmp, 2);
    off += 2;
    tmp = htons(NFS_DNS_CLASS_IN);
    memcpy(buf + off, &tmp, 2);
    off += 2;
    uint32_t t = htonl(ttl);
    memcpy(buf + off, &t, 4);
    off += 4;
    tmp = htons(rdlen);
    memcpy(buf + off, &tmp, 2);
    off += 2;
    memcpy(buf + off, rdata, rdlen);
    off += rdlen;

    return off;
}

static void test_response_parse_a_record(void) {
    uint8_t buf[256];
    uint8_t ip[] = {93, 184, 216, 34};
    size_t len = build_test_response(buf, sizeof(buf), 0x1234, 0, "www.example.com", NFS_DNS_TYPE_A,
                                     ip, 4, 300);
    ASSERT_TRUE(len > 0);

    struct nfs_dns_response resp;
    ASSERT_EQ(nfs_dns_response_parse(buf, len, &resp), 0);
    ASSERT_EQ(ntohs(resp.header.id), 0x1234);
    ASSERT_TRUE(strcmp(resp.qname, "www.example.com") == 0);
    ASSERT_EQ(resp.qtype, NFS_DNS_TYPE_A);
    ASSERT_EQ(resp.ancount, 1);
    ASSERT_EQ(resp.answers[0].type, NFS_DNS_TYPE_A);
    ASSERT_EQ(resp.answers[0].ttl, 300);
    ASSERT_EQ(resp.answers[0].rdlength, 4);
    ASSERT_TRUE(memcmp(resp.answers[0].rdata, ip, 4) == 0);
}

static void test_response_parse_nxdomain(void) {
    uint8_t buf[256];
    size_t len = build_test_response(buf, sizeof(buf), 0x5555, NFS_DNS_RCODE_NXDOMAIN,
                                     "nonexistent.example.com", NFS_DNS_TYPE_A, NULL, 0, 0);
    ASSERT_TRUE(len > 0);

    struct nfs_dns_response resp;
    ASSERT_EQ(nfs_dns_response_parse(buf, len, &resp), 0);
    ASSERT_EQ(nfs_dns_rcode(&resp.header), NFS_DNS_RCODE_NXDOMAIN);
    ASSERT_EQ(resp.ancount, 0);
}

static void test_response_parse_too_short(void) {
    uint8_t buf[4] = {0};
    struct nfs_dns_response resp;
    ASSERT_EQ(nfs_dns_response_parse(buf, sizeof(buf), &resp), -1);
}

static void test_response_parse_null(void) {
    struct nfs_dns_response resp;
    ASSERT_EQ(nfs_dns_response_parse(NULL, 64, &resp), -1);
}

/* ---- Flag helper tests ---- */

static void test_rcode_str(void) {
    ASSERT_TRUE(strcmp(nfs_dns_rcode_str(NFS_DNS_RCODE_NOERROR), "NOERROR") == 0);
    ASSERT_TRUE(strcmp(nfs_dns_rcode_str(NFS_DNS_RCODE_NXDOMAIN), "NXDOMAIN") == 0);
    ASSERT_TRUE(strcmp(nfs_dns_rcode_str(NFS_DNS_RCODE_SERVFAIL), "SERVFAIL") == 0);
    ASSERT_TRUE(strcmp(nfs_dns_rcode_str(99), "UNKNOWN") == 0);
}

static void test_type_str(void) {
    ASSERT_TRUE(strcmp(nfs_dns_type_str(NFS_DNS_TYPE_A), "A") == 0);
    ASSERT_TRUE(strcmp(nfs_dns_type_str(NFS_DNS_TYPE_AAAA), "AAAA") == 0);
    ASSERT_TRUE(strcmp(nfs_dns_type_str(NFS_DNS_TYPE_CNAME), "CNAME") == 0);
    ASSERT_TRUE(strcmp(nfs_dns_type_str(NFS_DNS_TYPE_MX), "MX") == 0);
    ASSERT_TRUE(strcmp(nfs_dns_type_str(9999), "UNKNOWN") == 0);
}

/* ---- Roundtrip test ---- */

static void test_query_response_roundtrip(void) {
    /* Build a query, then fake a matching response and parse it */
    uint8_t qbuf[512];
    int qlen = nfs_dns_query_build("test.example.org", NFS_DNS_TYPE_A, 0x9999, qbuf, sizeof(qbuf));
    ASSERT_TRUE(qlen > 0);

    /* Verify query header fields */
    struct nfs_dns_header qhdr;
    memcpy(&qhdr, qbuf, 12);
    ASSERT_EQ(ntohs(qhdr.id), 0x9999);
    ASSERT_TRUE(nfs_dns_is_query(&qhdr));

    /* Build matching response */
    uint8_t rbuf[256];
    uint8_t ip[] = {10, 0, 0, 1};
    size_t rlen = build_test_response(rbuf, sizeof(rbuf), 0x9999, 0, "test.example.org",
                                      NFS_DNS_TYPE_A, ip, 4, 600);
    ASSERT_TRUE(rlen > 0);

    struct nfs_dns_response resp;
    ASSERT_EQ(nfs_dns_response_parse(rbuf, rlen, &resp), 0);
    ASSERT_EQ(ntohs(resp.header.id), 0x9999);
    ASSERT_EQ(resp.answers[0].rdata[0], 10);
    ASSERT_EQ(resp.answers[0].rdata[3], 1);
}

int main(void) {
    printf("DNS Resolver Tests\n");

    test_name_encode_simple();
    test_name_encode_root();
    test_name_encode_empty();
    test_name_encode_trailing_dot();
    test_name_encode_single_label();
    test_name_encode_null_input();
    test_name_encode_buffer_too_small();
    test_name_decode_simple();
    test_name_decode_root();
    test_name_decode_compression();
    test_name_decode_null_input();
    test_query_build_basic();
    test_query_build_aaaa();
    test_query_build_null();
    test_query_build_buffer_too_small();
    test_response_parse_a_record();
    test_response_parse_nxdomain();
    test_response_parse_too_short();
    test_response_parse_null();
    test_rcode_str();
    test_type_str();
    test_query_response_roundtrip();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
