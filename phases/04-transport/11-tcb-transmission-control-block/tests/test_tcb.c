/* Unit tests for nfs_tcb — Transmission Control Block. */

#include "../tcb.h"
#include <stdio.h>
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

/* ---- init tests ---- */

static void test_init_state(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 12345, 80, 0x0A000001, 0x0A000002);
    ASSERT_EQ(tcb.state, NFS_TCB_CLOSED);
    ASSERT_EQ(tcb.local_port, 12345);
    ASSERT_EQ(tcb.remote_port, 80);
    ASSERT_EQ(tcb.local_addr, 0x0A000001);
    ASSERT_EQ(tcb.remote_addr, 0x0A000002);
}

static void test_init_seq_zeroed(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    ASSERT_EQ(tcb.snd_una, 0);
    ASSERT_EQ(tcb.snd_nxt, 0);
    ASSERT_EQ(tcb.rcv_nxt, 0);
    ASSERT_EQ(tcb.iss, 0);
    ASSERT_EQ(tcb.irs, 0);
}

static void test_init_default_windows(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    ASSERT_EQ(tcb.snd_wnd, 65535);
    ASSERT_EQ(tcb.rcv_wnd, 65535);
}

/* ---- ISS / IRS tests ---- */

static void test_set_iss(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_iss(&tcb, 1000);
    ASSERT_EQ(tcb.iss, 1000);
    ASSERT_EQ(tcb.snd_una, 1000);
    ASSERT_EQ(tcb.snd_nxt, 1001);
}

static void test_set_iss_large(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_iss(&tcb, 0xFFFFFF00);
    ASSERT_EQ(tcb.iss, 0xFFFFFF00);
    ASSERT_EQ(tcb.snd_una, 0xFFFFFF00);
    ASSERT_EQ(tcb.snd_nxt, 0xFFFFFF01);
}

static void test_set_irs(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_irs(&tcb, 5000);
    ASSERT_EQ(tcb.irs, 5000);
    ASSERT_EQ(tcb.rcv_nxt, 5001);
}

static void test_set_irs_large(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_irs(&tcb, 0xFFFFFFFE);
    ASSERT_EQ(tcb.irs, 0xFFFFFFFE);
    ASSERT_EQ(tcb.rcv_nxt, 0xFFFFFFFF);
}

/* ---- send available tests ---- */

static void test_snd_available_full_window(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_iss(&tcb, 100);
    /* snd_nxt=101, snd_una=100, in_flight=1, wnd=65535 -> available=65534 */
    ASSERT_EQ(nfs_tcb_snd_available(&tcb), 65534);
}

static void test_snd_available_after_send(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_iss(&tcb, 100);
    nfs_tcb_advance_snd(&tcb, 1000);
    /* in_flight = 1001, wnd = 65535, available = 64534 */
    ASSERT_EQ(nfs_tcb_snd_available(&tcb), 64534);
}

static void test_snd_available_window_full(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    tcb.snd_wnd = 100;
    nfs_tcb_set_iss(&tcb, 0);
    /* in_flight = 1, wnd = 100, available = 99 */
    nfs_tcb_advance_snd(&tcb, 99);
    /* in_flight = 100, available = 0 */
    ASSERT_EQ(nfs_tcb_snd_available(&tcb), 0);
}

static void test_snd_available_zero_window(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    tcb.snd_wnd = 0;
    nfs_tcb_set_iss(&tcb, 0);
    ASSERT_EQ(nfs_tcb_snd_available(&tcb), 0);
}

/* ---- advance snd tests ---- */

static void test_advance_snd(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_iss(&tcb, 100);
    ASSERT_EQ(tcb.snd_nxt, 101);
    nfs_tcb_advance_snd(&tcb, 500);
    ASSERT_EQ(tcb.snd_nxt, 601);
}

