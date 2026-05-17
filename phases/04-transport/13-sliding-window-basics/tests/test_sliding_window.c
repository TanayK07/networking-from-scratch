/* Tests for the send-side sliding window implementation.
 *
 * Test families:
 *   1. Initial state
 *   2. Send fills window
 *   3. Window full rejects more
 *   4. ACK advances base
 *   5. Partial ACK
 *   6. Duplicate ACK (no change)
 *   7. Available space after ACK
 *   8. Send after ACK frees space
 *   9. Get data for retransmission
 *  10. Full send-ACK cycle
 *  11. Edge: ACK beyond next_seq rejected
 *  12. Edge: ACK before base rejected
 */

#include "sliding_window.h"

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
 * Test 1: Initial state
 * --------------------------------------------------------------- */

static void test_init_state(void) {
    printf("  initial state...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 1000, 8, 64);

    ASSERT_EQ(w.base, 1000);
    ASSERT_EQ(w.next_seq, 1000);
    ASSERT_EQ(w.window_size, 8);
    ASSERT_EQ(nfs_send_window_in_flight(&w), 0);
    ASSERT_EQ(nfs_send_window_available(&w), 8);
    ASSERT_TRUE(nfs_send_window_can_send(&w));
    ASSERT_TRUE(w.buffer != NULL);
    ASSERT_TRUE(w.acked != NULL);

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: Send fills window
 * --------------------------------------------------------------- */

static void test_send_fills_window(void) {
    printf("  send fills window...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 0, 4, 64);

    /* Send exactly window_size bytes */
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(nfs_send_window_send(&w, (uint8_t)('A' + i)), 0);
    }

    ASSERT_EQ(nfs_send_window_in_flight(&w), 4);
    ASSERT_EQ(nfs_send_window_available(&w), 0);
    ASSERT_EQ(w.next_seq, 4);
    ASSERT_EQ(w.base, 0);

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: Window full rejects more sends
 * --------------------------------------------------------------- */

static void test_window_full_rejects(void) {
    printf("  window full rejects...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 100, 3, 64);

    ASSERT_EQ(nfs_send_window_send(&w, 'X'), 0);
    ASSERT_EQ(nfs_send_window_send(&w, 'Y'), 0);
    ASSERT_EQ(nfs_send_window_send(&w, 'Z'), 0);

    /* Window is full now */
    ASSERT_TRUE(!nfs_send_window_can_send(&w));
    ASSERT_EQ(nfs_send_window_send(&w, 'W'), -1);
    ASSERT_EQ(nfs_send_window_send(&w, 'V'), -1);

    /* next_seq should not have advanced */
    ASSERT_EQ(w.next_seq, 103);

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: ACK advances base
 * --------------------------------------------------------------- */

static void test_ack_advances_base(void) {
    printf("  ACK advances base...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 500, 8, 64);

    /* Send 5 bytes */
    for (int i = 0; i < 5; i++)
        nfs_send_window_send(&w, (uint8_t)i);

    ASSERT_EQ(w.base, 500);
    ASSERT_EQ(w.next_seq, 505);

    /* ACK all 5 */
    int newly = nfs_send_window_ack(&w, 505);
    ASSERT_EQ(newly, 5);
    ASSERT_EQ(w.base, 505);
    ASSERT_EQ(nfs_send_window_in_flight(&w), 0);
    ASSERT_EQ(nfs_send_window_available(&w), 8);

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: Partial ACK
 * --------------------------------------------------------------- */

static void test_partial_ack(void) {
    printf("  partial ACK...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 0, 8, 64);

    for (int i = 0; i < 6; i++)
        nfs_send_window_send(&w, (uint8_t)i);

    /* ACK first 3 */
    int newly = nfs_send_window_ack(&w, 3);
    ASSERT_EQ(newly, 3);
    ASSERT_EQ(w.base, 3);
    ASSERT_EQ(nfs_send_window_in_flight(&w), 3);
    ASSERT_EQ(nfs_send_window_available(&w), 5);

    /* ACK 2 more */
    newly = nfs_send_window_ack(&w, 5);
    ASSERT_EQ(newly, 2);
    ASSERT_EQ(w.base, 5);
    ASSERT_EQ(nfs_send_window_in_flight(&w), 1);

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: Duplicate ACK (no change)
 * --------------------------------------------------------------- */

static void test_duplicate_ack(void) {
    printf("  duplicate ACK...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 200, 8, 64);

    for (int i = 0; i < 4; i++)
        nfs_send_window_send(&w, (uint8_t)i);

    /* ACK first 2 */
    nfs_send_window_ack(&w, 202);
    ASSERT_EQ(w.base, 202);

    /* Duplicate ACK — same ack_num */
    int newly = nfs_send_window_ack(&w, 202);
    ASSERT_EQ(newly, 0);
    ASSERT_EQ(w.base, 202);                      /* unchanged */
    ASSERT_EQ(nfs_send_window_in_flight(&w), 2); /* unchanged */

    /* Another duplicate */
    newly = nfs_send_window_ack(&w, 202);
    ASSERT_EQ(newly, 0);

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: Available space after ACK
 * --------------------------------------------------------------- */

static void test_available_after_ack(void) {
    printf("  available after ACK...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 0, 4, 64);

    /* Fill window */
    for (int i = 0; i < 4; i++)
        nfs_send_window_send(&w, (uint8_t)i);

    ASSERT_EQ(nfs_send_window_available(&w), 0);
    ASSERT_TRUE(!nfs_send_window_can_send(&w));

    /* ACK 2 */
    nfs_send_window_ack(&w, 2);
    ASSERT_EQ(nfs_send_window_available(&w), 2);
    ASSERT_TRUE(nfs_send_window_can_send(&w));

    /* ACK all */
    nfs_send_window_ack(&w, 4);
    ASSERT_EQ(nfs_send_window_available(&w), 4);
    ASSERT_EQ(nfs_send_window_in_flight(&w), 0);

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: Send after ACK frees space
 * --------------------------------------------------------------- */

static void test_send_after_ack(void) {
    printf("  send after ACK frees space...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 0, 3, 64);

    /* Fill window */
    nfs_send_window_send(&w, 'A');
    nfs_send_window_send(&w, 'B');
    nfs_send_window_send(&w, 'C');
    ASSERT_EQ(nfs_send_window_send(&w, 'D'), -1);

    /* ACK first byte */
    nfs_send_window_ack(&w, 1);

    /* Now we can send one more */
    ASSERT_EQ(nfs_send_window_send(&w, 'D'), 0);
    ASSERT_EQ(w.next_seq, 4);
    ASSERT_EQ(nfs_send_window_in_flight(&w), 3);

    /* Still full */
    ASSERT_EQ(nfs_send_window_send(&w, 'E'), -1);

    /* ACK 2 more */
    nfs_send_window_ack(&w, 3);
    ASSERT_EQ(nfs_send_window_send(&w, 'E'), 0);
    ASSERT_EQ(nfs_send_window_send(&w, 'F'), 0);

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: Get data for retransmission
 * --------------------------------------------------------------- */

static void test_get_data(void) {
    printf("  get data for retransmit...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 10, 8, 64);

    nfs_send_window_send(&w, 'H');
    nfs_send_window_send(&w, 'E');
    nfs_send_window_send(&w, 'L');
    nfs_send_window_send(&w, 'P');

    size_t len;
    const uint8_t *data;

    /* Get from base */
    data = nfs_send_window_get_data(&w, 10, &len);
    ASSERT_TRUE(data != NULL);
    ASSERT_TRUE(len >= 4);
    ASSERT_EQ(data[0], 'H');
    ASSERT_EQ(data[1], 'E');
    ASSERT_EQ(data[2], 'L');
    ASSERT_EQ(data[3], 'P');

    /* Get from middle */
    data = nfs_send_window_get_data(&w, 12, &len);
    ASSERT_TRUE(data != NULL);
    ASSERT_TRUE(len >= 2);
    ASSERT_EQ(data[0], 'L');
    ASSERT_EQ(data[1], 'P');

    /* Out of range */
    data = nfs_send_window_get_data(&w, 9, &len);
    ASSERT_TRUE(data == NULL);
    ASSERT_EQ(len, 0);

    data = nfs_send_window_get_data(&w, 14, &len);
    ASSERT_TRUE(data == NULL);
    ASSERT_EQ(len, 0);

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: Full send-ACK cycle
 * --------------------------------------------------------------- */

static void test_full_cycle(void) {
    printf("  full send-ACK cycle...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 0, 4, 64);

    const char *msg = "ABCDEFGHIJ";
    size_t sent = 0;
    size_t total = strlen(msg);

    while (sent < total) {
        /* Send as much as window allows */
        while (sent < total && nfs_send_window_can_send(&w)) {
            nfs_send_window_send(&w, (uint8_t)msg[sent]);
            sent++;
        }

        /* ACK everything in-flight */
        nfs_send_window_ack(&w, w.next_seq);
    }

    ASSERT_EQ(w.base, 10);
    ASSERT_EQ(w.next_seq, 10);
    ASSERT_EQ(nfs_send_window_in_flight(&w), 0);
    ASSERT_EQ(nfs_send_window_available(&w), 4);

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 11: ACK beyond next_seq rejected
 * --------------------------------------------------------------- */

static void test_ack_beyond_next_seq(void) {
    printf("  ACK beyond next_seq rejected...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 50, 8, 64);

    nfs_send_window_send(&w, 'X');
    nfs_send_window_send(&w, 'Y');

    /* ACK for data we never sent */
    int rc = nfs_send_window_ack(&w, 53);
    ASSERT_EQ(rc, -1);
    ASSERT_EQ(w.base, 50); /* unchanged */

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 12: ACK before base rejected
 * --------------------------------------------------------------- */

static void test_ack_before_base(void) {
    printf("  ACK before base rejected...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 100, 8, 64);

    for (int i = 0; i < 4; i++)
        nfs_send_window_send(&w, (uint8_t)i);

    nfs_send_window_ack(&w, 102);
    ASSERT_EQ(w.base, 102);

    /* Old ACK below current base */
    int rc = nfs_send_window_ack(&w, 101);
    ASSERT_EQ(rc, -1);
    ASSERT_EQ(w.base, 102); /* unchanged */

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 13: Window size 1 (stop-and-wait)
 * --------------------------------------------------------------- */

static void test_window_size_one(void) {
    printf("  window size 1 (stop-and-wait)...");

    struct nfs_send_window w;
    nfs_send_window_init(&w, 0, 1, 64);

    ASSERT_EQ(nfs_send_window_send(&w, 'A'), 0);
    ASSERT_EQ(nfs_send_window_send(&w, 'B'), -1);

    nfs_send_window_ack(&w, 1);
    ASSERT_EQ(nfs_send_window_send(&w, 'B'), 0);
    ASSERT_EQ(nfs_send_window_send(&w, 'C'), -1);

    nfs_send_window_ack(&w, 2);
    ASSERT_EQ(w.base, 2);
    ASSERT_EQ(nfs_send_window_in_flight(&w), 0);

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 14: Buffer data integrity after wrap-around
 * --------------------------------------------------------------- */

static void test_buffer_wrap(void) {
    printf("  buffer data integrity...");

    struct nfs_send_window w;
    /* Small buffer to test wrapping logic */
    nfs_send_window_init(&w, 0, 4, 8);

    /* Send 4, ACK 4, send 4 more — some will wrap in the buffer */
    for (int i = 0; i < 4; i++)
        nfs_send_window_send(&w, (uint8_t)('A' + i));
    nfs_send_window_ack(&w, 4);

    for (int i = 0; i < 4; i++)
        nfs_send_window_send(&w, (uint8_t)('E' + i));

    /* Verify the second batch is readable */
    size_t len;
    const uint8_t *data = nfs_send_window_get_data(&w, 4, &len);
    ASSERT_TRUE(data != NULL);
    ASSERT_TRUE(len >= 1);
    ASSERT_EQ(data[0], 'E');

    nfs_send_window_free(&w);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    printf("Sliding window test suite\n");

    test_init_state();
    test_send_fills_window();
    test_window_full_rejects();
    test_ack_advances_base();
    test_partial_ack();
    test_duplicate_ack();
    test_available_after_ack();
    test_send_after_ack();
    test_get_data();
    test_full_cycle();
    test_ack_beyond_next_seq();
    test_ack_before_base();
    test_window_size_one();
    test_buffer_wrap();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
