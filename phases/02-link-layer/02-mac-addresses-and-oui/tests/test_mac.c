/* Unit tests for MAC address utilities. */

#include "../mac.h"

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

/* ------------------------------------------------------------------ */
/*  Test: broadcast detection                                          */
/* ------------------------------------------------------------------ */
static void test_broadcast(void) {
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ASSERT_EQ(nfs_mac_is_broadcast(bcast), 1);
    ASSERT_EQ(nfs_mac_is_multicast(bcast), 1); /* broadcast is also multicast */
    ASSERT_EQ(nfs_mac_is_unicast(bcast), 0);

    uint8_t not_bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};
    ASSERT_EQ(nfs_mac_is_broadcast(not_bcast), 0);

    uint8_t zero[6] = {0};
    ASSERT_EQ(nfs_mac_is_broadcast(zero), 0);
}

/* ------------------------------------------------------------------ */
/*  Test: multicast bit                                                */
/* ------------------------------------------------------------------ */
static void test_multicast(void) {
    /* IPv4 multicast: 01:00:5e:xx:xx:xx */
    uint8_t mcast[6] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};
    ASSERT_EQ(nfs_mac_is_multicast(mcast), 1);
    ASSERT_EQ(nfs_mac_is_unicast(mcast), 0);

    /* STP multicast: 01:80:c2:00:00:00 */
    uint8_t stp[6] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x00};
    ASSERT_EQ(nfs_mac_is_multicast(stp), 1);

    /* IPv6 multicast: 33:33:xx:xx:xx:xx */
    uint8_t mcast6[6] = {0x33, 0x33, 0xFF, 0x00, 0x00, 0x01};
    ASSERT_EQ(nfs_mac_is_multicast(mcast6), 1);

    /* Normal unicast */
    uint8_t uni[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    ASSERT_EQ(nfs_mac_is_multicast(uni), 0);
    ASSERT_EQ(nfs_mac_is_unicast(uni), 1);
}

/* ------------------------------------------------------------------ */
/*  Test: local admin bit                                              */
/* ------------------------------------------------------------------ */
static void test_local_admin(void) {
    /* Docker-style locally administered: 02:42:ac:11:00:02 */
    uint8_t local[6] = {0x02, 0x42, 0xAC, 0x11, 0x00, 0x02};
    ASSERT_EQ(nfs_mac_is_local(local), 1);
    ASSERT_EQ(nfs_mac_is_global(local), 0);
    ASSERT_EQ(nfs_mac_is_unicast(local), 1);

    /* Globally unique (OUI-based) */
    uint8_t global[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    ASSERT_EQ(nfs_mac_is_local(global), 0);
    ASSERT_EQ(nfs_mac_is_global(global), 1);

    /* Locally administered + multicast (both bits set: 0x03) */
    uint8_t both[6] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00};
    ASSERT_EQ(nfs_mac_is_local(both), 1);
    ASSERT_EQ(nfs_mac_is_multicast(both), 1);
}

/* ------------------------------------------------------------------ */
/*  Test: OUI extraction                                               */
/* ------------------------------------------------------------------ */
static void test_oui(void) {
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33};
    uint8_t oui[3];
    nfs_mac_get_oui(mac, oui);
    ASSERT_EQ(oui[0], 0xAA);
    ASSERT_EQ(oui[1], 0xBB);
    ASSERT_EQ(oui[2], 0xCC);

    uint8_t zero[6] = {0};
    nfs_mac_get_oui(zero, oui);
    ASSERT_EQ(oui[0], 0x00);
    ASSERT_EQ(oui[1], 0x00);
    ASSERT_EQ(oui[2], 0x00);
}

/* ------------------------------------------------------------------ */
/*  Test: format                                                       */
/* ------------------------------------------------------------------ */
static void test_format(void) {
    uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    char buf[32];
    ASSERT_EQ(nfs_mac_format(mac, buf, sizeof(buf)), 0);
    ASSERT_TRUE(strcmp(buf, "de:ad:be:ef:00:01") == 0);

    /* Buffer too small */
    ASSERT_EQ(nfs_mac_format(mac, buf, 10), -1);
    ASSERT_EQ(nfs_mac_format(mac, buf, 17), -1); /* need 18 */

    /* NULL inputs */
    ASSERT_EQ(nfs_mac_format(NULL, buf, 32), -1);
    ASSERT_EQ(nfs_mac_format(mac, NULL, 32), -1);
}

