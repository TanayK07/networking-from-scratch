/* Unit tests for receive window and zero-window probes. */

#include "../recv_win.h"
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

/* ---- initial advertised window == capacity ---- */

static void test_initial_advertised(void) {
    struct nfs_recv_window w;
    nfs_recv_window_init(&w, 65535);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 65535);
    ASSERT_EQ(nfs_recv_window_is_zero(&w), 0);
    nfs_recv_window_free(&w);
}

/* ---- receive fills buffer and shrinks window ---- */

static void test_receive_fills_buffer(void) {
    struct nfs_recv_window w;
    nfs_recv_window_init(&w, 100);

    uint8_t data[40];
    memset(data, 0xAA, 40);
    int accepted = nfs_recv_window_receive(&w, 0, data, 40);
    ASSERT_EQ(accepted, 40);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 60);
    ASSERT_EQ(w.rcv_nxt, 40);

    nfs_recv_window_free(&w);
}

/* ---- advertised shrinks correctly ---- */

static void test_advertised_shrinks(void) {
    struct nfs_recv_window w;
    nfs_recv_window_init(&w, 50);

    uint8_t d[10];
    memset(d, 1, 10);

    nfs_recv_window_receive(&w, 0, d, 10);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 40);

    nfs_recv_window_receive(&w, 10, d, 10);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 30);

    nfs_recv_window_receive(&w, 20, d, 10);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 20);

    nfs_recv_window_receive(&w, 30, d, 10);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 10);

    nfs_recv_window_receive(&w, 40, d, 10);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 0);

    nfs_recv_window_free(&w);
}

/* ---- read frees space ---- */

static void test_read_frees_space(void) {
    struct nfs_recv_window w;
    nfs_recv_window_init(&w, 20);

    uint8_t data[20];
    memset(data, 0x42, 20);
    nfs_recv_window_receive(&w, 0, data, 20);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 0);
    ASSERT_EQ(nfs_recv_window_is_zero(&w), 1);

    /* App reads 10 bytes */
    uint8_t out[10];
    size_t nr = nfs_recv_window_read(&w, out, 10);
    ASSERT_EQ(nr, 10);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 10);
    ASSERT_EQ(nfs_recv_window_is_zero(&w), 0);
    ASSERT_EQ(out[0], 0x42);

    nfs_recv_window_free(&w);
}

/* ---- zero window when full ---- */

static void test_zero_window_when_full(void) {
    struct nfs_recv_window w;
    nfs_recv_window_init(&w, 8);

    uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    nfs_recv_window_receive(&w, 0, data, 8);
    ASSERT_EQ(nfs_recv_window_is_zero(&w), 1);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 0);

    nfs_recv_window_free(&w);
}

/* ---- reject data when full ---- */

static void test_reject_when_full(void) {
    struct nfs_recv_window w;
    nfs_recv_window_init(&w, 4);

    uint8_t data[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    nfs_recv_window_receive(&w, 0, data, 4);
    ASSERT_EQ(nfs_recv_window_is_zero(&w), 1);

    /* Attempt to receive more -- zero space */
    uint8_t more = 0xFF;
    int accepted = nfs_recv_window_receive(&w, 4, &more, 1);
    ASSERT_EQ(accepted, 0);

    nfs_recv_window_free(&w);
}

/* ---- wrong seq rejected ---- */

static void test_wrong_seq_rejected(void) {
    struct nfs_recv_window w;
    nfs_recv_window_init(&w, 100);

    uint8_t data[5] = {1, 2, 3, 4, 5};

    /* Expected seq is 0, sending seq 10 */
    int accepted = nfs_recv_window_receive(&w, 10, data, 5);
    ASSERT_EQ(accepted, -1);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 100);

    /* Correct seq */
    accepted = nfs_recv_window_receive(&w, 0, data, 5);
    ASSERT_EQ(accepted, 5);

    /* Now expected is 5, but send 0 again */
    accepted = nfs_recv_window_receive(&w, 0, data, 5);
    ASSERT_EQ(accepted, -1);

    nfs_recv_window_free(&w);
}

/* ---- partial receive when buffer nearly full ---- */

static void test_partial_receive(void) {
    struct nfs_recv_window w;
    nfs_recv_window_init(&w, 10);

    uint8_t data[7];
    memset(data, 0x11, 7);
    nfs_recv_window_receive(&w, 0, data, 7);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 3);

    /* Try to put 5 bytes, only 3 fit */
    uint8_t more[5] = {0x22, 0x22, 0x22, 0x22, 0x22};
    int accepted = nfs_recv_window_receive(&w, 7, more, 5);
    ASSERT_EQ(accepted, 3);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 0);
    ASSERT_EQ(w.rcv_nxt, 10);

    nfs_recv_window_free(&w);
}

/* ---- read data integrity ---- */

