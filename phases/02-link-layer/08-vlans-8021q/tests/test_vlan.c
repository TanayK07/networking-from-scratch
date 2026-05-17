/* Unit tests for 802.1Q VLAN tag handling. */

#include "../vlan.h"

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

/* ------------------------------------------------------------------ */
/*  Test: struct size                                                   */
/* ------------------------------------------------------------------ */
static void test_struct_size(void) {
    ASSERT_EQ(sizeof(struct nfs_vlan_tag), 4);
}

/* ------------------------------------------------------------------ */
/*  Test: set + get PCP/DEI/VID roundtrip                              */
/* ------------------------------------------------------------------ */
static void test_set_get_roundtrip(void) {
    struct nfs_vlan_tag t;

    nfs_vlan_set(&t, 3, 0, 100);
    ASSERT_EQ(nfs_vlan_pcp(&t), 3);
    ASSERT_EQ(nfs_vlan_dei(&t), 0);
    ASSERT_EQ(nfs_vlan_vid(&t), 100);
    ASSERT_EQ(ntohs(t.tpid), 0x8100);

    nfs_vlan_set(&t, 7, 1, 4094);
    ASSERT_EQ(nfs_vlan_pcp(&t), 7);
    ASSERT_EQ(nfs_vlan_dei(&t), 1);
    ASSERT_EQ(nfs_vlan_vid(&t), 4094);

    nfs_vlan_set(&t, 0, 0, 0);
    ASSERT_EQ(nfs_vlan_pcp(&t), 0);
    ASSERT_EQ(nfs_vlan_dei(&t), 0);
    ASSERT_EQ(nfs_vlan_vid(&t), 0);

    nfs_vlan_set(&t, 0, 0, 1);
    ASSERT_EQ(nfs_vlan_vid(&t), 1);
}

/* ------------------------------------------------------------------ */
/*  Test: PCP extraction at boundaries                                 */
/* ------------------------------------------------------------------ */
static void test_pcp_boundaries(void) {
    struct nfs_vlan_tag t;
    for (uint8_t pcp = 0; pcp <= 7; pcp++) {
        nfs_vlan_set(&t, pcp, 0, 0);
        ASSERT_EQ(nfs_vlan_pcp(&t), pcp);
    }
}

/* ------------------------------------------------------------------ */
/*  Test: VID range                                                    */
/* ------------------------------------------------------------------ */
static void test_vid_range(void) {
    struct nfs_vlan_tag t;
    uint16_t test_vids[] = {0, 1, 2, 100, 1000, 2048, 4094};
    for (size_t i = 0; i < sizeof(test_vids) / sizeof(test_vids[0]); i++) {
        nfs_vlan_set(&t, 0, 0, test_vids[i]);
        ASSERT_EQ(nfs_vlan_vid(&t), test_vids[i]);
    }
}

/* ------------------------------------------------------------------ */
/*  Test: is_tagged detection                                          */
/* ------------------------------------------------------------------ */
static void test_is_tagged(void) {
    /* Untagged frame (IPv4 ethertype at offset 12). */
    uint8_t untagged[60];
    memset(untagged, 0, sizeof(untagged));
    untagged[12] = 0x08;
    untagged[13] = 0x00;
    ASSERT_EQ(nfs_vlan_is_tagged(untagged, sizeof(untagged)), 0);

    /* Tagged frame (0x8100 at offset 12). */
    uint8_t tagged[64];
    memset(tagged, 0, sizeof(tagged));
    tagged[12] = 0x81;
    tagged[13] = 0x00;
    ASSERT_EQ(nfs_vlan_is_tagged(tagged, sizeof(tagged)), 1);

    /* Too short. */
    ASSERT_EQ(nfs_vlan_is_tagged(tagged, 13), 0);
    ASSERT_EQ(nfs_vlan_is_tagged(NULL, 0), 0);
}

