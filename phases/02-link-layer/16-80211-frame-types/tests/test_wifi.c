/* Unit tests for IEEE 802.11 frame control and header parsing. */

#include "../wifi.h"

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
/*  Test 1: FC pack/unpack roundtrip (beacon)                          */
/* ------------------------------------------------------------------ */
static void test_fc_roundtrip_beacon(void) {
    struct nfs_wifi_fc fc;
    memset(&fc, 0, sizeof(fc));
    fc.type = NFS_WIFI_TYPE_MGMT;
    fc.subtype = NFS_WIFI_MGMT_BEACON;

    uint16_t raw = nfs_wifi_fc_pack(&fc);

    struct nfs_wifi_fc fc2;
    ASSERT_EQ(nfs_wifi_fc_unpack(raw, &fc2), 0);
    ASSERT_EQ(fc2.version, 0);
    ASSERT_EQ(fc2.type, NFS_WIFI_TYPE_MGMT);
    ASSERT_EQ(fc2.subtype, NFS_WIFI_MGMT_BEACON);
    ASSERT_EQ(fc2.to_ds, 0);
    ASSERT_EQ(fc2.from_ds, 0);
    ASSERT_EQ(fc2.retry, 0);
    ASSERT_EQ(fc2.protected_frame, 0);
}

/* ------------------------------------------------------------------ */
/*  Test 2: FC pack bit layout — beacon = 0x0080                       */
/* ------------------------------------------------------------------ */
static void test_fc_pack_beacon_bits(void) {
    struct nfs_wifi_fc fc;
    memset(&fc, 0, sizeof(fc));
    fc.type = NFS_WIFI_TYPE_MGMT;      /* 0 */
    fc.subtype = NFS_WIFI_MGMT_BEACON; /* 8 */

    /* version=00, type=00, subtype=1000 -> byte0 = 0x80
     * all flags 0 -> byte1 = 0x00
     * raw = 0x0080 */
    uint16_t raw = nfs_wifi_fc_pack(&fc);
    ASSERT_EQ(raw, 0x0080);
}

/* ------------------------------------------------------------------ */
/*  Test 3: FC data frame = 0x0008                                     */
/* ------------------------------------------------------------------ */
static void test_fc_pack_data(void) {
    struct nfs_wifi_fc fc;
    memset(&fc, 0, sizeof(fc));
    fc.type = NFS_WIFI_TYPE_DATA;    /* 2 */
    fc.subtype = NFS_WIFI_DATA_DATA; /* 0 */

    /* version=00, type=10, subtype=0000 -> byte0 = 0x08 */
    uint16_t raw = nfs_wifi_fc_pack(&fc);
    ASSERT_EQ(raw, 0x0008);
}

/* ------------------------------------------------------------------ */
/*  Test 4: FC ACK frame = 0x00D4                                      */
/* ------------------------------------------------------------------ */
static void test_fc_pack_ack(void) {
    struct nfs_wifi_fc fc;
    memset(&fc, 0, sizeof(fc));
    fc.type = NFS_WIFI_TYPE_CTRL;   /* 1 */
    fc.subtype = NFS_WIFI_CTRL_ACK; /* 13 */

    /* version=00, type=01, subtype=1101 -> bits = 0b11010100 = 0xD4
     * byte1 = 0x00
     * raw = 0x00D4 */
    uint16_t raw = nfs_wifi_fc_pack(&fc);
    ASSERT_EQ(raw, 0x00D4);
}

/* ------------------------------------------------------------------ */
/*  Test 5: FC RTS frame = 0x00B4                                      */
/* ------------------------------------------------------------------ */
static void test_fc_pack_rts(void) {
    struct nfs_wifi_fc fc;
    memset(&fc, 0, sizeof(fc));
    fc.type = NFS_WIFI_TYPE_CTRL;   /* 1 */
    fc.subtype = NFS_WIFI_CTRL_RTS; /* 11 */

    /* version=00, type=01, subtype=1011 -> bits = 0b10110100 = 0xB4 */
    uint16_t raw = nfs_wifi_fc_pack(&fc);
    ASSERT_EQ(raw, 0x00B4);
}

/* ------------------------------------------------------------------ */
/*  Test 6: FC flags — to_ds=1                                         */
/* ------------------------------------------------------------------ */
static void test_fc_to_ds_flag(void) {
    struct nfs_wifi_fc fc;
    memset(&fc, 0, sizeof(fc));
    fc.type = NFS_WIFI_TYPE_DATA;
    fc.to_ds = 1;

    uint16_t raw = nfs_wifi_fc_pack(&fc);
    /* bit 8 should be set */
    ASSERT_TRUE((raw & (1 << 8)) != 0);
    /* from_ds (bit 9) should not be set */
    ASSERT_TRUE((raw & (1 << 9)) == 0);
}

