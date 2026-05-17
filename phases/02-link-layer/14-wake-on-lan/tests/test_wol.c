/* Unit tests for WoL magic packet builder/validator. */

#include "../wol.h"
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

/* ---- build + validate roundtrip ---- */

static void test_build_validate_roundtrip(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t pkt[128];

    int n = nfs_wol_build(mac, pkt, sizeof(pkt));
    ASSERT_EQ(n, NFS_WOL_MAGIC_SIZE);

    uint8_t extracted[6];
    int rc = nfs_wol_validate(pkt, (size_t)n, extracted);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(memcmp(mac, extracted, 6) == 0);
}

/* ---- first 6 bytes all 0xFF ---- */

static void test_sync_bytes(void) {
    uint8_t mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t pkt[128];

    nfs_wol_build(mac, pkt, sizeof(pkt));

    ASSERT_EQ(pkt[0], 0xFF);
    ASSERT_EQ(pkt[1], 0xFF);
    ASSERT_EQ(pkt[2], 0xFF);
    ASSERT_EQ(pkt[3], 0xFF);
    ASSERT_EQ(pkt[4], 0xFF);
    ASSERT_EQ(pkt[5], 0xFF);
}

/* ---- MAC repeated exactly 16 times ---- */

static void test_mac_repeated_16(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t pkt[128];

    nfs_wol_build(mac, pkt, sizeof(pkt));

    for (int i = 0; i < 16; i++) {
        ASSERT_TRUE(memcmp(pkt + 6 + i * 6, mac, 6) == 0);
    }
}

/* ---- validate rejects wrong sync bytes ---- */

static void test_validate_bad_sync(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t pkt[128];

    nfs_wol_build(mac, pkt, sizeof(pkt));
    pkt[3] = 0x00; /* Corrupt sync byte */

    uint8_t out[6];
    ASSERT_EQ(nfs_wol_validate(pkt, NFS_WOL_MAGIC_SIZE, out), -1);
}

/* ---- validate rejects short packet ---- */

static void test_validate_short_packet(void) {
    uint8_t pkt[50];
    memset(pkt, 0xFF, sizeof(pkt));

    uint8_t out[6];
    ASSERT_EQ(nfs_wol_validate(pkt, sizeof(pkt), out), -1);
}

/* ---- validate rejects inconsistent MACs ---- */

static void test_validate_inconsistent_macs(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t pkt[128];

    nfs_wol_build(mac, pkt, sizeof(pkt));
    /* Corrupt one copy of the MAC (the 10th repetition) */
    pkt[6 + 9 * 6 + 2] ^= 0x01;

    uint8_t out[6];
    ASSERT_EQ(nfs_wol_validate(pkt, NFS_WOL_MAGIC_SIZE, out), -1);
}

/* ---- password append (4-byte) ---- */

static void test_password_4byte(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t pw[] = {0x12, 0x34, 0x56, 0x78};
    uint8_t pkt[128];

    int n = nfs_wol_build_with_password(mac, pw, 4, pkt, sizeof(pkt));
    ASSERT_EQ(n, 106);

    /* Still a valid magic packet for the first 102 bytes */
    uint8_t out[6];
    ASSERT_EQ(nfs_wol_validate(pkt, (size_t)n, out), 0);
    ASSERT_TRUE(memcmp(out, mac, 6) == 0);

    /* Check password bytes */
    ASSERT_EQ(pkt[102], 0x12);
    ASSERT_EQ(pkt[103], 0x34);
    ASSERT_EQ(pkt[104], 0x56);
    ASSERT_EQ(pkt[105], 0x78);
}

/* ---- password append (6-byte) ---- */