/* ------------------------------------------------------------------ */
/*  Test: tag insert — frame grows by 4                                */
/* ------------------------------------------------------------------ */
static void test_tag_insert(void) {
    uint8_t frame[60];
    memset(frame, 0, sizeof(frame));
    /* dst */
    memset(frame, 0xFF, 6);
    /* src */
    uint8_t src[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    memcpy(frame + 6, src, 6);
    /* ethertype = IPv4 */
    frame[12] = 0x08;
    frame[13] = 0x00;

    uint8_t out[128];
    int new_len = nfs_vlan_tag_insert(frame, 60, 200, 5, out, sizeof(out));

    ASSERT_EQ(new_len, 64); /* 60 + 4 */
    ASSERT_EQ(nfs_vlan_is_tagged(out, (size_t)new_len), 1);

    /* MACs should be preserved. */
    ASSERT_EQ(out[0], 0xFF);
    ASSERT_EQ(out[5], 0xFF);
    ASSERT_TRUE(memcmp(out + 6, src, 6) == 0);

    /* VLAN tag at offset 12-15. */
    struct nfs_vlan_tag tag;
    memcpy(&tag, out + 12, 4);
    ASSERT_EQ(nfs_vlan_vid(&tag), 200);
    ASSERT_EQ(nfs_vlan_pcp(&tag), 5);

    /* Original ethertype at offset 16-17. */
    ASSERT_EQ(out[16], 0x08);
    ASSERT_EQ(out[17], 0x00);
}

/* ------------------------------------------------------------------ */
/*  Test: tag strip — frame shrinks by 4                               */
/* ------------------------------------------------------------------ */
static void test_tag_strip(void) {
    /* Build a tagged frame first. */
    uint8_t frame[60];
    memset(frame, 0, sizeof(frame));
    memset(frame, 0xFF, 6);
    uint8_t src[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    memcpy(frame + 6, src, 6);
    frame[12] = 0x08;
    frame[13] = 0x00;

    uint8_t tagged[128];
    int tlen = nfs_vlan_tag_insert(frame, 60, 42, 2, tagged, sizeof(tagged));
    ASSERT_TRUE(tlen > 0);

    uint8_t stripped[128];
    uint16_t vid = 0;
    int slen = nfs_vlan_tag_strip(tagged, (size_t)tlen, stripped, sizeof(stripped), &vid);

    ASSERT_EQ(slen, 60); /* back to original size */
    ASSERT_EQ(vid, 42);
    ASSERT_EQ(nfs_vlan_is_tagged(stripped, (size_t)slen), 0);

    /* MACs preserved. */
    ASSERT_EQ(stripped[0], 0xFF);
    ASSERT_TRUE(memcmp(stripped + 6, src, 6) == 0);

    /* Original ethertype restored at offset 12. */
    ASSERT_EQ(stripped[12], 0x08);
    ASSERT_EQ(stripped[13], 0x00);
}

/* ------------------------------------------------------------------ */
/*  Test: strip untagged frame fails                                   */
/* ------------------------------------------------------------------ */
static void test_strip_untagged_fails(void) {
    uint8_t frame[60];
    memset(frame, 0, sizeof(frame));
    frame[12] = 0x08;
    frame[13] = 0x00; /* IPv4, not VLAN */

    uint8_t out[128];
    uint16_t vid;
    ASSERT_EQ(nfs_vlan_tag_strip(frame, 60, out, sizeof(out), &vid), -1);
}

/* ------------------------------------------------------------------ */
/*  Test: insert with buffer too small                                 */
/* ------------------------------------------------------------------ */
static void test_insert_buffer_too_small(void) {
    uint8_t frame[60];
    memset(frame, 0, sizeof(frame));
    frame[12] = 0x08;
    frame[13] = 0x00;

    uint8_t out[62]; /* needs 64, only 62 available */
    ASSERT_EQ(nfs_vlan_tag_insert(frame, 60, 100, 0, out, sizeof(out)), -1);
}

/* ------------------------------------------------------------------ */
/*  Test: format                                                       */
/* ------------------------------------------------------------------ */
static void test_format(void) {
    struct nfs_vlan_tag t;
    nfs_vlan_set(&t, 3, 0, 100);

    char buf[64];
    nfs_vlan_format(&t, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "VID=100") != NULL);
    ASSERT_TRUE(strstr(buf, "PCP=3") != NULL);
    ASSERT_TRUE(strstr(buf, "DEI=0") != NULL);
}

/* ------------------------------------------------------------------ */
/*  Test: insert + strip roundtrip preserves frame                     */
/* ------------------------------------------------------------------ */
static void test_insert_strip_roundtrip(void) {
    uint8_t frame[60];
    for (int i = 0; i < 60; i++)
        frame[i] = (uint8_t)(i & 0xFF);
    /* Set valid ethertype (not 0x8100 to avoid confusion). */
    frame[12] = 0x08;
    frame[13] = 0x00;

    uint8_t tagged[128];
    int tlen = nfs_vlan_tag_insert(frame, 60, 999, 7, tagged, sizeof(tagged));
    ASSERT_TRUE(tlen > 0);

    uint8_t restored[128];
    uint16_t vid;
    int slen = nfs_vlan_tag_strip(tagged, (size_t)tlen, restored, sizeof(restored), &vid);
    ASSERT_EQ(slen, 60);
    ASSERT_EQ(vid, 999);
    ASSERT_TRUE(memcmp(restored, frame, 60) == 0);
}

/* ------------------------------------------------------------------ */
/*  Test: DEI bit isolation                                            */
/* ------------------------------------------------------------------ */
static void test_dei_bit(void) {
    struct nfs_vlan_tag t;

    nfs_vlan_set(&t, 0, 1, 0);
    ASSERT_EQ(nfs_vlan_dei(&t), 1);
    ASSERT_EQ(nfs_vlan_pcp(&t), 0);
    ASSERT_EQ(nfs_vlan_vid(&t), 0);

    nfs_vlan_set(&t, 0, 0, 0);
    ASSERT_EQ(nfs_vlan_dei(&t), 0);
}

/* ------------------------------------------------------------------ */
/*  Test: strip with NULL vid_out                                      */
/* ------------------------------------------------------------------ */
static void test_strip_null_vid(void) {
    uint8_t frame[60];
    memset(frame, 0, sizeof(frame));
    frame[12] = 0x08;
    frame[13] = 0x00;

    uint8_t tagged[128];
    int tlen = nfs_vlan_tag_insert(frame, 60, 50, 0, tagged, sizeof(tagged));
    ASSERT_TRUE(tlen > 0);

    uint8_t stripped[128];
    int slen = nfs_vlan_tag_strip(tagged, (size_t)tlen, stripped, sizeof(stripped), NULL);
    ASSERT_EQ(slen, 60);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_struct_size();
    test_set_get_roundtrip();
    test_pcp_boundaries();
    test_vid_range();
    test_is_tagged();
    test_tag_insert();
    test_tag_strip();
    test_strip_untagged_fails();
    test_insert_buffer_too_small();
    test_format();
    test_insert_strip_roundtrip();
    test_dei_bit();
    test_strip_null_vid();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
