/* Unit tests for nfs_retx_queue functions. */

#include "../retx_queue.h"
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

/* ---- basic push and find ---- */

static void test_push_and_find(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 8);

    uint8_t data[] = {0x01, 0x02, 0x03};
    ASSERT_EQ(nfs_retx_queue_push(&q, 1000, data, 3, 1.0), 0);
    ASSERT_EQ(nfs_retx_queue_size(&q), 1);

    struct nfs_retx_seg *seg = nfs_retx_queue_find(&q, 1000);
    ASSERT_TRUE(seg != NULL);
    ASSERT_EQ(seg->seq, 1000);
    ASSERT_EQ(seg->data_len, 3);
    ASSERT_EQ(seg->data[0], 0x01);
    ASSERT_EQ(seg->data[1], 0x02);
    ASSERT_EQ(seg->data[2], 0x03);
    ASSERT_EQ(seg->retx_count, 0);

    nfs_retx_queue_free(&q);
}

/* ---- find non-existent ---- */

static void test_find_missing(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 4);

    uint8_t data[] = {0xAA};
    nfs_retx_queue_push(&q, 5000, data, 1, 0.0);

    ASSERT_TRUE(nfs_retx_queue_find(&q, 9999) == NULL);
    ASSERT_TRUE(nfs_retx_queue_find(&q, 5000) != NULL);

    nfs_retx_queue_free(&q);
}

/* ---- cumulative ACK removes segments ---- */

static void test_ack_removes_segments(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 8);

    uint8_t d1[] = {1};
    uint8_t d2[] = {2};
    uint8_t d3[] = {3};
    nfs_retx_queue_push(&q, 100, d1, 1, 0.0);
    nfs_retx_queue_push(&q, 200, d2, 1, 0.1);
    nfs_retx_queue_push(&q, 300, d3, 1, 0.2);
    ASSERT_EQ(nfs_retx_queue_size(&q), 3);

    /* ACK 250: removes seq 100 and 200 */
    int removed = nfs_retx_queue_ack(&q, 250);
    ASSERT_EQ(removed, 2);
    ASSERT_EQ(nfs_retx_queue_size(&q), 1);

    /* seq 300 remains */
    ASSERT_TRUE(nfs_retx_queue_find(&q, 300) != NULL);
    ASSERT_TRUE(nfs_retx_queue_find(&q, 100) == NULL);
    ASSERT_TRUE(nfs_retx_queue_find(&q, 200) == NULL);

    nfs_retx_queue_free(&q);
}

/* ---- partial ACK ---- */

static void test_partial_ack(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 8);

    uint8_t d = 0;
    nfs_retx_queue_push(&q, 1000, &d, 1, 0.0);
    nfs_retx_queue_push(&q, 1460, &d, 1, 0.1);
    nfs_retx_queue_push(&q, 2920, &d, 1, 0.2);

    /* Partial ACK: only first segment */
    int removed = nfs_retx_queue_ack(&q, 1460);
    ASSERT_EQ(removed, 1);
    ASSERT_EQ(nfs_retx_queue_size(&q), 2);
    ASSERT_TRUE(nfs_retx_queue_find(&q, 1460) != NULL);
    ASSERT_TRUE(nfs_retx_queue_find(&q, 2920) != NULL);

    nfs_retx_queue_free(&q);
}

/* ---- timeout detection ---- */

static void test_timeout_detection(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 8);

    uint8_t d = 0;
    nfs_retx_queue_push(&q, 100, &d, 1, 1.0);
    nfs_retx_queue_push(&q, 200, &d, 1, 2.0);
    nfs_retx_queue_push(&q, 300, &d, 1, 3.0);

    /* At time 4.0 with RTO 1.5: seg 100 (age 3.0) and seg 200 (age 2.0) timeout */
    int count = nfs_retx_queue_timeout(&q, 4.0, 1.5);
    ASSERT_EQ(count, 2);

    struct nfs_retx_seg *s1 = nfs_retx_queue_find(&q, 100);
    ASSERT_TRUE(s1 != NULL);
    ASSERT_EQ(s1->retx_count, 1);
    /* send_time updated to now */
    ASSERT_TRUE(s1->send_time >= 3.99 && s1->send_time <= 4.01);

    struct nfs_retx_seg *s3 = nfs_retx_queue_find(&q, 300);
    ASSERT_TRUE(s3 != NULL);
    ASSERT_EQ(s3->retx_count, 0);

    nfs_retx_queue_free(&q);
}

