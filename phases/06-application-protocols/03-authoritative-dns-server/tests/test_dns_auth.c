/* Unit tests for authoritative DNS server. */

#include "../dns_auth.h"
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

/* Helper: build a DNS query */
static int build_query(uint8_t *buf, size_t buf_sz, uint16_t id, const char *name, uint16_t qtype) {
    memset(buf, 0, buf_sz);
    struct nfs_dns_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.id = htons(id);
    hdr.flags = htons(0x0100); /* RD=1 */
    hdr.qdcount = htons(1);
    memcpy(buf, &hdr, 12);

    size_t off = 12;
    int nlen = nfs_dns_name_encode(name, buf + off, buf_sz - off);
    if (nlen < 0)
        return -1;
    off += (size_t)nlen;

    uint16_t tmp = htons(qtype);
    memcpy(buf + off, &tmp, 2);
    off += 2;
    tmp = htons(NFS_DNS_CLASS_IN);
    memcpy(buf + off, &tmp, 2);
    off += 2;

    return (int)off;
}

/* ---- Zone init tests ---- */

static void test_zone_init(void) {
    struct nfs_dns_zone zone;
    ASSERT_EQ(nfs_dns_zone_init(&zone, "example.com"), 0);
    ASSERT_TRUE(strcmp(zone.origin, "example.com") == 0);
    ASSERT_EQ(zone.rr_count, 0);
}

static void test_zone_init_null(void) {
    ASSERT_EQ(nfs_dns_zone_init(NULL, "test.com"), -1);
    struct nfs_dns_zone zone;
    ASSERT_EQ(nfs_dns_zone_init(&zone, NULL), -1);
}

/* ---- Add RR tests ---- */

static void test_add_a_record(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    ASSERT_EQ(nfs_dns_zone_add_a(&zone, "www.example.com", 300, 10, 0, 0, 1), 0);
    ASSERT_EQ(zone.rr_count, 1);
    ASSERT_EQ(zone.rrs[0].type, NFS_DNS_TYPE_A);
    ASSERT_EQ(zone.rrs[0].ttl, 300);
    ASSERT_EQ(zone.rrs[0].rdlength, 4);
    ASSERT_EQ(zone.rrs[0].rdata[0], 10);
    ASSERT_EQ(zone.rrs[0].rdata[3], 1);
}

static void test_add_ns_record(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    ASSERT_EQ(nfs_dns_zone_add_ns(&zone, "example.com", 86400, "ns1.example.com"), 0);
    ASSERT_EQ(zone.rr_count, 1);
    ASSERT_EQ(zone.rrs[0].type, NFS_DNS_TYPE_NS);
    ASSERT_EQ(zone.rrs[0].ttl, 86400);
    ASSERT_TRUE(zone.rrs[0].rdlength > 0);
}

static void test_add_soa_record(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");

    struct nfs_dns_soa soa = {.mname = "ns1.example.com",
                              .rname = "admin.example.com",
                              .serial = 2024010101,
                              .refresh = 3600,
                              .retry = 900,
                              .expire = 604800,
                              .minimum = 86400};
    ASSERT_EQ(nfs_dns_zone_add_soa(&zone, "example.com", 86400, &soa), 0);
    ASSERT_EQ(zone.rr_count, 1);
    ASSERT_EQ(zone.rrs[0].type, NFS_DNS_TYPE_SOA);
}

static void test_add_multiple_records(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    nfs_dns_zone_add_a(&zone, "example.com", 3600, 93, 184, 216, 34);
    nfs_dns_zone_add_a(&zone, "www.example.com", 300, 93, 184, 216, 34);
    nfs_dns_zone_add_ns(&zone, "example.com", 86400, "ns1.example.com");
    nfs_dns_zone_add_ns(&zone, "example.com", 86400, "ns2.example.com");
    ASSERT_EQ(zone.rr_count, 4);
}

static void test_add_rr_null_zone(void) {
    ASSERT_EQ(nfs_dns_zone_add_a(NULL, "x.com", 100, 1, 2, 3, 4), -1);
}

/* ---- Lookup tests ---- */

static void test_lookup_a_record(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    nfs_dns_zone_add_a(&zone, "www.example.com", 300, 10, 0, 0, 1);

    struct nfs_dns_lookup_result result;
    int count = nfs_dns_zone_lookup(&zone, "www.example.com", NFS_DNS_TYPE_A, &result);
    ASSERT_EQ(count, 1);
    ASSERT_EQ(result.matches[0]->type, NFS_DNS_TYPE_A);
    ASSERT_EQ(result.matches[0]->rdata[0], 10);
}

static void test_lookup_no_match(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    nfs_dns_zone_add_a(&zone, "www.example.com", 300, 10, 0, 0, 1);

    struct nfs_dns_lookup_result result;
    int count = nfs_dns_zone_lookup(&zone, "mail.example.com", NFS_DNS_TYPE_A, &result);
    ASSERT_EQ(count, 0);
}

static void test_lookup_wrong_type(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    nfs_dns_zone_add_a(&zone, "www.example.com", 300, 10, 0, 0, 1);

    struct nfs_dns_lookup_result result;
    int count = nfs_dns_zone_lookup(&zone, "www.example.com", NFS_DNS_TYPE_AAAA, &result);
    ASSERT_EQ(count, 0);
}

