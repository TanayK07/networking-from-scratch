/* Tests for TCP Finite State Machine (RFC 9293).
 *
 * Test families:
 *   1.  Init to CLOSED
 *   2.  Client-side 3WHS: CLOSED -> SYN_SENT -> ESTABLISHED
 *   3.  Server-side 3WHS: CLOSED -> LISTEN -> SYN_RCVD -> ESTABLISHED
 *   4.  Client graceful close: ESTABLISHED -> FIN_WAIT_1 -> FIN_WAIT_2 -> TIME_WAIT -> CLOSED
 *   5.  Server graceful close: ESTABLISHED -> CLOSE_WAIT -> LAST_ACK -> CLOSED
 *   6.  Simultaneous close: ESTABLISHED -> FIN_WAIT_1 -> CLOSING -> TIME_WAIT -> CLOSED
 *   7.  FIN_WAIT_1 -> TIME_WAIT (FIN+ACK received)
 *   8.  Invalid transitions return -1
 *   9.  State names
 *  10.  Event names
 *  11.  Action names
 *  12.  Close from LISTEN
 *  13.  Close from SYN_SENT
 *  14.  Close from SYN_RCVD
 *  15.  Simultaneous open
 */

#include "../tcp_fsm.h"

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

/* ---------------------------------------------------------------
 * Test 1: init to CLOSED
 * --------------------------------------------------------------- */
static void test_init(void) {
    printf("  init...");

    struct nfs_tcp_fsm fsm;
    nfs_tcp_fsm_init(&fsm);
    ASSERT_EQ(fsm.state, NFS_TCP_CLOSED);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: client-side 3WHS
 *   CLOSED -> SYN_SENT (send SYN)
 *   SYN_SENT -> ESTABLISHED (recv SYN+ACK, send ACK)
 * --------------------------------------------------------------- */
static void test_client_3whs(void) {
    printf("  client 3WHS...");

    struct nfs_tcp_fsm fsm;
    int action;

    nfs_tcp_fsm_init(&fsm);
    ASSERT_EQ(fsm.state, NFS_TCP_CLOSED);

    /* Active open: send SYN. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_ACTIVE, &action), NFS_TCP_SYN_SENT);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_SYN);
    ASSERT_EQ(fsm.state, NFS_TCP_SYN_SENT);

    /* Receive SYN-ACK: send ACK. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_SYNACK, &action), NFS_TCP_ESTABLISHED);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_ACK);
    ASSERT_EQ(fsm.state, NFS_TCP_ESTABLISHED);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: server-side 3WHS
 *   CLOSED -> LISTEN (passive open)
 *   LISTEN -> SYN_RCVD (recv SYN, send SYN+ACK)
 *   SYN_RCVD -> ESTABLISHED (recv ACK)
 * --------------------------------------------------------------- */
static void test_server_3whs(void) {
    printf("  server 3WHS...");

    struct nfs_tcp_fsm fsm;
    int action;

    nfs_tcp_fsm_init(&fsm);

    /* Passive open. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_PASSIVE, &action), NFS_TCP_LISTEN);
    ASSERT_EQ(action, NFS_TCP_ACT_NONE);

    /* Receive SYN: send SYN,ACK. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_SYN, &action), NFS_TCP_SYN_RCVD);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_SYNACK);

    /* Receive ACK: established. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_ACK, &action), NFS_TCP_ESTABLISHED);
    ASSERT_EQ(action, NFS_TCP_ACT_NONE);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: client graceful close
 *   ESTABLISHED -> FIN_WAIT_1 (close, send FIN)
 *   FIN_WAIT_1 -> FIN_WAIT_2 (recv ACK)
 *   FIN_WAIT_2 -> TIME_WAIT (recv FIN, send ACK)
 *   TIME_WAIT -> CLOSED (timeout, delete TCB)
 * --------------------------------------------------------------- */
static void test_client_close(void) {
    printf("  client graceful close...");

    struct nfs_tcp_fsm fsm;
    int action;

    /* Get to ESTABLISHED first. */
    nfs_tcp_fsm_init(&fsm);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_ACTIVE, &action);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_SYNACK, &action);
    ASSERT_EQ(fsm.state, NFS_TCP_ESTABLISHED);

    /* Close: send FIN. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_CLOSE, &action), NFS_TCP_FIN_WAIT_1);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_FIN);

    /* Receive ACK of our FIN. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_ACK, &action), NFS_TCP_FIN_WAIT_2);
    ASSERT_EQ(action, NFS_TCP_ACT_NONE);

    /* Receive FIN from peer: send ACK. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_FIN, &action), NFS_TCP_TIME_WAIT);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_ACK);

    /* 2MSL timeout: delete TCB. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_TIMEOUT, &action), NFS_TCP_CLOSED);
    ASSERT_EQ(action, NFS_TCP_ACT_DELETE_TCB);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: server graceful close
 *   ESTABLISHED -> CLOSE_WAIT (recv FIN, send ACK)
 *   CLOSE_WAIT -> LAST_ACK (close, send FIN)
 *   LAST_ACK -> CLOSED (recv ACK, delete TCB)
 * --------------------------------------------------------------- */
static void test_server_close(void) {
    printf("  server graceful close...");

    struct nfs_tcp_fsm fsm;
    int action;

    /* Get to ESTABLISHED (server side). */
    nfs_tcp_fsm_init(&fsm);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_PASSIVE, &action);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_SYN, &action);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_ACK, &action);
    ASSERT_EQ(fsm.state, NFS_TCP_ESTABLISHED);

    /* Receive FIN from client: send ACK. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_FIN, &action), NFS_TCP_CLOSE_WAIT);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_ACK);

    /* Application closes: send FIN. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_CLOSE, &action), NFS_TCP_LAST_ACK);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_FIN);

    /* Receive ACK: delete TCB. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_ACK, &action), NFS_TCP_CLOSED);
    ASSERT_EQ(action, NFS_TCP_ACT_DELETE_TCB);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: simultaneous close
 *   ESTABLISHED -> FIN_WAIT_1 (close, send FIN)
 *   FIN_WAIT_1 -> CLOSING (recv FIN, send ACK)
 *   CLOSING -> TIME_WAIT (recv ACK)
 *   TIME_WAIT -> CLOSED (timeout)
 * --------------------------------------------------------------- */