static void test_advance_snd_multiple(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_iss(&tcb, 0);
    nfs_tcb_advance_snd(&tcb, 10);
    nfs_tcb_advance_snd(&tcb, 20);
    nfs_tcb_advance_snd(&tcb, 30);
    ASSERT_EQ(tcb.snd_nxt, 61); /* 1 (from ISS) + 10 + 20 + 30 */
}

/* ---- ack received tests ---- */

static void test_ack_received_advances(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_iss(&tcb, 100);
    nfs_tcb_advance_snd(&tcb, 500);
    nfs_tcb_ack_received(&tcb, 301);
    ASSERT_EQ(tcb.snd_una, 301);
}

static void test_ack_received_old_ack_ignored(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_iss(&tcb, 100);
    nfs_tcb_advance_snd(&tcb, 500);
    nfs_tcb_ack_received(&tcb, 301);
    nfs_tcb_ack_received(&tcb, 200); /* old ACK, should be ignored */
    ASSERT_EQ(tcb.snd_una, 301);
}

static void test_ack_received_exact(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_iss(&tcb, 100);
    nfs_tcb_ack_received(&tcb, 100); /* same as snd_una, no advance */
    ASSERT_EQ(tcb.snd_una, 100);
}

static void test_ack_frees_window(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    tcb.snd_wnd = 100;
    nfs_tcb_set_iss(&tcb, 0);
    nfs_tcb_advance_snd(&tcb, 99); /* in_flight=100, available=0 */
    ASSERT_EQ(nfs_tcb_snd_available(&tcb), 0);
    nfs_tcb_ack_received(&tcb, 50); /* free 50 bytes */
    ASSERT_EQ(nfs_tcb_snd_available(&tcb), 50);
}

/* ---- data received tests ---- */

static void test_data_received(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_irs(&tcb, 5000);
    ASSERT_EQ(tcb.rcv_nxt, 5001);
    nfs_tcb_data_received(&tcb, 100);
    ASSERT_EQ(tcb.rcv_nxt, 5101);
}

static void test_data_received_multiple(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_irs(&tcb, 0);
    nfs_tcb_data_received(&tcb, 50);
    nfs_tcb_data_received(&tcb, 75);
    ASSERT_EQ(tcb.rcv_nxt, 126); /* 1 + 50 + 75 */
}

/* ---- 4-tuple matching tests ---- */

static void test_matches_correct(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 12345, 80, 0x0A000001, 0x0A000002);
    /* Packet from remote (src) to local (dst) */
    ASSERT_EQ(nfs_tcb_matches(&tcb, 0x0A000002, 80, 0x0A000001, 12345), 1);
}

static void test_matches_wrong_src_addr(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 12345, 80, 0x0A000001, 0x0A000002);
    ASSERT_EQ(nfs_tcb_matches(&tcb, 0x0A000003, 80, 0x0A000001, 12345), 0);
}

static void test_matches_wrong_src_port(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 12345, 80, 0x0A000001, 0x0A000002);
    ASSERT_EQ(nfs_tcb_matches(&tcb, 0x0A000002, 81, 0x0A000001, 12345), 0);
}

static void test_matches_wrong_dst_addr(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 12345, 80, 0x0A000001, 0x0A000002);
    ASSERT_EQ(nfs_tcb_matches(&tcb, 0x0A000002, 80, 0x0A000099, 12345), 0);
}

static void test_matches_wrong_dst_port(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 12345, 80, 0x0A000001, 0x0A000002);
    ASSERT_EQ(nfs_tcb_matches(&tcb, 0x0A000002, 80, 0x0A000001, 9999), 0);
}

/* ---- format tests ---- */

static void test_format_contains_ports(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 12345, 80, 0x0A000001, 0x0A000002);
    nfs_tcb_set_iss(&tcb, 1000);
    char buf[256];
    nfs_tcb_format(&tcb, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "12345") != NULL);
    ASSERT_TRUE(strstr(buf, "80") != NULL);
}

