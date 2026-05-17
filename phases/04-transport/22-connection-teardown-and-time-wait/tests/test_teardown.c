/* Tests for TCP connection teardown FSM and TIME_WAIT.
 *
 * Test families:
 *   1.  Init state
 *   2.  Active close: ESTABLISHED -> FIN_WAIT_1 -> FIN_WAIT_2 -> TIME_WAIT -> CLOSED
 *   3.  Passive close: ESTABLISHED -> CLOSE_WAIT -> LAST_ACK -> CLOSED
 *   4.  Simultaneous close: FIN_WAIT_1 -> CLOSING -> TIME_WAIT -> CLOSED
 *   5.  TIME_WAIT 2*MSL timer not expired
 *   6.  TIME_WAIT 2*MSL timer expired
 *   7.  Invalid transitions rejected
 *   8.  State names
 *   9.  FIN consumes sequence number
 *  10.  TIME_WAIT with different MSL values
 *  11.  Double close rejected
 *  12.  TIME_WAIT check on non-TIME_WAIT state
 */

#include "teardown.h"

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

    struct nfs_tcp_conn c;
    nfs_conn_init(&c, 1000, 2000, 30.0);

    ASSERT_EQ(c.state, NFS_TCP_ST_ESTABLISHED);
    ASSERT_EQ(c.local_seq, 1000);
    ASSERT_EQ(c.remote_seq, 2000);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: Active close full sequence
 * --------------------------------------------------------------- */

static void test_active_close(void) {
    printf("  active close sequence...");

    struct nfs_tcp_conn c;
    nfs_conn_init(&c, 1000, 2000, 30.0);

    /* ESTABLISHED -> FIN_WAIT_1 */
    ASSERT_EQ(nfs_conn_close(&c), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_FIN_WAIT_1);

    /* FIN_WAIT_1 -> FIN_WAIT_2 (ACK for our FIN) */
    ASSERT_EQ(nfs_conn_recv_ack(&c), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_FIN_WAIT_2);

    /* FIN_WAIT_2 -> TIME_WAIT (peer's FIN) */
    ASSERT_EQ(nfs_conn_recv_fin(&c), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_TIME_WAIT);

    /* TIME_WAIT -> CLOSED (after 2*MSL) */
    c.time_wait_start = 100.0;
    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 160.0 + 0.1), 1);
    ASSERT_EQ(c.state, NFS_TCP_ST_CLOSED);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: Passive close full sequence
 * --------------------------------------------------------------- */

static void test_passive_close(void) {
    printf("  passive close sequence...");

    struct nfs_tcp_conn c;
    nfs_conn_init(&c, 5000, 6000, 30.0);

    /* ESTABLISHED -> CLOSE_WAIT (receive FIN) */
    ASSERT_EQ(nfs_conn_recv_fin(&c), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_CLOSE_WAIT);

    /* CLOSE_WAIT -> LAST_ACK (application closes) */
    ASSERT_EQ(nfs_conn_close(&c), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_LAST_ACK);

    /* LAST_ACK -> CLOSED (our FIN ACKed) */
    ASSERT_EQ(nfs_conn_recv_ack(&c), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_CLOSED);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: Simultaneous close
 * --------------------------------------------------------------- */

static void test_simultaneous_close(void) {
    printf("  simultaneous close...");

    struct nfs_tcp_conn c;
    nfs_conn_init(&c, 8000, 9000, 60.0);

    /* ESTABLISHED -> FIN_WAIT_1 (we close) */
    ASSERT_EQ(nfs_conn_close(&c), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_FIN_WAIT_1);

    /* FIN_WAIT_1 -> CLOSING (peer also sent FIN before seeing ours) */
    ASSERT_EQ(nfs_conn_recv_fin(&c), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_CLOSING);

    /* CLOSING -> TIME_WAIT (our FIN ACKed) */
    ASSERT_EQ(nfs_conn_recv_ack(&c), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_TIME_WAIT);

    /* TIME_WAIT -> CLOSED */
    c.time_wait_start = 0.0;
    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 120.1), 1);
    ASSERT_EQ(c.state, NFS_TCP_ST_CLOSED);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: TIME_WAIT not expired
 * --------------------------------------------------------------- */