static void test_simultaneous_close(void) {
    printf("  simultaneous close...");

    struct nfs_tcp_fsm fsm;
    int action;

    nfs_tcp_fsm_init(&fsm);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_ACTIVE, &action);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_SYNACK, &action);
    ASSERT_EQ(fsm.state, NFS_TCP_ESTABLISHED);

    /* Both sides close simultaneously. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_CLOSE, &action), NFS_TCP_FIN_WAIT_1);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_FIN);

    /* Receive FIN before our FIN's ACK. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_FIN, &action), NFS_TCP_CLOSING);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_ACK);

    /* Receive ACK of our FIN. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_ACK, &action), NFS_TCP_TIME_WAIT);
    ASSERT_EQ(action, NFS_TCP_ACT_NONE);

    /* Timeout. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_TIMEOUT, &action), NFS_TCP_CLOSED);
    ASSERT_EQ(action, NFS_TCP_ACT_DELETE_TCB);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: FIN_WAIT_1 -> TIME_WAIT (FIN+ACK received)
 * --------------------------------------------------------------- */
static void test_finwait1_to_timewait(void) {
    printf("  FIN_WAIT_1 -> TIME_WAIT (FINACK)...");

    struct nfs_tcp_fsm fsm;
    int action;

    nfs_tcp_fsm_init(&fsm);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_ACTIVE, &action);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_SYNACK, &action);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_CLOSE, &action);
    ASSERT_EQ(fsm.state, NFS_TCP_FIN_WAIT_1);

    /* Receive FIN+ACK simultaneously. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_FINACK, &action), NFS_TCP_TIME_WAIT);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_ACK);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: invalid transitions return -1
 * --------------------------------------------------------------- */
static void test_invalid_transitions(void) {
    printf("  invalid transitions...");

    struct nfs_tcp_fsm fsm;
    int action;

    nfs_tcp_fsm_init(&fsm);

    /* CLOSED: cannot receive SYN. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_SYN, &action), -1);
    ASSERT_EQ(fsm.state, NFS_TCP_CLOSED); /* State unchanged. */

    /* CLOSED: cannot CLOSE. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_CLOSE, &action), -1);

    /* CLOSED: cannot receive ACK. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_ACK, &action), -1);

    /* CLOSED: cannot receive FIN. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_FIN, &action), -1);

    /* CLOSED: cannot timeout. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_TIMEOUT, &action), -1);

    /* Go to ESTABLISHED, then try invalid events. */
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_ACTIVE, &action);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_SYNACK, &action);
    ASSERT_EQ(fsm.state, NFS_TCP_ESTABLISHED);

    /* ESTABLISHED: cannot receive SYN. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_SYN, &action), -1);
    ASSERT_EQ(fsm.state, NFS_TCP_ESTABLISHED);

    /* ESTABLISHED: cannot passive open. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_PASSIVE, &action), -1);

    /* ESTABLISHED: cannot timeout. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_TIMEOUT, &action), -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: state names
 * --------------------------------------------------------------- */
static void test_state_names(void) {
    printf("  state names...");

    ASSERT_TRUE(strcmp(nfs_tcp_state_name(NFS_TCP_CLOSED), "CLOSED") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_state_name(NFS_TCP_LISTEN), "LISTEN") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_state_name(NFS_TCP_SYN_SENT), "SYN_SENT") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_state_name(NFS_TCP_SYN_RCVD), "SYN_RCVD") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_state_name(NFS_TCP_ESTABLISHED), "ESTABLISHED") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_state_name(NFS_TCP_FIN_WAIT_1), "FIN_WAIT_1") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_state_name(NFS_TCP_FIN_WAIT_2), "FIN_WAIT_2") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_state_name(NFS_TCP_CLOSE_WAIT), "CLOSE_WAIT") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_state_name(NFS_TCP_CLOSING), "CLOSING") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_state_name(NFS_TCP_LAST_ACK), "LAST_ACK") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_state_name(NFS_TCP_TIME_WAIT), "TIME_WAIT") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_state_name(-1), "UNKNOWN") == 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: event names
 * --------------------------------------------------------------- */
