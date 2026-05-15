#ifndef NFS_TCP_FSM_H
#define NFS_TCP_FSM_H

/* ---------------------------------------------------------------
 * TCP Finite State Machine (RFC 9293).
 *
 * The 11 TCP states and the transitions between them, implemented
 * as a pure function.  No networking -- just state transitions.
 *
 *                              +---------+ ---------\      active OPEN
 *                              |  CLOSED |            \    -----------
 *                              +---------+<---------\   \   snd SYN
 *                                |     ^              \   \
 *                   passive OPEN |     |   CLOSE        \   \
 *                   ----------- |     | ----------       \   \
 *                    create TCB |     | delete TCB         \   \
 *                                V     |                     \   V
 *                              +---------+            +---------+
 *                              |  LISTEN |            | SYN     |
 *                              +---------+            | SENT    |
 *                    rcv SYN  /  |     \              +---------+
 *                   --------/    |      \    rcv SYN,ACK
 *                  snd SYN,ACK   |       \  ----------
 *                     /          |        \ snd ACK
 *                    V           |         V
 *              +---------+       |   +---------+
 *              | SYN     |       |   |  ESTAB  |
 *              | RCVD    |<------+   +---------+
 *              +---------+   rcv ACK   |     |
 *                |       of SYN    CLOSE|    | rcv FIN
 *                |                ------     | -------
 *                |               snd FIN     | snd ACK
 *                V                   V       V
 *              +---------+       +---------+ +---------+
 *              | FIN     |       | FIN     | | CLOSE   |
 *              | WAIT-1  |       | WAIT-1  | | WAIT    |
 *              +---------+       +---------+ +---------+
 * --------------------------------------------------------------- */

/* ---------------------------------------------------------------
 * TCP states (RFC 9293, Section 3.3.2)
 * --------------------------------------------------------------- */
enum {
    NFS_TCP_CLOSED = 0,
    NFS_TCP_LISTEN,
    NFS_TCP_SYN_SENT,
    NFS_TCP_SYN_RCVD,
    NFS_TCP_ESTABLISHED,
    NFS_TCP_FIN_WAIT_1,
    NFS_TCP_FIN_WAIT_2,
    NFS_TCP_CLOSE_WAIT,
    NFS_TCP_CLOSING,
    NFS_TCP_LAST_ACK,
    NFS_TCP_TIME_WAIT
};

/* ---------------------------------------------------------------
 * TCP events (inputs to the state machine)
 * --------------------------------------------------------------- */
enum {
    NFS_TCP_EV_OPEN_PASSIVE = 0,
    NFS_TCP_EV_OPEN_ACTIVE,
    NFS_TCP_EV_CLOSE,
    NFS_TCP_EV_RECV_SYN,
    NFS_TCP_EV_RECV_SYNACK,
    NFS_TCP_EV_RECV_ACK,
    NFS_TCP_EV_RECV_FIN,
    NFS_TCP_EV_RECV_FINACK,
    NFS_TCP_EV_TIMEOUT
};

/* ---------------------------------------------------------------
 * TCP actions (outputs from the state machine)
 * --------------------------------------------------------------- */
enum {
    NFS_TCP_ACT_NONE = 0,
    NFS_TCP_ACT_SEND_SYN,
    NFS_TCP_ACT_SEND_SYNACK,
    NFS_TCP_ACT_SEND_ACK,
    NFS_TCP_ACT_SEND_FIN,
    NFS_TCP_ACT_SEND_FINACK,
    NFS_TCP_ACT_DELETE_TCB
};

/* ---------------------------------------------------------------
 * TCP FSM structure
 * --------------------------------------------------------------- */
struct nfs_tcp_fsm {
    int state;
};

/* ---------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

/* Initialize the FSM to CLOSED state. */
void nfs_tcp_fsm_init(struct nfs_tcp_fsm *fsm);

/* Handle an event.  Performs the state transition and sets *action
 * to the resulting action.  Returns the new state, or -1 if the
 * transition is invalid. */
int nfs_tcp_fsm_handle(struct nfs_tcp_fsm *fsm, int event, int *action);

/* Return a human-readable name for a state. */
const char *nfs_tcp_state_name(int state);

/* Return a human-readable name for an event. */
const char *nfs_tcp_event_name(int event);

/* Return a human-readable name for an action. */
const char *nfs_tcp_action_name(int action);

#endif /* NFS_TCP_FSM_H */