static void test_time_wait_not_expired(void) {
    printf("  TIME_WAIT not expired...");

    struct nfs_tcp_conn c;
    nfs_conn_init(&c, 1000, 2000, 30.0);

    nfs_conn_close(&c);
    nfs_conn_recv_ack(&c);
    nfs_conn_recv_fin(&c);
    ASSERT_EQ(c.state, NFS_TCP_ST_TIME_WAIT);

    c.time_wait_start = 100.0;

    /* 2*MSL = 60s. At t=130 only 30s have passed. */
    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 130.0), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_TIME_WAIT);

    /* At t=159.9 still not expired */
    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 159.9), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_TIME_WAIT);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: TIME_WAIT expired exactly at boundary
 * --------------------------------------------------------------- */

static void test_time_wait_exact_boundary(void) {
    printf("  TIME_WAIT exact boundary...");

    struct nfs_tcp_conn c;
    nfs_conn_init(&c, 1000, 2000, 30.0);

    nfs_conn_close(&c);
    nfs_conn_recv_ack(&c);
    nfs_conn_recv_fin(&c);
    c.time_wait_start = 0.0;

    /* Exactly at 2*MSL = 60.0 should expire */
    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 60.0), 1);
    ASSERT_EQ(c.state, NFS_TCP_ST_CLOSED);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: Invalid transitions rejected
 * --------------------------------------------------------------- */

static void test_invalid_transitions(void) {
    printf("  invalid transitions rejected...");

    struct nfs_tcp_conn c;

    /* Can't recv_ack in ESTABLISHED */
    nfs_conn_init(&c, 1, 2, 30.0);
    ASSERT_EQ(nfs_conn_recv_ack(&c), -1);

    /* Can't close in FIN_WAIT_1 */
    nfs_conn_init(&c, 1, 2, 30.0);
    nfs_conn_close(&c);
    ASSERT_EQ(nfs_conn_close(&c), -1);

    /* Can't close in FIN_WAIT_2 */
    nfs_conn_init(&c, 1, 2, 30.0);
    nfs_conn_close(&c);
    nfs_conn_recv_ack(&c);
    ASSERT_EQ(nfs_conn_close(&c), -1);

    /* Can't recv_fin in TIME_WAIT */
    nfs_conn_init(&c, 1, 2, 30.0);
    nfs_conn_close(&c);
    nfs_conn_recv_ack(&c);
    nfs_conn_recv_fin(&c);
    ASSERT_EQ(c.state, NFS_TCP_ST_TIME_WAIT);
    ASSERT_EQ(nfs_conn_recv_fin(&c), -1);

    /* Can't close in CLOSED */
    nfs_conn_init(&c, 1, 2, 30.0);
    nfs_conn_close(&c);
    nfs_conn_recv_ack(&c);
    nfs_conn_recv_fin(&c);
    c.time_wait_start = 0.0;
    nfs_conn_time_wait_expired(&c, 100.0);
    ASSERT_EQ(c.state, NFS_TCP_ST_CLOSED);
    ASSERT_EQ(nfs_conn_close(&c), -1);

    /* Can't recv_fin in CLOSE_WAIT */
    nfs_conn_init(&c, 1, 2, 30.0);
    nfs_conn_recv_fin(&c);
    ASSERT_EQ(c.state, NFS_TCP_ST_CLOSE_WAIT);
    ASSERT_EQ(nfs_conn_recv_fin(&c), -1);

    /* Can't recv_ack in CLOSE_WAIT */
    nfs_conn_init(&c, 1, 2, 30.0);
    nfs_conn_recv_fin(&c);
    ASSERT_EQ(nfs_conn_recv_ack(&c), -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: State names
 * --------------------------------------------------------------- */

static void test_state_names(void) {
    printf("  state names...");

    ASSERT_TRUE(strcmp(nfs_conn_state_name(NFS_TCP_ST_ESTABLISHED), "ESTABLISHED") == 0);
    ASSERT_TRUE(strcmp(nfs_conn_state_name(NFS_TCP_ST_FIN_WAIT_1), "FIN_WAIT_1") == 0);
    ASSERT_TRUE(strcmp(nfs_conn_state_name(NFS_TCP_ST_FIN_WAIT_2), "FIN_WAIT_2") == 0);
    ASSERT_TRUE(strcmp(nfs_conn_state_name(NFS_TCP_ST_CLOSING), "CLOSING") == 0);
    ASSERT_TRUE(strcmp(nfs_conn_state_name(NFS_TCP_ST_TIME_WAIT), "TIME_WAIT") == 0);
    ASSERT_TRUE(strcmp(nfs_conn_state_name(NFS_TCP_ST_CLOSE_WAIT), "CLOSE_WAIT") == 0);
    ASSERT_TRUE(strcmp(nfs_conn_state_name(NFS_TCP_ST_LAST_ACK), "LAST_ACK") == 0);
    ASSERT_TRUE(strcmp(nfs_conn_state_name(NFS_TCP_ST_CLOSED), "CLOSED") == 0);
    ASSERT_TRUE(strcmp(nfs_conn_state_name(99), "UNKNOWN") == 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: FIN consumes sequence number
 * --------------------------------------------------------------- */

static void test_fin_consumes_seq(void) {
    printf("  FIN consumes sequence number...");

    struct nfs_tcp_conn c;
    nfs_conn_init(&c, 1000, 2000, 30.0);

    /* Our close: local_seq should increment (FIN consumes 1 seq) */
    uint32_t before = c.local_seq;
    nfs_conn_close(&c);
    ASSERT_EQ(c.local_seq, before + 1);

    /* Receive peer's FIN: remote_seq should increment */
    nfs_conn_recv_ack(&c); /* FIN_WAIT_1 -> FIN_WAIT_2 */
    uint32_t remote_before = c.remote_seq;
    nfs_conn_recv_fin(&c); /* FIN_WAIT_2 -> TIME_WAIT */
    ASSERT_EQ(c.remote_seq, remote_before + 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: TIME_WAIT with different MSL values
 * --------------------------------------------------------------- */

static void test_different_msl(void) {
    printf("  TIME_WAIT with different MSL...");

    /* MSL = 120s (common real-world value) */
    struct nfs_tcp_conn c;
    nfs_conn_init(&c, 1, 2, 120.0);
    nfs_conn_close(&c);
    nfs_conn_recv_ack(&c);
    nfs_conn_recv_fin(&c);
    c.time_wait_start = 0.0;

    /* 2*MSL = 240s. Not expired at 200s. */
    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 200.0), 0);
    /* Expired at 240s */
    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 240.0), 1);
    ASSERT_EQ(c.state, NFS_TCP_ST_CLOSED);

    /* MSL = 1s (for fast testing) */
    nfs_conn_init(&c, 1, 2, 1.0);
    nfs_conn_close(&c);
    nfs_conn_recv_ack(&c);
    nfs_conn_recv_fin(&c);
    c.time_wait_start = 10.0;

    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 11.5), 0);
    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 12.0), 1);
    ASSERT_EQ(c.state, NFS_TCP_ST_CLOSED);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 11: Double close rejected
 * --------------------------------------------------------------- */