static void test_event_names(void) {
    printf("  event names...");

    ASSERT_TRUE(strcmp(nfs_tcp_event_name(NFS_TCP_EV_OPEN_PASSIVE), "OPEN_PASSIVE") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_event_name(NFS_TCP_EV_OPEN_ACTIVE), "OPEN_ACTIVE") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_event_name(NFS_TCP_EV_CLOSE), "CLOSE") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_event_name(NFS_TCP_EV_RECV_SYN), "RECV_SYN") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_event_name(NFS_TCP_EV_RECV_SYNACK), "RECV_SYNACK") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_event_name(NFS_TCP_EV_RECV_ACK), "RECV_ACK") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_event_name(NFS_TCP_EV_RECV_FIN), "RECV_FIN") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_event_name(NFS_TCP_EV_RECV_FINACK), "RECV_FINACK") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_event_name(NFS_TCP_EV_TIMEOUT), "TIMEOUT") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_event_name(-1), "UNKNOWN") == 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 11: action names
 * --------------------------------------------------------------- */
static void test_action_names(void) {
    printf("  action names...");

    ASSERT_TRUE(strcmp(nfs_tcp_action_name(NFS_TCP_ACT_NONE), "NONE") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_action_name(NFS_TCP_ACT_SEND_SYN), "SEND_SYN") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_action_name(NFS_TCP_ACT_SEND_SYNACK), "SEND_SYNACK") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_action_name(NFS_TCP_ACT_SEND_ACK), "SEND_ACK") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_action_name(NFS_TCP_ACT_SEND_FIN), "SEND_FIN") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_action_name(NFS_TCP_ACT_SEND_FINACK), "SEND_FINACK") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_action_name(NFS_TCP_ACT_DELETE_TCB), "DELETE_TCB") == 0);
    ASSERT_TRUE(strcmp(nfs_tcp_action_name(-1), "UNKNOWN") == 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 12: close from LISTEN
 * --------------------------------------------------------------- */
static void test_close_from_listen(void) {
    printf("  close from LISTEN...");

    struct nfs_tcp_fsm fsm;
    int action;

    nfs_tcp_fsm_init(&fsm);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_PASSIVE, &action);
    ASSERT_EQ(fsm.state, NFS_TCP_LISTEN);

    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_CLOSE, &action), NFS_TCP_CLOSED);
    ASSERT_EQ(action, NFS_TCP_ACT_DELETE_TCB);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 13: close from SYN_SENT
 * --------------------------------------------------------------- */
static void test_close_from_syn_sent(void) {
    printf("  close from SYN_SENT...");

    struct nfs_tcp_fsm fsm;
    int action;

    nfs_tcp_fsm_init(&fsm);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_ACTIVE, &action);
    ASSERT_EQ(fsm.state, NFS_TCP_SYN_SENT);

    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_CLOSE, &action), NFS_TCP_CLOSED);
    ASSERT_EQ(action, NFS_TCP_ACT_DELETE_TCB);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 14: close from SYN_RCVD
 * --------------------------------------------------------------- */
static void test_close_from_syn_rcvd(void) {
    printf("  close from SYN_RCVD...");

    struct nfs_tcp_fsm fsm;
    int action;

    nfs_tcp_fsm_init(&fsm);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_PASSIVE, &action);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_SYN, &action);
    ASSERT_EQ(fsm.state, NFS_TCP_SYN_RCVD);

    /* Close from SYN_RCVD -> FIN_WAIT_1 (send FIN). */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_CLOSE, &action), NFS_TCP_FIN_WAIT_1);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_FIN);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 15: simultaneous open
 *   Both sides do active open.  SYN_SENT + recv SYN -> SYN_RCVD.
 * --------------------------------------------------------------- */
static void test_simultaneous_open(void) {
    printf("  simultaneous open...");

    struct nfs_tcp_fsm fsm;
    int action;

    nfs_tcp_fsm_init(&fsm);
    nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_OPEN_ACTIVE, &action);
    ASSERT_EQ(fsm.state, NFS_TCP_SYN_SENT);

    /* Receive SYN (not SYN-ACK): simultaneous open. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_SYN, &action), NFS_TCP_SYN_RCVD);
    ASSERT_EQ(action, NFS_TCP_ACT_SEND_SYNACK);

    /* Then receive ACK to complete. */
    ASSERT_EQ(nfs_tcp_fsm_handle(&fsm, NFS_TCP_EV_RECV_ACK, &action), NFS_TCP_ESTABLISHED);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */
int main(void) {
    printf("TCP state machine (RFC 9293) test suite\n");

    test_init();
    test_client_3whs();
    test_server_3whs();
    test_client_close();
    test_server_close();
    test_simultaneous_close();
    test_finwait1_to_timewait();
    test_invalid_transitions();
    test_state_names();
    test_event_names();
    test_action_names();
    test_close_from_listen();
    test_close_from_syn_sent();
    test_close_from_syn_rcvd();
    test_simultaneous_open();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