static void test_lookup_any_type(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    nfs_dns_zone_add_a(&zone, "example.com", 3600, 93, 184, 216, 34);
    nfs_dns_zone_add_ns(&zone, "example.com", 86400, "ns1.example.com");

    struct nfs_dns_lookup_result result;
    int count = nfs_dns_zone_lookup(&zone, "example.com", 255, &result);
    ASSERT_EQ(count, 2);
}

static void test_lookup_case_insensitive(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    nfs_dns_zone_add_a(&zone, "WWW.EXAMPLE.COM", 300, 10, 0, 0, 1);

    struct nfs_dns_lookup_result result;
    int count = nfs_dns_zone_lookup(&zone, "www.example.com", NFS_DNS_TYPE_A, &result);
    ASSERT_EQ(count, 1);
}

static void test_lookup_multiple_ns(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    nfs_dns_zone_add_ns(&zone, "example.com", 86400, "ns1.example.com");
    nfs_dns_zone_add_ns(&zone, "example.com", 86400, "ns2.example.com");

    struct nfs_dns_lookup_result result;
    int count = nfs_dns_zone_lookup(&zone, "example.com", NFS_DNS_TYPE_NS, &result);
    ASSERT_EQ(count, 2);
}

static void test_lookup_null(void) {
    struct nfs_dns_lookup_result result;
    ASSERT_EQ(nfs_dns_zone_lookup(NULL, "x.com", 1, &result), -1);
}

/* ---- Response building tests ---- */

static void test_build_response_a(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    nfs_dns_zone_add_a(&zone, "www.example.com", 300, 93, 184, 216, 34);
    nfs_dns_zone_add_ns(&zone, "example.com", 86400, "ns1.example.com");

    uint8_t qbuf[512];
    int qlen = build_query(qbuf, sizeof(qbuf), 0xAAAA, "www.example.com", NFS_DNS_TYPE_A);
    ASSERT_TRUE(qlen > 0);

    uint8_t rbuf[512];
    int rlen = nfs_dns_zone_build_response(&zone, qbuf, (size_t)qlen, rbuf, sizeof(rbuf));
    ASSERT_TRUE(rlen > 0);

    /* Verify response header */
    struct nfs_dns_hdr rhdr;
    memcpy(&rhdr, rbuf, 12);
    ASSERT_EQ(ntohs(rhdr.id), 0xAAAA);
    /* QR=1 */
    uint16_t flags = ntohs(rhdr.flags);
    ASSERT_TRUE((flags & 0x8000) != 0);
    /* AA=1 */
    ASSERT_TRUE((flags & 0x0400) != 0);
    /* RCODE=0 */
    ASSERT_EQ(flags & 0x000F, 0);
    ASSERT_EQ(ntohs(rhdr.ancount), 1);
    ASSERT_TRUE(ntohs(rhdr.nscount) >= 1);
}

static void test_build_response_nxdomain(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    nfs_dns_zone_add_a(&zone, "www.example.com", 300, 10, 0, 0, 1);

    uint8_t qbuf[512];
    int qlen = build_query(qbuf, sizeof(qbuf), 0xBBBB, "nonexistent.example.com", NFS_DNS_TYPE_A);
    ASSERT_TRUE(qlen > 0);

    uint8_t rbuf[512];
    int rlen = nfs_dns_zone_build_response(&zone, qbuf, (size_t)qlen, rbuf, sizeof(rbuf));
    ASSERT_TRUE(rlen > 0);

    struct nfs_dns_hdr rhdr;
    memcpy(&rhdr, rbuf, 12);
    uint16_t flags = ntohs(rhdr.flags);
    ASSERT_EQ(flags & 0x000F, NFS_DNS_RCODE_NXDOMAIN);
    ASSERT_EQ(ntohs(rhdr.ancount), 0);
}

static void test_build_response_null(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    uint8_t rbuf[512];
    ASSERT_EQ(nfs_dns_zone_build_response(&zone, NULL, 0, rbuf, sizeof(rbuf)), -1);
}

static void test_build_response_preserves_id(void) {
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");
    nfs_dns_zone_add_a(&zone, "example.com", 3600, 1, 2, 3, 4);

    uint8_t qbuf[512];
    int qlen = build_query(qbuf, sizeof(qbuf), 0x1234, "example.com", NFS_DNS_TYPE_A);
    ASSERT_TRUE(qlen > 0);

    uint8_t rbuf[512];
    int rlen = nfs_dns_zone_build_response(&zone, qbuf, (size_t)qlen, rbuf, sizeof(rbuf));
    ASSERT_TRUE(rlen > 0);

    struct nfs_dns_hdr rhdr;
    memcpy(&rhdr, rbuf, 12);
    ASSERT_EQ(ntohs(rhdr.id), 0x1234);
}

int main(void) {
    printf("Authoritative DNS Server Tests\n");

    test_zone_init();
    test_zone_init_null();
    test_add_a_record();
    test_add_ns_record();
    test_add_soa_record();
    test_add_multiple_records();
    test_add_rr_null_zone();
    test_lookup_a_record();
    test_lookup_no_match();
    test_lookup_wrong_type();
    test_lookup_any_type();
    test_lookup_case_insensitive();
    test_lookup_multiple_ns();
    test_lookup_null();
    test_build_response_a();
    test_build_response_nxdomain();
    test_build_response_null();
    test_build_response_preserves_id();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
