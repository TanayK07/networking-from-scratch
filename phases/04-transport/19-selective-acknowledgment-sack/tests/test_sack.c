/* Tests for TCP SACK scoreboard.
 *
 * Test families:
 *   1.  Init state
 *   2.  Add single block
 *   3.  Add overlapping blocks merge
 *   4.  Add adjacent blocks merge
 *   5.  Most-recent-first ordering
 *   6.  Cumulative ACK advance removes old blocks
 *   7.  is_sacked check
 *   8.  Build+parse option roundtrip
 *   9.  Holes detection
 *  10.  4 block limit
 *  11.  Multiple merges into one
 *  12.  Advance cumack trims block left edge
 *  13.  Empty scoreboard holes
 *  14.  Parse malformed option
 */

#include "sack.h"

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

/* ---------------------------------------------------------------
 * Test 1: Init state
 * --------------------------------------------------------------- */

static void test_init(void) {
    printf("  init state...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    ASSERT_EQ(sb.cum_ack, 1000);
    ASSERT_EQ(sb.nblocks, 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: Add single block
 * --------------------------------------------------------------- */

static void test_add_single(void) {
    printf("  add single block...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    int n = nfs_sack_add_block(&sb, 1200, 1300);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(sb.nblocks, 1);
    ASSERT_EQ(sb.blocks[0].left, 1200);
    ASSERT_EQ(sb.blocks[0].right, 1300);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: Overlapping blocks merge
 * --------------------------------------------------------------- */

static void test_overlapping_merge(void) {
    printf("  overlapping blocks merge...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    nfs_sack_add_block(&sb, 1200, 1300);
    nfs_sack_add_block(&sb, 1250, 1400);

    ASSERT_EQ(sb.nblocks, 1);
    /* The merged block should be the most recent (position 0) */
    ASSERT_EQ(sb.blocks[0].left, 1200);
    ASSERT_EQ(sb.blocks[0].right, 1400);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: Adjacent blocks merge
 * --------------------------------------------------------------- */

static void test_adjacent_merge(void) {
    printf("  adjacent blocks merge...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    nfs_sack_add_block(&sb, 1200, 1300);
    nfs_sack_add_block(&sb, 1300, 1400);

    ASSERT_EQ(sb.nblocks, 1);
    ASSERT_EQ(sb.blocks[0].left, 1200);
    ASSERT_EQ(sb.blocks[0].right, 1400);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: Most-recent-first ordering
 * --------------------------------------------------------------- */

static void test_most_recent_first(void) {
    printf("  most-recent-first ordering...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    nfs_sack_add_block(&sb, 1200, 1300);
    nfs_sack_add_block(&sb, 1500, 1600);

    ASSERT_EQ(sb.nblocks, 2);
    /* Most recently added should be first */
    ASSERT_EQ(sb.blocks[0].left, 1500);
    ASSERT_EQ(sb.blocks[0].right, 1600);
    ASSERT_EQ(sb.blocks[1].left, 1200);
    ASSERT_EQ(sb.blocks[1].right, 1300);

    /* Add another — it should be first */
    nfs_sack_add_block(&sb, 1800, 1900);
    ASSERT_EQ(sb.blocks[0].left, 1800);
    ASSERT_EQ(sb.blocks[0].right, 1900);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: Cumulative ACK advance removes old blocks
 * --------------------------------------------------------------- */

static void test_cumack_advance(void) {
    printf("  cumack advance removes old blocks...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    nfs_sack_add_block(&sb, 1100, 1200);
    nfs_sack_add_block(&sb, 1300, 1400);
    nfs_sack_add_block(&sb, 1500, 1600);

    ASSERT_EQ(sb.nblocks, 3);

    /* Advance past first block */
    nfs_sack_advance_cumack(&sb, 1200);
    ASSERT_EQ(sb.cum_ack, 1200);
    ASSERT_EQ(sb.nblocks, 2);

    /* Advance past second block */
    nfs_sack_advance_cumack(&sb, 1400);
    ASSERT_EQ(sb.nblocks, 1);
    ASSERT_EQ(sb.blocks[0].left, 1500);
    ASSERT_EQ(sb.blocks[0].right, 1600);

    /* Advance past all */
    nfs_sack_advance_cumack(&sb, 1600);
    ASSERT_EQ(sb.nblocks, 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: is_sacked check
 * --------------------------------------------------------------- */

static void test_is_sacked(void) {
    printf("  is_sacked check...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    nfs_sack_add_block(&sb, 1200, 1300);
    nfs_sack_add_block(&sb, 1500, 1600);

    /* Inside first block */
    ASSERT_TRUE(nfs_sack_is_sacked(&sb, 1200));
    ASSERT_TRUE(nfs_sack_is_sacked(&sb, 1250));
    ASSERT_TRUE(nfs_sack_is_sacked(&sb, 1299));

    /* Right edge is exclusive */
    ASSERT_TRUE(!nfs_sack_is_sacked(&sb, 1300));

    /* Inside second block */
    ASSERT_TRUE(nfs_sack_is_sacked(&sb, 1500));
    ASSERT_TRUE(nfs_sack_is_sacked(&sb, 1550));

    /* Between blocks */
    ASSERT_TRUE(!nfs_sack_is_sacked(&sb, 1400));

    /* Below cum_ack */
    ASSERT_TRUE(!nfs_sack_is_sacked(&sb, 999));

    /* Above all blocks */
    ASSERT_TRUE(!nfs_sack_is_sacked(&sb, 1700));

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: Build+parse option roundtrip
 * --------------------------------------------------------------- */

static void test_build_parse_roundtrip(void) {
    printf("  build+parse option roundtrip...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    nfs_sack_add_block(&sb, 1200, 1300);
    nfs_sack_add_block(&sb, 1500, 1600);

    uint8_t wire[64];
    int n = nfs_sack_build_option(&sb, wire, sizeof(wire));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(wire[0], 5);         /* SACK kind */
    ASSERT_EQ(wire[1], 2 + 8 * 2); /* 2 header + 2 blocks * 8 bytes */

    /* Parse it back */
    struct nfs_sack_block parsed[4];
    size_t nfound;
    int rc = nfs_sack_parse_option(wire, (size_t)n, parsed, 4, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 2);

    /* Blocks should match (in same order as built) */
    ASSERT_EQ(parsed[0].left, sb.blocks[0].left);
    ASSERT_EQ(parsed[0].right, sb.blocks[0].right);
    ASSERT_EQ(parsed[1].left, sb.blocks[1].left);
    ASSERT_EQ(parsed[1].right, sb.blocks[1].right);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: Holes detection
 * --------------------------------------------------------------- */

static void test_holes(void) {
    printf("  holes detection...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    /* cum_ack=1000, blocks: [1200,1300), [1500,1600) */
    nfs_sack_add_block(&sb, 1200, 1300);
    nfs_sack_add_block(&sb, 1500, 1600);

    struct nfs_sack_block holes[8];
    size_t nholes = nfs_sack_holes(&sb, holes, 8);

    ASSERT_EQ(nholes, 2);

    /* Hole 1: [1000, 1200) */
    ASSERT_EQ(holes[0].left, 1000);
    ASSERT_EQ(holes[0].right, 1200);

    /* Hole 2: [1300, 1500) */
    ASSERT_EQ(holes[1].left, 1300);
    ASSERT_EQ(holes[1].right, 1500);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: 4 block limit
 * --------------------------------------------------------------- */

static void test_block_limit(void) {
    printf("  4 block limit...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 0);

    nfs_sack_add_block(&sb, 100, 200);
    nfs_sack_add_block(&sb, 300, 400);
    nfs_sack_add_block(&sb, 500, 600);
    nfs_sack_add_block(&sb, 700, 800);

    ASSERT_EQ(sb.nblocks, 4);

    /* Adding a 5th should still result in at most 4 blocks */
    nfs_sack_add_block(&sb, 900, 1000);
    ASSERT_TRUE(sb.nblocks <= 4);

    /* Most recent should be first */
    ASSERT_EQ(sb.blocks[0].left, 900);
    ASSERT_EQ(sb.blocks[0].right, 1000);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 11: Multiple merges into one
 * --------------------------------------------------------------- */

static void test_multi_merge(void) {
    printf("  multiple merges into one...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 0);

    nfs_sack_add_block(&sb, 100, 200);
    nfs_sack_add_block(&sb, 300, 400);
    ASSERT_EQ(sb.nblocks, 2);

    /* Add a block that bridges them: [150, 350) */
    nfs_sack_add_block(&sb, 150, 350);
    ASSERT_EQ(sb.nblocks, 1);
    ASSERT_EQ(sb.blocks[0].left, 100);
    ASSERT_EQ(sb.blocks[0].right, 400);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 12: Advance cumack trims block left edge
 * --------------------------------------------------------------- */

static void test_cumack_trim(void) {
    printf("  cumack trim...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    nfs_sack_add_block(&sb, 1100, 1300);
    ASSERT_EQ(sb.nblocks, 1);

    /* Advance into the middle of the block */
    nfs_sack_advance_cumack(&sb, 1200);
    ASSERT_EQ(sb.nblocks, 1);
    ASSERT_EQ(sb.blocks[0].left, 1200);
    ASSERT_EQ(sb.blocks[0].right, 1300);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 13: Empty scoreboard holes
 * --------------------------------------------------------------- */

static void test_empty_holes(void) {
    printf("  empty scoreboard holes...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    struct nfs_sack_block holes[8];
    size_t nholes = nfs_sack_holes(&sb, holes, 8);
    ASSERT_EQ(nholes, 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 14: Parse malformed option
 * --------------------------------------------------------------- */

static void test_parse_malformed(void) {
    printf("  parse malformed option...");

    struct nfs_sack_block blocks[4];
    size_t nfound;

    /* Too short */
    uint8_t buf1[] = {5};
    ASSERT_EQ(nfs_sack_parse_option(buf1, sizeof(buf1), blocks, 4, &nfound), -1);

    /* Wrong kind */
    uint8_t buf2[] = {4, 2};
    ASSERT_EQ(nfs_sack_parse_option(buf2, sizeof(buf2), blocks, 4, &nfound), -1);

    /* Length not multiple of 8+2 */
    uint8_t buf3[] = {5, 5, 0, 0, 0};
    ASSERT_EQ(nfs_sack_parse_option(buf3, sizeof(buf3), blocks, 4, &nfound), -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 15: Build with empty scoreboard returns 0
 * --------------------------------------------------------------- */

static void test_build_empty(void) {
    printf("  build empty scoreboard...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    uint8_t wire[64];
    int n = nfs_sack_build_option(&sb, wire, sizeof(wire));
    ASSERT_EQ(n, 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 16: Single hole when one block present
 * --------------------------------------------------------------- */

static void test_single_hole(void) {
    printf("  single hole...");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 500);

    nfs_sack_add_block(&sb, 600, 700);

    struct nfs_sack_block holes[8];
    size_t nholes = nfs_sack_holes(&sb, holes, 8);
    ASSERT_EQ(nholes, 1);
    ASSERT_EQ(holes[0].left, 500);
    ASSERT_EQ(holes[0].right, 600);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    printf("SACK scoreboard test suite\n");

    test_init();
    test_add_single();
    test_overlapping_merge();
    test_adjacent_merge();
    test_most_recent_first();
    test_cumack_advance();
    test_is_sacked();
    test_build_parse_roundtrip();
    test_holes();
    test_block_limit();
    test_multi_merge();
    test_cumack_trim();
    test_empty_holes();
    test_parse_malformed();
    test_build_empty();
    test_single_hole();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