static void test_password_6byte(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t pw[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t pkt[128];

    int n = nfs_wol_build_with_password(mac, pw, 6, pkt, sizeof(pkt));
    ASSERT_EQ(n, 108);

    ASSERT_EQ(pkt[102], 0xAA);
    ASSERT_EQ(pkt[107], 0xFF);
}

/* ---- no password ---- */

static void test_password_none(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t pkt[128];

    int n = nfs_wol_build_with_password(mac, NULL, 0, pkt, sizeof(pkt));
    ASSERT_EQ(n, 102);
}

/* ---- invalid password length ---- */

static void test_password_invalid_len(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t pw[] = {0x01, 0x02, 0x03};
    uint8_t pkt[128];

    int n = nfs_wol_build_with_password(mac, pw, 3, pkt, sizeof(pkt));
    ASSERT_EQ(n, -1);
}

/* ---- mac parse colon format ---- */

static void test_mac_parse_colon(void) {
    uint8_t mac[6];
    ASSERT_EQ(nfs_wol_mac_parse("aa:bb:cc:dd:ee:ff", mac), 0);
    ASSERT_EQ(mac[0], 0xAA);
    ASSERT_EQ(mac[1], 0xBB);
    ASSERT_EQ(mac[2], 0xCC);
    ASSERT_EQ(mac[3], 0xDD);
    ASSERT_EQ(mac[4], 0xEE);
    ASSERT_EQ(mac[5], 0xFF);
}

/* ---- mac parse dash format ---- */

static void test_mac_parse_dash(void) {
    uint8_t mac[6];
    ASSERT_EQ(nfs_wol_mac_parse("00-1A-2B-3C-4D-5E", mac), 0);
    ASSERT_EQ(mac[0], 0x00);
    ASSERT_EQ(mac[1], 0x1A);
    ASSERT_EQ(mac[2], 0x2B);
    ASSERT_EQ(mac[3], 0x3C);
    ASSERT_EQ(mac[4], 0x4D);
    ASSERT_EQ(mac[5], 0x5E);
}

/* ---- mac parse invalid ---- */

static void test_mac_parse_invalid(void) {
    uint8_t mac[6];
    ASSERT_EQ(nfs_wol_mac_parse("zz:bb:cc:dd:ee:ff", mac), -1);
    ASSERT_EQ(nfs_wol_mac_parse("aa:bb", mac), -1);
    ASSERT_EQ(nfs_wol_mac_parse("", mac), -1);
    ASSERT_EQ(nfs_wol_mac_parse(NULL, mac), -1);
}

/* ---- build with NULL args ---- */

static void test_build_null(void) {
    uint8_t pkt[128];
    ASSERT_EQ(nfs_wol_build(NULL, pkt, sizeof(pkt)), -1);

    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    ASSERT_EQ(nfs_wol_build(mac, NULL, 128), -1);
}

/* ---- build with buffer too small ---- */

static void test_build_small_buffer(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    uint8_t pkt[50];
    ASSERT_EQ(nfs_wol_build(mac, pkt, sizeof(pkt)), -1);
}

/* ---- validate NULL args ---- */

static void test_validate_null(void) {
    uint8_t out[6];
    ASSERT_EQ(nfs_wol_validate(NULL, 102, out), -1);

    uint8_t pkt[102];
    ASSERT_EQ(nfs_wol_validate(pkt, 102, NULL), -1);
}

/* ---- mac format ---- */

static void test_mac_format(void) {
    uint8_t mac[] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    char buf[32];
    ASSERT_EQ(nfs_wol_mac_format(mac, buf, sizeof(buf)), 0);
    ASSERT_TRUE(strcmp(buf, "00:1a:2b:3c:4d:5e") == 0);
}

int main(void) {
    printf("Wake-on-LAN Tests\n");

    test_build_validate_roundtrip();
    test_sync_bytes();
    test_mac_repeated_16();
    test_validate_bad_sync();
    test_validate_short_packet();
    test_validate_inconsistent_macs();
    test_password_4byte();
    test_password_6byte();
    test_password_none();
    test_password_invalid_len();
    test_mac_parse_colon();
    test_mac_parse_dash();
    test_mac_parse_invalid();
    test_build_null();
    test_build_small_buffer();
    test_validate_null();
    test_mac_format();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
