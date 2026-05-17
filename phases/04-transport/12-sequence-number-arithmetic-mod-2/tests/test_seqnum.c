/* Tests for TCP sequence number arithmetic (RFC 793, RFC 9293).
 *
 * TCP sequence numbers wrap at 2^32.  The tricky part is comparison:
 *   a < b  iff  (int32_t)(a - b) < 0
 *
 * Test families:
 *   1.  Basic ordering (no wrap)
 *   2.  Wraparound: 0xFFFFFFF0 < 0x00000010
 *   3.  Equal values
 *   4.  Add with no wrap
 *   5.  Add with wrap
 *   6.  Diff across wrap
 *   7.  Between (no wrap)
 *   8.  Between (with wrap)
 *   9.  Window check (no wrap)
 *  10.  Window check (with wrap)
 *  11.  Edge: seq 0 vs MAX
 *  12.  Edge: halfway point (2^31 boundary)
 *  13.  Window size 0
 *  14.  Large window
 *  15.  Comprehensive wraparound comparisons
 */

#include "../seqnum.h"

#include <stdio.h>

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
 * Test 1: basic ordering (no wrap)
 * --------------------------------------------------------------- */
static void test_basic_ordering(void) {
    printf("  basic ordering...");

    ASSERT_EQ(nfs_seq_lt(1, 2), 1);
    ASSERT_EQ(nfs_seq_lt(2, 1), 0);
    ASSERT_EQ(nfs_seq_lt(1, 1), 0);

    ASSERT_EQ(nfs_seq_le(1, 2), 1);
    ASSERT_EQ(nfs_seq_le(2, 1), 0);
    ASSERT_EQ(nfs_seq_le(1, 1), 1);

    ASSERT_EQ(nfs_seq_gt(2, 1), 1);
    ASSERT_EQ(nfs_seq_gt(1, 2), 0);
    ASSERT_EQ(nfs_seq_gt(1, 1), 0);

    ASSERT_EQ(nfs_seq_ge(2, 1), 1);
    ASSERT_EQ(nfs_seq_ge(1, 2), 0);
    ASSERT_EQ(nfs_seq_ge(1, 1), 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: wraparound
 *
 * 0xFFFFFFF0 should be "less than" 0x00000010 in sequence space
 * because 0xFFFFFFF0 is 32 positions before 0x00000010.
 * --------------------------------------------------------------- */
static void test_wraparound(void) {
    printf("  wraparound...");

    /* 0xFFFFFFF0 is "before" 0x00000010 (diff = -32). */
    ASSERT_EQ(nfs_seq_lt(0xFFFFFFF0, 0x00000010), 1);
    ASSERT_EQ(nfs_seq_gt(0x00000010, 0xFFFFFFF0), 1);

    /* 0xFFFFFFFF is "before" 0x00000000. */
    ASSERT_EQ(nfs_seq_lt(0xFFFFFFFF, 0x00000000), 1);
    ASSERT_EQ(nfs_seq_lt(0x00000000, 0xFFFFFFFF), 0);

    /* 0xFFFFFFFF is "before" 0x00000001. */
    ASSERT_EQ(nfs_seq_lt(0xFFFFFFFF, 0x00000001), 1);

    /* Small wraps. */
    ASSERT_EQ(nfs_seq_lt(0xFFFFFFFE, 0x00000002), 1);
    ASSERT_EQ(nfs_seq_gt(0x00000002, 0xFFFFFFFE), 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: equal values
 * --------------------------------------------------------------- */
static void test_equal_values(void) {
    printf("  equal values...");

    ASSERT_EQ(nfs_seq_lt(0, 0), 0);
    ASSERT_EQ(nfs_seq_le(0, 0), 1);
    ASSERT_EQ(nfs_seq_gt(0, 0), 0);
    ASSERT_EQ(nfs_seq_ge(0, 0), 1);

    ASSERT_EQ(nfs_seq_lt(0xFFFFFFFF, 0xFFFFFFFF), 0);
    ASSERT_EQ(nfs_seq_le(0xFFFFFFFF, 0xFFFFFFFF), 1);

    ASSERT_EQ(nfs_seq_diff(100, 100), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: add with no wrap
 * --------------------------------------------------------------- */
static void test_add_no_wrap(void) {
    printf("  add (no wrap)...");

    ASSERT_EQ(nfs_seq_add(0, 1), 1);
    ASSERT_EQ(nfs_seq_add(100, 50), 150);
    ASSERT_EQ(nfs_seq_add(0, 0), 0);
    ASSERT_EQ(nfs_seq_add(1000, 1000), 2000);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: add with wrap
 * --------------------------------------------------------------- */
static void test_add_with_wrap(void) {
    printf("  add (with wrap)...");

    ASSERT_EQ(nfs_seq_add(0xFFFFFFFF, 1), 0x00000000);
    ASSERT_EQ(nfs_seq_add(0xFFFFFFFF, 2), 0x00000001);
    ASSERT_EQ(nfs_seq_add(0xFFFFFFF0, 32), 0x00000010);
    ASSERT_EQ(nfs_seq_add(0xFFFFFFF0, 16), 0x00000000);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: diff across wrap
 * --------------------------------------------------------------- */
static void test_diff_across_wrap(void) {
    printf("  diff across wrap...");

    /* 0x00000010 - 0xFFFFFFF0 = 32 (unsigned), interpreted as +32. */
    ASSERT_EQ(nfs_seq_diff(0x00000010, 0xFFFFFFF0), 32);

    /* 0xFFFFFFF0 - 0x00000010 = -32. */
    ASSERT_EQ(nfs_seq_diff(0xFFFFFFF0, 0x00000010), -32);

    /* 0x00000000 - 0xFFFFFFFF = 1. */
    ASSERT_EQ(nfs_seq_diff(0x00000000, 0xFFFFFFFF), 1);

    /* 0xFFFFFFFF - 0x00000000 = -1. */
    ASSERT_EQ(nfs_seq_diff(0xFFFFFFFF, 0x00000000), -1);

    /* Normal diff. */
    ASSERT_EQ(nfs_seq_diff(200, 100), 100);
    ASSERT_EQ(nfs_seq_diff(100, 200), -100);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: between (no wrap)
 * --------------------------------------------------------------- */
static void test_between_no_wrap(void) {
    printf("  between (no wrap)...");

    ASSERT_EQ(nfs_seq_between(5, 1, 10), 1);
    ASSERT_EQ(nfs_seq_between(1, 1, 10), 1);  /* lo is inclusive */
    ASSERT_EQ(nfs_seq_between(10, 1, 10), 1); /* hi is inclusive */
    ASSERT_EQ(nfs_seq_between(0, 1, 10), 0);
    ASSERT_EQ(nfs_seq_between(11, 1, 10), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: between (with wrap)
 * --------------------------------------------------------------- */
static void test_between_with_wrap(void) {
    printf("  between (with wrap)...");

    /* Range [0xFFFFFFF0, 0x00000010] wrapping around. */
    ASSERT_EQ(nfs_seq_between(0xFFFFFFF0, 0xFFFFFFF0, 0x00000010), 1);
    ASSERT_EQ(nfs_seq_between(0xFFFFFFFF, 0xFFFFFFF0, 0x00000010), 1);
    ASSERT_EQ(nfs_seq_between(0x00000000, 0xFFFFFFF0, 0x00000010), 1);
    ASSERT_EQ(nfs_seq_between(0x00000010, 0xFFFFFFF0, 0x00000010), 1);
    ASSERT_EQ(nfs_seq_between(0x00000011, 0xFFFFFFF0, 0x00000010), 0);
    ASSERT_EQ(nfs_seq_between(0xFFFFFFEF, 0xFFFFFFF0, 0x00000010), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: window check (no wrap)
 * --------------------------------------------------------------- */
static void test_window_no_wrap(void) {
    printf("  window (no wrap)...");

    /* Window [100, 200) -- half-open. */
    ASSERT_EQ(nfs_seq_in_window(100, 100, 100), 1); /* start inclusive */
    ASSERT_EQ(nfs_seq_in_window(150, 100, 100), 1);
    ASSERT_EQ(nfs_seq_in_window(199, 100, 100), 1);
    ASSERT_EQ(nfs_seq_in_window(200, 100, 100), 0); /* end exclusive */
    ASSERT_EQ(nfs_seq_in_window(99, 100, 100), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: window check (with wrap)
 * --------------------------------------------------------------- */
static void test_window_with_wrap(void) {
    printf("  window (with wrap)...");

    /* Window starting at 0xFFFFFFF0, size 32.
     * Covers [0xFFFFFFF0, 0x00000010). */
    ASSERT_EQ(nfs_seq_in_window(0xFFFFFFF0, 0xFFFFFFF0, 32), 1);
    ASSERT_EQ(nfs_seq_in_window(0xFFFFFFFF, 0xFFFFFFF0, 32), 1);
    ASSERT_EQ(nfs_seq_in_window(0x00000000, 0xFFFFFFF0, 32), 1);
    ASSERT_EQ(nfs_seq_in_window(0x0000000F, 0xFFFFFFF0, 32), 1);
    ASSERT_EQ(nfs_seq_in_window(0x00000010, 0xFFFFFFF0, 32), 0);
    ASSERT_EQ(nfs_seq_in_window(0xFFFFFFEF, 0xFFFFFFF0, 32), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 11: edge: seq 0 vs MAX
 * --------------------------------------------------------------- */
static void test_zero_vs_max(void) {
    printf("  seq 0 vs MAX...");

    /* 0xFFFFFFFF is "before" 0 (diff = -1). */
    ASSERT_EQ(nfs_seq_lt(0xFFFFFFFF, 0), 1);
    ASSERT_EQ(nfs_seq_gt(0, 0xFFFFFFFF), 1);
    ASSERT_EQ(nfs_seq_diff(0, 0xFFFFFFFF), 1);
    ASSERT_EQ(nfs_seq_diff(0xFFFFFFFF, 0), -1);

    /* add(MAX, 1) = 0. */
    ASSERT_EQ(nfs_seq_add(0xFFFFFFFF, 1), 0);

    /* Window: MAX in [MAX, MAX+2)? Yes. */
    ASSERT_EQ(nfs_seq_in_window(0xFFFFFFFF, 0xFFFFFFFF, 2), 1);
    /* 0 in [MAX, MAX+2)? Yes. */
    ASSERT_EQ(nfs_seq_in_window(0, 0xFFFFFFFF, 2), 1);
    /* 1 in [MAX, MAX+2)? No. */
    ASSERT_EQ(nfs_seq_in_window(1, 0xFFFFFFFF, 2), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 12: edge: halfway point (2^31 boundary)
 *
 * At exactly 2^31 distance, the comparison is inherently ambiguous.
 * Both (int32_t)(a - b) and (int32_t)(b - a) equal INT32_MIN < 0,
 * so both a < b AND b < a are "true".  This is the antipodal point
 * in the circular sequence space -- exactly 2^31 apart.
 *
 * In practice, TCP never compares sequence numbers this far apart
 * because the window size is bounded well below 2^31.
 * --------------------------------------------------------------- */
static void test_halfway_point(void) {
    printf("  halfway point...");

    nfs_seq_t a = 0;
    nfs_seq_t b = 0x80000000; /* exactly 2^31 away */

    /* (int32_t)(0 - 0x80000000) = INT32_MIN < 0, so lt says a < b. */
    ASSERT_EQ(nfs_seq_lt(a, b), 1);
    /* (int32_t)(0x80000000 - 0) = INT32_MIN < 0, so lt ALSO says b < a!
     * This is the known ambiguity at the antipodal point. */
    ASSERT_EQ(nfs_seq_lt(b, a), 1);

    /* At 2^31 - 1 distance, clearly "before" (no ambiguity). */
    nfs_seq_t c = 0x7FFFFFFF;
    ASSERT_EQ(nfs_seq_lt(a, c), 1);
    ASSERT_EQ(nfs_seq_gt(c, a), 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 13: window size 0
 * --------------------------------------------------------------- */
static void test_window_size_zero(void) {
    printf("  window size 0...");

    /* Empty window: nothing is in it. */
    ASSERT_EQ(nfs_seq_in_window(100, 100, 0), 0);
    ASSERT_EQ(nfs_seq_in_window(0, 0, 0), 0);
    ASSERT_EQ(nfs_seq_in_window(0xFFFFFFFF, 0xFFFFFFFF, 0), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 14: large window
 * --------------------------------------------------------------- */
static void test_large_window(void) {
    printf("  large window...");

    /* Window of size UINT32_MAX covers almost everything. */
    ASSERT_EQ(nfs_seq_in_window(0, 0, 0xFFFFFFFF), 1);
    ASSERT_EQ(nfs_seq_in_window(0xFFFFFFFE, 0, 0xFFFFFFFF), 1);
    /* 0xFFFFFFFF is just outside [0, 0 + 0xFFFFFFFF). */
    ASSERT_EQ(nfs_seq_in_window(0xFFFFFFFF, 0, 0xFFFFFFFF), 0);

    /* Window of size 1: only the start. */
    ASSERT_EQ(nfs_seq_in_window(100, 100, 1), 1);
    ASSERT_EQ(nfs_seq_in_window(101, 100, 1), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 15: comprehensive wraparound comparisons
 * --------------------------------------------------------------- */
static void test_comprehensive_wrap(void) {
    printf("  comprehensive wrap...");

    /* Near-zero values. */
    ASSERT_EQ(nfs_seq_lt(0xFFFFFF00, 0x00000100), 1);
    ASSERT_EQ(nfs_seq_gt(0x00000100, 0xFFFFFF00), 1);
    ASSERT_EQ(nfs_seq_le(0xFFFFFF00, 0x00000100), 1);
    ASSERT_EQ(nfs_seq_ge(0x00000100, 0xFFFFFF00), 1);

    /* Diff should be 512. */
    ASSERT_EQ(nfs_seq_diff(0x00000100, 0xFFFFFF00), 512);

    /* Between at wrap. */
    ASSERT_EQ(nfs_seq_between(0x00000000, 0xFFFFFF00, 0x00000100), 1);

    /* Window at wrap boundary. */
    ASSERT_EQ(nfs_seq_in_window(0x00000000, 0xFFFFFF00, 512), 1);
    ASSERT_EQ(nfs_seq_in_window(0x000000FF, 0xFFFFFF00, 512), 1);
    ASSERT_EQ(nfs_seq_in_window(0x00000100, 0xFFFFFF00, 512), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */
int main(void) {
    printf("Sequence number arithmetic (RFC 793) test suite\n");

    test_basic_ordering();
    test_wraparound();
    test_equal_values();
    test_add_no_wrap();
    test_add_with_wrap();
    test_diff_across_wrap();
    test_between_no_wrap();
    test_between_with_wrap();
    test_window_no_wrap();
    test_window_with_wrap();
    test_zero_vs_max();
    test_halfway_point();
    test_window_size_zero();
    test_large_window();
    test_comprehensive_wrap();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