/* ------------------------------------------------------------------ */
/*  Test 7: FC protected frame — bit 14                                */
/* ------------------------------------------------------------------ */
static void test_fc_protected_frame(void) {
    uint16_t raw = (1 << 14); /* only protected_frame bit set */

    struct nfs_wifi_fc fc;
    ASSERT_EQ(nfs_wifi_fc_unpack(raw, &fc), 0);
    ASSERT_EQ(fc.protected_frame, 1);
    ASSERT_EQ(fc.to_ds, 0);
    ASSERT_EQ(fc.from_ds, 0);
    ASSERT_EQ(fc.retry, 0);
}

/* ------------------------------------------------------------------ */
/*  Test 8: Header size — management = 24                              */
/* ------------------------------------------------------------------ */
static void test_hdr_size_mgmt(void) {
    struct nfs_wifi_fc fc;
    memset(&fc, 0, sizeof(fc));
    fc.type = NFS_WIFI_TYPE_MGMT;
    fc.subtype = NFS_WIFI_MGMT_BEACON;

    ASSERT_EQ(nfs_wifi_hdr_size(&fc), 24);
}

/* ------------------------------------------------------------------ */
/*  Test 9: Header size — control ACK = 10                             */
/* ------------------------------------------------------------------ */
static void test_hdr_size_ctrl_ack(void) {
    struct nfs_wifi_fc fc;
    memset(&fc, 0, sizeof(fc));
    fc.type = NFS_WIFI_TYPE_CTRL;
    fc.subtype = NFS_WIFI_CTRL_ACK;

    ASSERT_EQ(nfs_wifi_hdr_size(&fc), 10);
}

/* ------------------------------------------------------------------ */
/*  Test 10: Header size — control RTS = 16                            */
/* ------------------------------------------------------------------ */
static void test_hdr_size_ctrl_rts(void) {
    struct nfs_wifi_fc fc;
    memset(&fc, 0, sizeof(fc));
    fc.type = NFS_WIFI_TYPE_CTRL;
    fc.subtype = NFS_WIFI_CTRL_RTS;

    ASSERT_EQ(nfs_wifi_hdr_size(&fc), 16);
}

/* ------------------------------------------------------------------ */
/*  Test 11: Header size — data (no addr4) = 24                        */
/* ------------------------------------------------------------------ */
static void test_hdr_size_data(void) {
    struct nfs_wifi_fc fc;
    memset(&fc, 0, sizeof(fc));
    fc.type = NFS_WIFI_TYPE_DATA;
    fc.subtype = NFS_WIFI_DATA_DATA;

    ASSERT_EQ(nfs_wifi_hdr_size(&fc), 24);
}

/* ------------------------------------------------------------------ */
/*  Test 12: Header size — WDS (to_ds=1, from_ds=1) = 30               */
/* ------------------------------------------------------------------ */
static void test_hdr_size_wds(void) {
    struct nfs_wifi_fc fc;
    memset(&fc, 0, sizeof(fc));
    fc.type = NFS_WIFI_TYPE_DATA;
    fc.subtype = NFS_WIFI_DATA_DATA;
    fc.to_ds = 1;
    fc.from_ds = 1;

    ASSERT_EQ(nfs_wifi_hdr_size(&fc), 30);
}

