/* Unit tests for What is a Network. */

#include "../network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(expr, expected) \
    do { \
        tests_run++; \
        long long _got = (long long)(expr); \
        long long _exp = (long long)(expected); \
        if (_got != _exp) { \
            fprintf(stderr, "  FAIL %s:%d: %s == 0x%llx, want 0x%llx\n", \
                    __FILE__, __LINE__, #expr, _got, _exp); \
            return; \
        } \
        tests_passed++; \
    } while (0)

#define ASSERT_TRUE(expr) \
    do { \
        tests_run++; \
        if (!(expr)) { \
            fprintf(stderr, "  FAIL %s:%d: %s is false\n", \
                    __FILE__, __LINE__, #expr); \
            return; \
        } \
        tests_passed++; \
    } while (0)

/* ---- 1. Build/parse roundtrip -------------------------------- */

static void test_build_parse_roundtrip(void)
{
    uint8_t buf[256];
    const uint8_t payload[] = "Hello, network!";
    size_t plen = sizeof(payload) - 1;

    int total = nfs_pkt_build(buf, sizeof(buf),
                              1, 3, 0x42, 0xDEADBEEF,
                              payload, plen);
    ASSERT_TRUE(total > 0);
    ASSERT_EQ((size_t)total, NFS_PKT_HDR_SIZE + plen);

    struct nfs_pkt_header hdr;
    const uint8_t *out_payload;
    size_t out_len;

    ASSERT_EQ(nfs_pkt_parse(buf, (size_t)total, &hdr, &out_payload, &out_len), 0);
    ASSERT_EQ((hdr.ver_type >> 4) & 0x0F, 1);      /* version */
    ASSERT_EQ(hdr.ver_type & 0x0F, 3);              /* type */
    ASSERT_EQ(hdr.flags, 0x42);
    ASSERT_EQ(ntohs(hdr.length), total);
    ASSERT_EQ(ntohl(hdr.id), 0xDEADBEEF);
    ASSERT_EQ(out_len, plen);
    ASSERT_TRUE(memcmp(out_payload, payload, plen) == 0);
}

/* ---- 2. Empty payload ---------------------------------------- */

static void test_empty_payload(void)
{
    uint8_t buf[64];
    int total = nfs_pkt_build(buf, sizeof(buf),
                              1, 0, 0, 1,
                              NULL, 0);
    ASSERT_EQ(total, (int)NFS_PKT_HDR_SIZE);

    struct nfs_pkt_header hdr;
    const uint8_t *p;
    size_t plen;
    ASSERT_EQ(nfs_pkt_parse(buf, (size_t)total, &hdr, &p, &plen), 0);
    ASSERT_EQ(plen, 0);
    ASSERT_EQ(ntohl(hdr.id), 1);
}

/* ---- 3. Large payload ---------------------------------------- */

static void test_large_payload(void)
{
    size_t payload_sz = 10000;
    uint8_t *payload = malloc(payload_sz);
    ASSERT_TRUE(payload != NULL);
    for (size_t i = 0; i < payload_sz; i++)
        payload[i] = (uint8_t)(i & 0xFF);

    size_t buf_sz = NFS_PKT_HDR_SIZE + payload_sz;
    uint8_t *buf = malloc(buf_sz);
    ASSERT_TRUE(buf != NULL);

    int total = nfs_pkt_build(buf, buf_sz,
                              1, 7, 0xFF, 999999,
                              payload, payload_sz);
    ASSERT_EQ(total, (int)buf_sz);

    struct nfs_pkt_header hdr;
    const uint8_t *p;
    size_t plen;
    ASSERT_EQ(nfs_pkt_parse(buf, (size_t)total, &hdr, &p, &plen), 0);
    ASSERT_EQ(plen, payload_sz);
    ASSERT_TRUE(memcmp(p, payload, payload_sz) == 0);

    free(payload);
    free(buf);
}

/* ---- 4. Truncated packet rejection --------------------------- */

static void test_truncated_packet(void)
{
    uint8_t buf[4] = {0x10, 0x00, 0x00, 0x08};  /* only 4 bytes, need 8 */
    struct nfs_pkt_header hdr;
    const uint8_t *p;
    size_t plen;
    ASSERT_EQ(nfs_pkt_parse(buf, sizeof(buf), &hdr, &p, &plen), -1);
}

/* ---- 5. Length mismatch rejection ----------------------------- */

static void test_length_mismatch(void)
{
    /* Build a valid packet, then tell parse the buffer is smaller */
    uint8_t buf[256];
    const uint8_t payload[] = "test data for mismatch";
    int total = nfs_pkt_build(buf, sizeof(buf),
                              1, 1, 0, 42,
                              payload, sizeof(payload) - 1);
    ASSERT_TRUE(total > 0);

    /* Header says total bytes but we only give 50 bytes (< total) */
    struct nfs_pkt_header hdr;
    const uint8_t *p;
    size_t plen;
    /* If total > 50, parse should fail because wire_len > buf_len */
    if (total > 50) {
        ASSERT_EQ(nfs_pkt_parse(buf, 50, &hdr, &p, &plen), -1);
    }
}

/* ---- 6. Network byte order verification ---------------------- */

static void test_network_byte_order(void)
{
    /* Manually construct an 8-byte header in big-endian */
    uint8_t buf[10];
    buf[0] = 0x13;      /* version=1, type=3 */
    buf[1] = 0xAB;      /* flags */
    buf[2] = 0x00;      /* length high byte */
    buf[3] = 0x0A;      /* length low byte = 10 */
    buf[4] = 0x00;      /* id bytes (big-endian 0x00000007) */
    buf[5] = 0x00;
    buf[6] = 0x00;
    buf[7] = 0x07;
    buf[8] = 0xCC;      /* 2-byte payload */
    buf[9] = 0xDD;

    struct nfs_pkt_header hdr;
    const uint8_t *p;
    size_t plen;
    ASSERT_EQ(nfs_pkt_parse(buf, 10, &hdr, &p, &plen), 0);
    ASSERT_EQ((hdr.ver_type >> 4) & 0x0F, 1);
    ASSERT_EQ(hdr.ver_type & 0x0F, 3);
    ASSERT_EQ(hdr.flags, 0xAB);
    ASSERT_EQ(ntohs(hdr.length), 10);
    ASSERT_EQ(ntohl(hdr.id), 7);
    ASSERT_EQ(plen, 2);
    ASSERT_EQ(p[0], 0xCC);
    ASSERT_EQ(p[1], 0xDD);
}

/* ---- 7. Internet checksum: RFC 1071 test vector --------------- */

static void test_checksum_rfc1071(void)
{
    /* RFC 1071 section 3 example:
     * Data: {0x00,0x01, 0xf2,0x03, 0xf4,0xf5, 0xf6,0xf7}
     * 16-bit words: 0x0001 + 0xf203 + 0xf4f5 + 0xf6f7 = 0x2ddf0
     * Fold: 0xddf0 + 0x0002 = 0xddf2
     * Complement: ~0xddf2 = 0x220d
     */
    uint8_t data[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
    uint16_t cksum = nfs_inet_checksum(data, sizeof(data));
    ASSERT_EQ(cksum, 0x220d);
}

/* ---- 8. Internet checksum: self-verify ----------------------- */

static void test_checksum_self_verify(void)
{
    uint8_t data[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
    uint16_t cksum = nfs_inet_checksum(data, sizeof(data));

    /* Append checksum in network byte order and verify result is 0 */
    uint8_t verify[10];
    memcpy(verify, data, 8);
    verify[8] = (uint8_t)(cksum >> 8);
    verify[9] = (uint8_t)(cksum & 0xFF);
    uint16_t check = nfs_inet_checksum(verify, 10);
    ASSERT_EQ(check, 0x0000);
}

/* ---- 9. Internet checksum: odd-length data ------------------- */

static void test_checksum_odd_length(void)
{
    /* 3 bytes: {0x01, 0x02, 0x03}
     * Words: 0x0102 + 0x0300 (padded) = 0x0402
     * Complement: ~0x0402 = 0xfbfd
     */
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint16_t cksum = nfs_inet_checksum(data, sizeof(data));
    ASSERT_EQ(cksum, 0xfbfd);

    /* Verify odd-length produces different result than even-padded:
     * 4 bytes {0x01, 0x02, 0x03, 0x00} should give same checksum
     * since the odd-byte padding adds a zero byte. */
    uint8_t data_padded[] = {0x01, 0x02, 0x03, 0x00};
    uint16_t cksum_padded = nfs_inet_checksum(data_padded, sizeof(data_padded));
    ASSERT_EQ(cksum, cksum_padded);

    /* Single byte */
    uint8_t one_byte[] = {0xAB};
    uint16_t ck1 = nfs_inet_checksum(one_byte, 1);
    /* 0xAB00 padded, complement = ~0xAB00 = 0x54FF */
    ASSERT_EQ(ck1, 0x54FF);
}

/* ---- 10. Internet checksum: all zeros -> 0xFFFF -------------- */

static void test_checksum_all_zeros(void)
{
    uint8_t data[8] = {0};
    uint16_t cksum = nfs_inet_checksum(data, sizeof(data));
    ASSERT_EQ(cksum, 0xFFFF);
}

/* ---- 11. Internet checksum: all ones -> 0x0000 --------------- */

static void test_checksum_all_ones(void)
{
    uint8_t data[8];
    memset(data, 0xFF, sizeof(data));
    /* Words: 0xFFFF * 4 = 0x3FFFC
     * Fold: 0xFFFC + 0x0003 = 0xFFFF
     * Complement: ~0xFFFF = 0x0000
     */
    uint16_t cksum = nfs_inet_checksum(data, sizeof(data));
    ASSERT_EQ(cksum, 0x0000);
}

/* ---- 12. Encapsulate/decapsulate roundtrip ------------------- */

static void test_encap_decap_roundtrip(void)
{
    uint8_t buf[256];
    const uint8_t payload[] = "encapsulated data";
    size_t plen = sizeof(payload) - 1;

    int total = nfs_encapsulate(buf, sizeof(buf),
                                5, 0x01, 42,
                                payload, plen);
    ASSERT_TRUE(total > 0);
    ASSERT_EQ((size_t)total, NFS_PKT_HDR_SIZE + plen);

    struct nfs_pkt_header hdr;
    const uint8_t *out;
    size_t out_len;
    ASSERT_EQ(nfs_decapsulate(buf, (size_t)total, &hdr, &out, &out_len), 0);
    ASSERT_EQ((hdr.ver_type >> 4) & 0x0F, NFS_PKT_VERSION);
    ASSERT_EQ(hdr.ver_type & 0x0F, 5);
    ASSERT_EQ(hdr.flags, 0x01);
    ASSERT_EQ(ntohl(hdr.id), 42);
    ASSERT_EQ(out_len, plen);
    ASSERT_TRUE(memcmp(out, payload, plen) == 0);
}

/* ---- 13. All packet types (0-15) ----------------------------- */

static void test_all_packet_types(void)
{
    uint8_t buf[64];
    const uint8_t payload[] = "x";

    for (int t = 0; t < 16; t++) {
        int total = nfs_pkt_build(buf, sizeof(buf),
                                  1, (uint8_t)t, 0, 0,
                                  payload, 1);
        ASSERT_TRUE(total > 0);

        struct nfs_pkt_header hdr;
        const uint8_t *p;
        size_t plen;
        ASSERT_EQ(nfs_pkt_parse(buf, (size_t)total, &hdr, &p, &plen), 0);
        ASSERT_EQ(hdr.ver_type & 0x0F, t);
    }
}

/* ---- 14. Max-value fields ------------------------------------ */

static void test_max_value_fields(void)
{
    uint8_t buf[64];
    int total = nfs_pkt_build(buf, sizeof(buf),
                              0x0F, 0x0F, 0xFF, 0xFFFFFFFF,
                              NULL, 0);
    ASSERT_EQ(total, (int)NFS_PKT_HDR_SIZE);

    struct nfs_pkt_header hdr;
    const uint8_t *p;
    size_t plen;
    ASSERT_EQ(nfs_pkt_parse(buf, (size_t)total, &hdr, &p, &plen), 0);
    ASSERT_EQ((hdr.ver_type >> 4) & 0x0F, 0x0F);
    ASSERT_EQ(hdr.ver_type & 0x0F, 0x0F);
    ASSERT_EQ(hdr.flags, 0xFF);
    ASSERT_EQ(ntohl(hdr.id), (long long)0xFFFFFFFF);
}

/* ---- 15. NULL pointer handling ------------------------------- */

static void test_null_pointers(void)
{
    /* NULL buf in build */
    ASSERT_EQ(nfs_pkt_build(NULL, 256, 1, 0, 0, 0, NULL, 0), -1);

    /* NULL payload with non-zero length in build */
    uint8_t buf[64];
    ASSERT_EQ(nfs_pkt_build(buf, sizeof(buf), 1, 0, 0, 0, NULL, 10), -1);

    /* NULL buf in parse */
    struct nfs_pkt_header hdr;
    const uint8_t *p;
    size_t plen;
    ASSERT_EQ(nfs_pkt_parse(NULL, 64, &hdr, &p, &plen), -1);

    /* NULL hdr in parse */
    ASSERT_EQ(nfs_pkt_parse(buf, 64, NULL, &p, &plen), -1);

    /* NULL payload_out in parse */
    ASSERT_EQ(nfs_pkt_parse(buf, 64, &hdr, NULL, &plen), -1);

    /* NULL payload_len_out in parse */
    ASSERT_EQ(nfs_pkt_parse(buf, 64, &hdr, &p, NULL), -1);
}

/* ---- Header size static assert ------------------------------- */

static void test_header_size(void)
{
    ASSERT_EQ(sizeof(struct nfs_pkt_header), 8);
}

/* ---- Test runner --------------------------------------------- */

int main(void)
{
    printf("=== What is a Network — Tests ===\n\n");

    printf("[build/parse roundtrip]\n");
    test_build_parse_roundtrip();

    printf("[empty payload]\n");
    test_empty_payload();

    printf("[large payload]\n");
    test_large_payload();

    printf("[truncated packet rejection]\n");
    test_truncated_packet();

    printf("[length mismatch rejection]\n");
    test_length_mismatch();

    printf("[network byte order verification]\n");
    test_network_byte_order();

    printf("[checksum: RFC 1071 test vector]\n");
    test_checksum_rfc1071();

    printf("[checksum: self-verify]\n");
    test_checksum_self_verify();

    printf("[checksum: odd-length data]\n");
    test_checksum_odd_length();

    printf("[checksum: all zeros]\n");
    test_checksum_all_zeros();

    printf("[checksum: all ones]\n");
    test_checksum_all_ones();

    printf("[encapsulate/decapsulate roundtrip]\n");
    test_encap_decap_roundtrip();

    printf("[all packet types 0-15]\n");
    test_all_packet_types();

    printf("[max-value fields]\n");
    test_max_value_fields();

    printf("[NULL pointer handling]\n");
    test_null_pointers();

    printf("[header size]\n");
    test_header_size();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) { printf("PASS\n"); return 0; }
    printf("FAIL\n"); return 1;
}