/* ------------------------------------------------------------------ */
/*  Test: parse colon-separated                                        */
/* ------------------------------------------------------------------ */
static void test_parse_colon(void) {
    uint8_t mac[6];
    ASSERT_EQ(nfs_mac_parse("de:ad:be:ef:00:01", mac), 0);
    ASSERT_EQ(mac[0], 0xDE);
    ASSERT_EQ(mac[1], 0xAD);
    ASSERT_EQ(mac[2], 0xBE);
    ASSERT_EQ(mac[3], 0xEF);
    ASSERT_EQ(mac[4], 0x00);
    ASSERT_EQ(mac[5], 0x01);
}

/* ------------------------------------------------------------------ */
/*  Test: parse dash-separated                                         */
/* ------------------------------------------------------------------ */
static void test_parse_dash(void) {
    uint8_t mac[6];
    ASSERT_EQ(nfs_mac_parse("AA-BB-CC-DD-EE-FF", mac), 0);
    ASSERT_EQ(mac[0], 0xAA);
    ASSERT_EQ(mac[5], 0xFF);
}

/* ------------------------------------------------------------------ */
/*  Test: parse invalid                                                */
/* ------------------------------------------------------------------ */
static void test_parse_invalid(void) {
    uint8_t mac[6];
    ASSERT_EQ(nfs_mac_parse(NULL, mac), -1);
    ASSERT_EQ(nfs_mac_parse("not-a-mac-addr", mac), -1);
    ASSERT_EQ(nfs_mac_parse("", mac), -1);
    ASSERT_EQ(nfs_mac_parse("aa:bb:cc", mac), -1); /* too few */
}

/* ------------------------------------------------------------------ */
/*  Test: format+parse roundtrip                                       */
/* ------------------------------------------------------------------ */
static void test_roundtrip(void) {
    uint8_t original[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    char buf[32];
    ASSERT_EQ(nfs_mac_format(original, buf, sizeof(buf)), 0);

    uint8_t parsed[6];
    ASSERT_EQ(nfs_mac_parse(buf, parsed), 0);
    ASSERT_TRUE(memcmp(original, parsed, 6) == 0);
}

/* ------------------------------------------------------------------ */
/*  Test: type string                                                  */
/* ------------------------------------------------------------------ */
static void test_type_str(void) {
    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ASSERT_TRUE(strcmp(nfs_mac_type_str(bcast), "broadcast") == 0);

    uint8_t mcast[6] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};
    ASSERT_TRUE(strcmp(nfs_mac_type_str(mcast), "multicast") == 0);

    uint8_t local[6] = {0x02, 0x42, 0xAC, 0x11, 0x00, 0x02};
    ASSERT_TRUE(strcmp(nfs_mac_type_str(local), "unicast/local") == 0);

    uint8_t global[6] = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    ASSERT_TRUE(strcmp(nfs_mac_type_str(global), "unicast/global") == 0);
}

/* ------------------------------------------------------------------ */
/*  Test: zero MAC                                                     */
/* ------------------------------------------------------------------ */
static void test_zero_mac(void) {
    uint8_t zero[6] = {0};
    ASSERT_EQ(nfs_mac_is_broadcast(zero), 0);
    ASSERT_EQ(nfs_mac_is_multicast(zero), 0);
    ASSERT_EQ(nfs_mac_is_unicast(zero), 1);
    ASSERT_EQ(nfs_mac_is_local(zero), 0);
    ASSERT_EQ(nfs_mac_is_global(zero), 1);
    ASSERT_TRUE(strcmp(nfs_mac_type_str(zero), "unicast/global") == 0);
}

/* ------------------------------------------------------------------ */
/*  Test: well-known multicast MACs                                    */
/* ------------------------------------------------------------------ */
static void test_wellknown_multicast(void) {
    /* CDP/VTP: 01:00:0c:cc:cc:cc */
    uint8_t cdp[6] = {0x01, 0x00, 0x0C, 0xCC, 0xCC, 0xCC};
    ASSERT_EQ(nfs_mac_is_multicast(cdp), 1);
    ASSERT_EQ(nfs_mac_is_broadcast(cdp), 0);

    /* LLDP: 01:80:c2:00:00:0e */
    uint8_t lldp[6] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E};
    ASSERT_EQ(nfs_mac_is_multicast(lldp), 1);
    ASSERT_TRUE(strcmp(nfs_mac_type_str(lldp), "multicast") == 0);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_broadcast();
    test_multicast();
    test_local_admin();
    test_oui();
    test_format();
    test_parse_colon();
    test_parse_dash();
    test_parse_invalid();
    test_roundtrip();
    test_type_str();
    test_zero_mac();
    test_wellknown_multicast();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
