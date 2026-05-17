/* Unit tests for SLAAC. */

#include "../slaac.h"
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

/* ---- EUI-64 tests ---- */

static void test_eui64_basic(void) {
    /* MAC: 00:1A:2B:3C:4D:5E
     * EUI-64: 02:1A:2B:FF:FE:3C:4D:5E (U/L bit flipped) */
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t eui64[8];
    ASSERT_EQ(nfs_slaac_eui64(mac, eui64), 0);
    ASSERT_EQ(eui64[0], 0x02); /* 0x00 ^ 0x02 */
    ASSERT_EQ(eui64[1], 0x1A);
    ASSERT_EQ(eui64[2], 0x2B);
    ASSERT_EQ(eui64[3], 0xFF);
    ASSERT_EQ(eui64[4], 0xFE);
    ASSERT_EQ(eui64[5], 0x3C);
    ASSERT_EQ(eui64[6], 0x4D);
    ASSERT_EQ(eui64[7], 0x5E);
}

static void test_eui64_ul_bit_set(void) {
    /* MAC with U/L bit already set: 02:... -> 00:... after flip */
    uint8_t mac[] = {0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    uint8_t eui64[8];
    ASSERT_EQ(nfs_slaac_eui64(mac, eui64), 0);
    ASSERT_EQ(eui64[0], 0x00); /* 0x02 ^ 0x02 = 0x00 */
}

static void test_eui64_all_ff(void) {
    uint8_t mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t eui64[8];
    ASSERT_EQ(nfs_slaac_eui64(mac, eui64), 0);
    ASSERT_EQ(eui64[0], 0xFD); /* 0xFF ^ 0x02 */
    ASSERT_EQ(eui64[3], 0xFF);
    ASSERT_EQ(eui64[4], 0xFE);
}

static void test_eui64_null(void) {
    uint8_t eui64[8];
    ASSERT_EQ(nfs_slaac_eui64(NULL, eui64), -1);
    uint8_t mac[6] = {0};
    ASSERT_EQ(nfs_slaac_eui64(mac, NULL), -1);
}

/* ---- Generate address tests ---- */

static void test_generate_addr_basic(void) {
    uint8_t prefix[16] = {0x20, 0x01, 0x0D, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t addr[16];

    ASSERT_EQ(nfs_slaac_generate_addr(prefix, 64, mac, addr), 0);

    /* First 8 bytes should be prefix */
    ASSERT_EQ(addr[0], 0x20);
    ASSERT_EQ(addr[1], 0x01);
    ASSERT_EQ(addr[2], 0x0D);
    ASSERT_EQ(addr[3], 0xB8);
    ASSERT_EQ(addr[4], 0x00);
    ASSERT_EQ(addr[5], 0x00);
    ASSERT_EQ(addr[6], 0x00);
    ASSERT_EQ(addr[7], 0x00);

    /* Last 8 bytes should be EUI-64 */
    ASSERT_EQ(addr[8], 0x02);
    ASSERT_EQ(addr[9], 0x1A);
    ASSERT_EQ(addr[10], 0x2B);
    ASSERT_EQ(addr[11], 0xFF);
    ASSERT_EQ(addr[12], 0xFE);
    ASSERT_EQ(addr[13], 0x3C);
    ASSERT_EQ(addr[14], 0x4D);
    ASSERT_EQ(addr[15], 0x5E);
}

static void test_generate_addr_nonzero_iid(void) {
    /* Prefix with non-zero bytes in IID area should be overwritten */
    uint8_t prefix[16] = {0x20, 0x01, 0x0D, 0xB8, 0,    0,    0,    0,
                          0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t addr[16];

    ASSERT_EQ(nfs_slaac_generate_addr(prefix, 64, mac, addr), 0);
    /* IID should be EUI-64, not the original values */
    ASSERT_EQ(addr[8], 0x02);
    ASSERT_EQ(addr[11], 0xFF);
    ASSERT_EQ(addr[12], 0xFE);
}

static void test_generate_addr_null(void) {
    uint8_t prefix[16] = {0};
    uint8_t mac[6] = {0};
    uint8_t addr[16];
    ASSERT_EQ(nfs_slaac_generate_addr(NULL, 64, mac, addr), -1);
    ASSERT_EQ(nfs_slaac_generate_addr(prefix, 64, NULL, addr), -1);
    ASSERT_EQ(nfs_slaac_generate_addr(prefix, 64, mac, NULL), -1);
}

static void test_generate_addr_bad_prefix_len(void) {
    uint8_t prefix[16] = {0};
    uint8_t mac[6] = {0};
    uint8_t addr[16];
    ASSERT_EQ(nfs_slaac_generate_addr(prefix, 129, mac, addr), -1);
}

/* ---- Validate prefix tests ---- */

static void test_validate_prefix_good(void) {
    uint8_t prefix[16] = {0x20, 0x01, 0x0D, 0xB8};
    ASSERT_EQ(nfs_slaac_validate_prefix(prefix, 64, 1), 1);
}

static void test_validate_prefix_not_64(void) {
    uint8_t prefix[16] = {0x20, 0x01, 0x0D, 0xB8};
    ASSERT_EQ(nfs_slaac_validate_prefix(prefix, 48, 1), 0);
    ASSERT_EQ(nfs_slaac_validate_prefix(prefix, 128, 1), 0);
}

static void test_validate_prefix_multicast(void) {
    uint8_t prefix[16] = {0xFF, 0x02};
    ASSERT_EQ(nfs_slaac_validate_prefix(prefix, 64, 1), 0);
}

static void test_validate_prefix_linklocal(void) {
    uint8_t prefix[16] = {0xFE, 0x80};
    ASSERT_EQ(nfs_slaac_validate_prefix(prefix, 64, 1), 0); /* reject */
    ASSERT_EQ(nfs_slaac_validate_prefix(prefix, 64, 0), 1); /* allow */
}

static void test_validate_prefix_all_zeros(void) {
    uint8_t prefix[16] = {0};
    ASSERT_EQ(nfs_slaac_validate_prefix(prefix, 64, 1), 0);
}

static void test_validate_prefix_null(void) {
    ASSERT_EQ(nfs_slaac_validate_prefix(NULL, 64, 1), 0);
}

/* ---- Link-local tests ---- */

static void test_linklocal(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t addr[16];
    ASSERT_EQ(nfs_slaac_linklocal(mac, addr), 0);
    ASSERT_EQ(addr[0], 0xFE);
    ASSERT_EQ(addr[1], 0x80);
    /* bytes 2-7 should be zero */
    for (int i = 2; i < 8; i++)
        ASSERT_EQ(addr[i], 0);
    /* bytes 8-15 should be EUI-64 */
    ASSERT_EQ(addr[8], 0x02);
    ASSERT_EQ(addr[11], 0xFF);
    ASSERT_EQ(addr[12], 0xFE);
}

static void test_linklocal_null(void) {
    uint8_t addr[16];
    ASSERT_EQ(nfs_slaac_linklocal(NULL, addr), -1);
}

/* ---- Solicited-node multicast tests ---- */

static void test_solicited_node(void) {
    uint8_t addr[16] = {0x20, 0x01, 0x0D, 0xB8, 0,    0,    0,    0,
                        0x02, 0x1A, 0x2B, 0xFF, 0xFE, 0x3C, 0x4D, 0x5E};
    uint8_t mcast[16];
    ASSERT_EQ(nfs_slaac_solicited_node(addr, mcast), 0);
    ASSERT_EQ(mcast[0], 0xFF);
    ASSERT_EQ(mcast[1], 0x02);
    ASSERT_EQ(mcast[11], 0x01);
    ASSERT_EQ(mcast[12], 0xFF);
    ASSERT_EQ(mcast[13], 0x3C); /* addr[13] */
    ASSERT_EQ(mcast[14], 0x4D); /* addr[14] */
    ASSERT_EQ(mcast[15], 0x5E); /* addr[15] */
}

static void test_solicited_node_null(void) {
    uint8_t mcast[16];
    ASSERT_EQ(nfs_slaac_solicited_node(NULL, mcast), -1);
}

/* ---- Format tests ---- */

static void test_format_ipv6(void) {
    uint8_t addr[16] = {0xFE, 0x80, 0,    0,    0,    0,    0,    0,
                        0x02, 0x1A, 0x2B, 0xFF, 0xFE, 0x3C, 0x4D, 0x5E};
    char str[64];
    nfs_slaac_format_ipv6(addr, str, sizeof(str));
    ASSERT_TRUE(strcmp(str, "fe80:0000:0000:0000:021a:2bff:fe3c:4d5e") == 0);
}

static void test_format_ipv6_small_buffer(void) {
    uint8_t addr[16] = {0};
    char str[10];
    nfs_slaac_format_ipv6(addr, str, sizeof(str));
    ASSERT_EQ(str[0], '\0');
}

int main(void) {
    printf("SLAAC Tests\n");

    test_eui64_basic();
    test_eui64_ul_bit_set();
    test_eui64_all_ff();
    test_eui64_null();
    test_generate_addr_basic();
    test_generate_addr_nonzero_iid();
    test_generate_addr_null();
    test_generate_addr_bad_prefix_len();
    test_validate_prefix_good();
    test_validate_prefix_not_64();
    test_validate_prefix_multicast();
    test_validate_prefix_linklocal();
    test_validate_prefix_all_zeros();
    test_validate_prefix_null();
    test_linklocal();
    test_linklocal_null();
    test_solicited_node();
    test_solicited_node_null();
    test_format_ipv6();
    test_format_ipv6_small_buffer();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