static void test_read_data_integrity(void) {
    struct nfs_recv_window w;
    nfs_recv_window_init(&w, 32);

    uint8_t data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; /* "Hello" */
    nfs_recv_window_receive(&w, 0, data, 5);

    uint8_t out[8];
    size_t nr = nfs_recv_window_read(&w, out, 8);
    ASSERT_EQ(nr, 5);
    ASSERT_EQ(out[0], 0x48);
    ASSERT_EQ(out[1], 0x65);
    ASSERT_EQ(out[2], 0x6C);
    ASSERT_EQ(out[3], 0x6C);
    ASSERT_EQ(out[4], 0x6F);

    nfs_recv_window_free(&w);
}

/* ---- read from empty buffer ---- */

static void test_read_empty(void) {
    struct nfs_recv_window w;
    nfs_recv_window_init(&w, 16);

    uint8_t out[8];
    size_t nr = nfs_recv_window_read(&w, out, 8);
    ASSERT_EQ(nr, 0);

    nfs_recv_window_free(&w);
}

/* ---- ZWP: timer fires at interval ---- */

static void test_zwp_fires_at_interval(void) {
    struct nfs_zwp z;
    nfs_zwp_init(&z, 1.0);

    nfs_zwp_start(&z, 0.0);
    ASSERT_EQ(z.active, 1);

    /* Before interval */
    ASSERT_EQ(nfs_zwp_check(&z, 0.5), 0);
    ASSERT_EQ(nfs_zwp_check(&z, 0.99), 0);

    /* At interval */
    ASSERT_EQ(nfs_zwp_check(&z, 1.0), 1);
    ASSERT_EQ(z.probe_count, 1);

    /* After first probe, timer resets */
    ASSERT_EQ(nfs_zwp_check(&z, 1.5), 0);
    ASSERT_EQ(nfs_zwp_check(&z, 2.0), 1);
    ASSERT_EQ(z.probe_count, 2);
}

/* ---- ZWP: stop clears probing ---- */

static void test_zwp_stop(void) {
    struct nfs_zwp z;
    nfs_zwp_init(&z, 0.5);

    nfs_zwp_start(&z, 0.0);
    nfs_zwp_check(&z, 0.5);
    ASSERT_EQ(z.probe_count, 1);
    ASSERT_EQ(z.active, 1);

    nfs_zwp_stop(&z);
    ASSERT_EQ(z.active, 0);
    ASSERT_EQ(z.probe_count, 0);

    /* Should not fire after stop */
    ASSERT_EQ(nfs_zwp_check(&z, 10.0), 0);
}

/* ---- ZWP: not active initially ---- */

static void test_zwp_not_active(void) {
    struct nfs_zwp z;
    nfs_zwp_init(&z, 1.0);

    ASSERT_EQ(z.active, 0);
    ASSERT_EQ(nfs_zwp_check(&z, 100.0), 0);
}

/* ---- ZWP: multiple probes ---- */

static void test_zwp_multiple_probes(void) {
    struct nfs_zwp z;
    nfs_zwp_init(&z, 2.0);

    nfs_zwp_start(&z, 0.0);

    ASSERT_EQ(nfs_zwp_check(&z, 2.0), 1);
    ASSERT_EQ(z.probe_count, 1);

    ASSERT_EQ(nfs_zwp_check(&z, 4.0), 1);
    ASSERT_EQ(z.probe_count, 2);

    ASSERT_EQ(nfs_zwp_check(&z, 6.0), 1);
    ASSERT_EQ(z.probe_count, 3);
}

/* ---- receive and read cycle ---- */

static void test_receive_read_cycle(void) {
    struct nfs_recv_window w;
    nfs_recv_window_init(&w, 8);

    /* Fill completely */
    uint8_t d1[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    nfs_recv_window_receive(&w, 0, d1, 8);
    ASSERT_EQ(nfs_recv_window_is_zero(&w), 1);

    /* Read half */
    uint8_t out[4];
    nfs_recv_window_read(&w, out, 4);
    ASSERT_EQ(nfs_recv_window_advertised(&w), 4);

    /* Write 4 more */
    uint8_t d2[4] = {9, 10, 11, 12};
    int acc = nfs_recv_window_receive(&w, 8, d2, 4);
    ASSERT_EQ(acc, 4);
    ASSERT_EQ(nfs_recv_window_is_zero(&w), 1);

    /* Read everything */
    uint8_t all[8];
    size_t nr = nfs_recv_window_read(&w, all, 8);
    ASSERT_EQ(nr, 8);
    ASSERT_EQ(all[0], 5); /* remainder from first batch */
    ASSERT_EQ(all[3], 8);
    ASSERT_EQ(all[4], 9); /* second batch */
    ASSERT_EQ(all[7], 12);

    nfs_recv_window_free(&w);
}

int main(void) {
    printf("Receive Window & Zero-Window Probe Tests\n");

    test_initial_advertised();
    test_receive_fills_buffer();
    test_advertised_shrinks();
    test_read_frees_space();
    test_zero_window_when_full();
    test_reject_when_full();
    test_wrong_seq_rejected();
    test_partial_receive();
    test_read_data_integrity();
    test_read_empty();
    test_zwp_fires_at_interval();
    test_zwp_stop();
    test_zwp_not_active();
    test_zwp_multiple_probes();
    test_receive_read_cycle();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