static void test_double_close(void) {
    printf("  double close rejected...");

    struct nfs_tcp_conn c;
    nfs_conn_init(&c, 1000, 2000, 30.0);

    ASSERT_EQ(nfs_conn_close(&c), 0);
    ASSERT_EQ(c.state, NFS_TCP_ST_FIN_WAIT_1);

    /* Second close should fail */
    ASSERT_EQ(nfs_conn_close(&c), -1);
    ASSERT_EQ(c.state, NFS_TCP_ST_FIN_WAIT_1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 12: TIME_WAIT check on non-TIME_WAIT state
 * --------------------------------------------------------------- */

static void test_time_wait_wrong_state(void) {
    printf("  time_wait check on wrong state...");

    struct nfs_tcp_conn c;
    nfs_conn_init(&c, 1, 2, 30.0);

    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 100.0), -1);

    nfs_conn_close(&c);
    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 100.0), -1);

    nfs_conn_recv_ack(&c);
    ASSERT_EQ(nfs_conn_time_wait_expired(&c, 100.0), -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 13: Passive close with seq tracking
 * --------------------------------------------------------------- */

static void test_passive_close_seq(void) {
    printf("  passive close seq tracking...");

    struct nfs_tcp_conn c;
    nfs_conn_init(&c, 5000, 6000, 30.0);

    /* Receive FIN: remote_seq advances */
    nfs_conn_recv_fin(&c);
    ASSERT_EQ(c.remote_seq, 6001);
    ASSERT_EQ(c.local_seq, 5000); /* unchanged — we haven't sent FIN yet */

    /* Close: local_seq advances */
    nfs_conn_close(&c);
    ASSERT_EQ(c.local_seq, 5001);

    /* ACK completes */
    nfs_conn_recv_ack(&c);
    ASSERT_EQ(c.state, NFS_TCP_ST_CLOSED);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    printf("TCP teardown and TIME_WAIT test suite\n");

    test_init();
    test_active_close();
    test_passive_close();
    test_simultaneous_close();
    test_time_wait_not_expired();
    test_time_wait_exact_boundary();
    test_invalid_transitions();
    test_state_names();
    test_fin_consumes_seq();
    test_different_msl();
    test_double_close();
    test_time_wait_wrong_state();
    test_passive_close_seq();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