/* ---- retx_count increments on repeated timeouts ---- */

static void test_retx_count_increments(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 4);

    uint8_t d = 0xAA;
    nfs_retx_queue_push(&q, 500, &d, 1, 0.0);

    /* First timeout at t=2.0, RTO=1.0 */
    nfs_retx_queue_timeout(&q, 2.0, 1.0);
    struct nfs_retx_seg *seg = nfs_retx_queue_find(&q, 500);
    ASSERT_TRUE(seg != NULL);
    ASSERT_EQ(seg->retx_count, 1);

    /* Second timeout at t=4.0, RTO=1.0 */
    nfs_retx_queue_timeout(&q, 4.0, 1.0);
    ASSERT_EQ(seg->retx_count, 2);

    /* Third timeout at t=6.0, RTO=1.0 */
    nfs_retx_queue_timeout(&q, 6.0, 1.0);
    ASSERT_EQ(seg->retx_count, 3);

    nfs_retx_queue_free(&q);
}

/* ---- empty after full ACK ---- */

static void test_empty_after_full_ack(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 8);

    uint8_t d = 0;
    nfs_retx_queue_push(&q, 0, &d, 1, 0.0);
    nfs_retx_queue_push(&q, 100, &d, 1, 0.1);
    nfs_retx_queue_push(&q, 200, &d, 1, 0.2);

    ASSERT_EQ(nfs_retx_queue_empty(&q), 0);

    /* ACK everything */
    int removed = nfs_retx_queue_ack(&q, 300);
    ASSERT_EQ(removed, 3);
    ASSERT_EQ(nfs_retx_queue_empty(&q), 1);
    ASSERT_EQ(nfs_retx_queue_size(&q), 0);

    nfs_retx_queue_free(&q);
}

/* ---- capacity limits ---- */

static void test_capacity_limits(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 2);

    uint8_t d = 0;
    ASSERT_EQ(nfs_retx_queue_push(&q, 10, &d, 1, 0.0), 0);
    ASSERT_EQ(nfs_retx_queue_push(&q, 20, &d, 1, 0.1), 0);
    /* Queue full */
    ASSERT_EQ(nfs_retx_queue_push(&q, 30, &d, 1, 0.2), -1);
    ASSERT_EQ(nfs_retx_queue_size(&q), 2);

    /* After removing one, push should work again */
    nfs_retx_queue_ack(&q, 15);
    ASSERT_EQ(nfs_retx_queue_size(&q), 1);
    ASSERT_EQ(nfs_retx_queue_push(&q, 30, &d, 1, 0.3), 0);
    ASSERT_EQ(nfs_retx_queue_size(&q), 2);

    nfs_retx_queue_free(&q);
}

/* ---- queue ordering preserved ---- */

static void test_queue_ordering(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 8);

    uint8_t d1 = 0xAA, d2 = 0xBB, d3 = 0xCC;
    nfs_retx_queue_push(&q, 1000, &d1, 1, 0.0);
    nfs_retx_queue_push(&q, 2000, &d2, 1, 0.1);
    nfs_retx_queue_push(&q, 3000, &d3, 1, 0.2);

    /* Remove middle via ACK */
    nfs_retx_queue_ack(&q, 2001);
    ASSERT_EQ(nfs_retx_queue_size(&q), 1);

    /* Only seq 3000 remains */
    struct nfs_retx_seg *seg = nfs_retx_queue_find(&q, 3000);
    ASSERT_TRUE(seg != NULL);
    ASSERT_EQ(seg->data[0], 0xCC);

    nfs_retx_queue_free(&q);
}