static void test_format_contains_addrs(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 12345, 80, 0x0A000001, 0x0A000002);
    char buf[256];
    nfs_tcb_format(&tcb, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "10.0.0.1") != NULL);
    ASSERT_TRUE(strstr(buf, "10.0.0.2") != NULL);
}

static void test_format_contains_seq(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 12345, 80, 0x0A000001, 0x0A000002);
    nfs_tcb_set_iss(&tcb, 1000);
    nfs_tcb_set_irs(&tcb, 5000);
    char buf[256];
    nfs_tcb_format(&tcb, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "snd_una=1000") != NULL);
    ASSERT_TRUE(strstr(buf, "snd_nxt=1001") != NULL);
    ASSERT_TRUE(strstr(buf, "rcv_nxt=5001") != NULL);
}

/* ---- full handshake scenario ---- */

static void test_full_handshake_scenario(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 5000, 80, 0xC0A80001, 0xC0A80002);

    /* SYN: set ISS */
    nfs_tcb_set_iss(&tcb, 100);
    ASSERT_EQ(tcb.snd_una, 100);
    ASSERT_EQ(tcb.snd_nxt, 101);

    /* SYN-ACK: set IRS and process ACK */
    nfs_tcb_set_irs(&tcb, 300);
    nfs_tcb_ack_received(&tcb, 101);
    ASSERT_EQ(tcb.snd_una, 101);
    ASSERT_EQ(tcb.rcv_nxt, 301);

    /* Send 1000 bytes */
    nfs_tcb_advance_snd(&tcb, 1000);
    ASSERT_EQ(tcb.snd_nxt, 1101);

    /* Partial ACK for 500 bytes */
    nfs_tcb_ack_received(&tcb, 601);
    ASSERT_EQ(tcb.snd_una, 601);

    /* Receive 200 bytes */
    nfs_tcb_data_received(&tcb, 200);
    ASSERT_EQ(tcb.rcv_nxt, 501);

    /* Full ACK */
    nfs_tcb_ack_received(&tcb, 1101);
    ASSERT_EQ(tcb.snd_una, 1101);
    ASSERT_EQ(nfs_tcb_snd_available(&tcb), 65535);
}

/* ---- edge cases ---- */

static void test_format_small_buffer(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 80, 80, 0, 0);
    char buf[10];
    nfs_tcb_format(&tcb, buf, sizeof(buf));
    /* Should not crash; output is truncated */
    ASSERT_TRUE(1);
}

static void test_iss_wraparound(void) {
    struct nfs_tcb tcb;
    nfs_tcb_init(&tcb, 1000, 2000, 0, 0);
    nfs_tcb_set_iss(&tcb, 0xFFFFFFFF);
    ASSERT_EQ(tcb.snd_una, 0xFFFFFFFF);
    ASSERT_EQ(tcb.snd_nxt, 0x00000000); /* wraps around */
}

int main(void) {
    printf("TCB Tests\n");

    test_init_state();
    test_init_seq_zeroed();
    test_init_default_windows();
    test_set_iss();
    test_set_iss_large();
    test_set_irs();
    test_set_irs_large();
    test_snd_available_full_window();
    test_snd_available_after_send();
    test_snd_available_window_full();
    test_snd_available_zero_window();
    test_advance_snd();
    test_advance_snd_multiple();
    test_ack_received_advances();
    test_ack_received_old_ack_ignored();
    test_ack_received_exact();
    test_ack_frees_window();
    test_data_received();
    test_data_received_multiple();
    test_matches_correct();
    test_matches_wrong_src_addr();
    test_matches_wrong_src_port();
    test_matches_wrong_dst_addr();
    test_matches_wrong_dst_port();
    test_format_contains_ports();
    test_format_contains_addrs();
    test_format_contains_seq();
    test_full_handshake_scenario();
    test_format_small_buffer();
    test_iss_wraparound();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