/* ------------------------------------------------------------------ */
/*  Test 13: Header build/parse roundtrip — beacon                     */
/* ------------------------------------------------------------------ */
static void test_hdr_build_parse_roundtrip(void) {
    struct nfs_wifi_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.fc.type = NFS_WIFI_TYPE_MGMT;
    hdr.fc.subtype = NFS_WIFI_MGMT_BEACON;
    hdr.duration = 0x1234;

    uint8_t a1[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t a2[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    uint8_t a3[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    memcpy(hdr.addr1, a1, 6);
    memcpy(hdr.addr2, a2, 6);
    memcpy(hdr.addr3, a3, 6);
    hdr.seq_ctrl = nfs_wifi_seq_ctrl(42, 0);

    uint8_t buf[64];
    int len = nfs_wifi_hdr_build(&hdr, buf, sizeof(buf));
    ASSERT_EQ(len, 24);

    struct nfs_wifi_hdr parsed;
    ASSERT_EQ(nfs_wifi_hdr_parse(buf, (size_t)len, &parsed), 0);
    ASSERT_EQ(parsed.fc.type, NFS_WIFI_TYPE_MGMT);
    ASSERT_EQ(parsed.fc.subtype, NFS_WIFI_MGMT_BEACON);
    ASSERT_EQ(parsed.duration, 0x1234);
    ASSERT_TRUE(memcmp(parsed.addr1, a1, 6) == 0);
    ASSERT_TRUE(memcmp(parsed.addr2, a2, 6) == 0);
    ASSERT_TRUE(memcmp(parsed.addr3, a3, 6) == 0);
    ASSERT_EQ(parsed.seq_ctrl, nfs_wifi_seq_ctrl(42, 0));
    ASSERT_EQ(parsed.has_addr4, 0);
}

/* ------------------------------------------------------------------ */
/*  Test 14: Address interpretation — from AP (ToDS=0, FromDS=1)       */
/* ------------------------------------------------------------------ */
static void test_addr_from_ap(void) {
    struct nfs_wifi_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.fc.type = NFS_WIFI_TYPE_DATA;
    hdr.fc.from_ds = 1;

    uint8_t da[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t bssid[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    uint8_t sa[] = {0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC};
    memcpy(hdr.addr1, da, 6);
    memcpy(hdr.addr2, bssid, 6);
    memcpy(hdr.addr3, sa, 6);

    const uint8_t *pda, *psa, *pbssid;
    ASSERT_EQ(nfs_wifi_get_da(&hdr, &pda), 0);
    ASSERT_EQ(nfs_wifi_get_sa(&hdr, &psa), 0);
    ASSERT_EQ(nfs_wifi_get_bssid(&hdr, &pbssid), 0);

    ASSERT_TRUE(memcmp(pda, da, 6) == 0);
    ASSERT_TRUE(memcmp(psa, sa, 6) == 0);
    ASSERT_TRUE(memcmp(pbssid, bssid, 6) == 0);
}

/* ------------------------------------------------------------------ */
/*  Test 15: Address interpretation — to AP (ToDS=1, FromDS=0)         */
/* ------------------------------------------------------------------ */
static void test_addr_to_ap(void) {
    struct nfs_wifi_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.fc.type = NFS_WIFI_TYPE_DATA;
    hdr.fc.to_ds = 1;

    uint8_t bssid[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    uint8_t sa[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t da[] = {0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC};
    memcpy(hdr.addr1, bssid, 6);
    memcpy(hdr.addr2, sa, 6);
    memcpy(hdr.addr3, da, 6);

    const uint8_t *pda, *psa, *pbssid;
    ASSERT_EQ(nfs_wifi_get_da(&hdr, &pda), 0);
    ASSERT_EQ(nfs_wifi_get_sa(&hdr, &psa), 0);
    ASSERT_EQ(nfs_wifi_get_bssid(&hdr, &pbssid), 0);

    ASSERT_TRUE(memcmp(pda, da, 6) == 0);
    ASSERT_TRUE(memcmp(psa, sa, 6) == 0);
    ASSERT_TRUE(memcmp(pbssid, bssid, 6) == 0);
}

/* ------------------------------------------------------------------ */
/*  Test 16: Address interpretation — WDS (ToDS=1, FromDS=1)           */
/* ------------------------------------------------------------------ */
static void test_addr_wds(void) {
    struct nfs_wifi_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.fc.type = NFS_WIFI_TYPE_DATA;
    hdr.fc.to_ds = 1;
    hdr.fc.from_ds = 1;
    hdr.has_addr4 = 1;

    uint8_t ra[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t ta[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    uint8_t da[] = {0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC};
    uint8_t sa[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    memcpy(hdr.addr1, ra, 6);
    memcpy(hdr.addr2, ta, 6);
    memcpy(hdr.addr3, da, 6);
    memcpy(hdr.addr4, sa, 6);

    const uint8_t *pda, *psa;
    ASSERT_EQ(nfs_wifi_get_da(&hdr, &pda), 0);
    ASSERT_EQ(nfs_wifi_get_sa(&hdr, &psa), 0);

    ASSERT_TRUE(memcmp(pda, da, 6) == 0);
    ASSERT_TRUE(memcmp(psa, sa, 6) == 0);

    /* WDS has no single BSSID — expect -1 */
    const uint8_t *pbssid;
    ASSERT_EQ(nfs_wifi_get_bssid(&hdr, &pbssid), -1);
}

/* ------------------------------------------------------------------ */
/*  Test 17: Sequence control — seq=100, frag=3                        */
/* ------------------------------------------------------------------ */
static void test_seq_ctrl_build(void) {
    uint16_t sc = nfs_wifi_seq_ctrl(100, 3);
    /* (100 << 4) | 3 = 1600 + 3 = 1603 */
    ASSERT_EQ(sc, 1603);
    ASSERT_EQ(nfs_wifi_seq_num(sc), 100);
    ASSERT_EQ(nfs_wifi_frag_num(sc), 3);
}

/* ------------------------------------------------------------------ */
/*  Test 18: Sequence control extract — 0x1234                         */
/* ------------------------------------------------------------------ */
static void test_seq_ctrl_extract(void) {
    uint16_t sc = 0x1234;
    /* seq = 0x1234 >> 4 = 0x123 = 291 */
    ASSERT_EQ(nfs_wifi_seq_num(sc), 291);
    /* frag = 0x1234 & 0x0F = 4 */
    ASSERT_EQ(nfs_wifi_frag_num(sc), 4);
}

/* ------------------------------------------------------------------ */
/*  Test 19: Sequence number max = 4095                                */
/* ------------------------------------------------------------------ */
static void test_seq_num_max(void) {
    uint16_t sc = nfs_wifi_seq_ctrl(4095, 0);
    ASSERT_EQ(nfs_wifi_seq_num(sc), 4095);
}

/* ------------------------------------------------------------------ */
/*  Test 20: Fragment number max = 15                                  */
/* ------------------------------------------------------------------ */
static void test_frag_num_max(void) {
    uint16_t sc = nfs_wifi_seq_ctrl(0, 15);
    ASSERT_EQ(nfs_wifi_frag_num(sc), 15);
}

/* ------------------------------------------------------------------ */
/*  Test 21: Type names                                                */
/* ------------------------------------------------------------------ */
static void test_type_names(void) {
    ASSERT_TRUE(strcmp(nfs_wifi_type_name(0), "Management") == 0);
    ASSERT_TRUE(strcmp(nfs_wifi_type_name(1), "Control") == 0);
    ASSERT_TRUE(strcmp(nfs_wifi_type_name(2), "Data") == 0);
    ASSERT_TRUE(strcmp(nfs_wifi_type_name(3), "Unknown") == 0);
}

/* ------------------------------------------------------------------ */
/*  Test 22: Subtype names                                             */
/* ------------------------------------------------------------------ */
static void test_subtype_names(void) {
    ASSERT_TRUE(strcmp(nfs_wifi_subtype_name(0, 8), "Beacon") == 0);
    ASSERT_TRUE(strcmp(nfs_wifi_subtype_name(1, 13), "ACK") == 0);
    ASSERT_TRUE(strcmp(nfs_wifi_subtype_name(2, 0), "Data") == 0);
}

/* ------------------------------------------------------------------ */
/*  Test 23: Parse rejects too short (< 10 bytes)                      */
/* ------------------------------------------------------------------ */
static void test_parse_too_short(void) {
    uint8_t buf[9] = {0};
    struct nfs_wifi_hdr hdr;

    ASSERT_EQ(nfs_wifi_hdr_parse(buf, 9, &hdr), -1);
    ASSERT_EQ(nfs_wifi_hdr_parse(buf, 0, &hdr), -1);
    ASSERT_EQ(nfs_wifi_hdr_parse(NULL, 24, &hdr), -1);
}

/* ------------------------------------------------------------------ */
/*  Test 24: Management subtypes constants                             */
/* ------------------------------------------------------------------ */
static void test_mgmt_subtype_constants(void) {
    ASSERT_EQ(NFS_WIFI_MGMT_BEACON, 8);
    ASSERT_EQ(NFS_WIFI_MGMT_AUTH, 11);
    ASSERT_EQ(NFS_WIFI_MGMT_DEAUTH, 12);
    ASSERT_EQ(NFS_WIFI_MGMT_PROBE_REQ, 4);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_fc_roundtrip_beacon();
    test_fc_pack_beacon_bits();
    test_fc_pack_data();
    test_fc_pack_ack();
    test_fc_pack_rts();
    test_fc_to_ds_flag();
    test_fc_protected_frame();
    test_hdr_size_mgmt();
    test_hdr_size_ctrl_ack();
    test_hdr_size_ctrl_rts();
    test_hdr_size_data();
    test_hdr_size_wds();
    test_hdr_build_parse_roundtrip();
    test_addr_from_ap();
    test_addr_to_ap();
    test_addr_wds();
    test_seq_ctrl_build();
    test_seq_ctrl_extract();
    test_seq_num_max();
    test_frag_num_max();
    test_type_names();
    test_subtype_names();
    test_parse_too_short();
    test_mgmt_subtype_constants();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