/* ---- data too large ---- */

static void test_data_too_large(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 4);

    uint8_t big[NFS_RETX_MSS + 1];
    memset(big, 0xFF, sizeof(big));
    ASSERT_EQ(nfs_retx_queue_push(&q, 0, big, sizeof(big), 0.0), -1);
    ASSERT_EQ(nfs_retx_queue_size(&q), 0);

    nfs_retx_queue_free(&q);
}

/* ---- push maximum MSS data ---- */

static void test_push_max_mss(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 2);

    uint8_t data[NFS_RETX_MSS];
    memset(data, 0x42, NFS_RETX_MSS);
    ASSERT_EQ(nfs_retx_queue_push(&q, 5000, data, NFS_RETX_MSS, 1.0), 0);

    struct nfs_retx_seg *seg = nfs_retx_queue_find(&q, 5000);
    ASSERT_TRUE(seg != NULL);
    ASSERT_EQ(seg->data_len, NFS_RETX_MSS);
    ASSERT_EQ(seg->data[0], 0x42);
    ASSERT_EQ(seg->data[NFS_RETX_MSS - 1], 0x42);

    nfs_retx_queue_free(&q);
}

/* ---- zero-length segment ---- */

static void test_zero_length_segment(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 4);

    ASSERT_EQ(nfs_retx_queue_push(&q, 7777, NULL, 0, 0.5), 0);
    struct nfs_retx_seg *seg = nfs_retx_queue_find(&q, 7777);
    ASSERT_TRUE(seg != NULL);
    ASSERT_EQ(seg->data_len, 0);

    nfs_retx_queue_free(&q);
}

/* ---- ack with no match removes nothing ---- */

static void test_ack_no_match(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 4);

    uint8_t d = 0;
    nfs_retx_queue_push(&q, 500, &d, 1, 0.0);

    /* ACK below all seqs */
    int removed = nfs_retx_queue_ack(&q, 100);
    ASSERT_EQ(removed, 0);
    ASSERT_EQ(nfs_retx_queue_size(&q), 1);

    nfs_retx_queue_free(&q);
}

/* ---- timeout with no timed-out segments ---- */

static void test_timeout_none(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 4);

    uint8_t d = 0;
    nfs_retx_queue_push(&q, 100, &d, 1, 5.0);

    /* now=5.5 with RTO=1.0 => age 0.5 < 1.0 */
    int count = nfs_retx_queue_timeout(&q, 5.5, 1.0);
    ASSERT_EQ(count, 0);

    struct nfs_retx_seg *seg = nfs_retx_queue_find(&q, 100);
    ASSERT_TRUE(seg != NULL);
    ASSERT_EQ(seg->retx_count, 0);

    nfs_retx_queue_free(&q);
}

/* ---- init and free empty queue ---- */

static void test_empty_queue(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 16);

    ASSERT_EQ(nfs_retx_queue_empty(&q), 1);
    ASSERT_EQ(nfs_retx_queue_size(&q), 0);
    ASSERT_TRUE(nfs_retx_queue_find(&q, 0) == NULL);
    ASSERT_EQ(nfs_retx_queue_ack(&q, 9999), 0);
    ASSERT_EQ(nfs_retx_queue_timeout(&q, 100.0, 1.0), 0);

    nfs_retx_queue_free(&q);
}

int main(void) {
    printf("Retransmission Queue Tests\n");

    test_push_and_find();
    test_find_missing();
    test_ack_removes_segments();
    test_partial_ack();
    test_timeout_detection();
    test_retx_count_increments();
    test_empty_after_full_ack();
    test_capacity_limits();
    test_queue_ordering();
    test_data_too_large();
    test_push_max_mss();
    test_zero_length_segment();
    test_ack_no_match();
    test_timeout_none();
    test_empty_queue();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
