/* teardown.c — TCP connection teardown FSM implementation.
 *
 * Models the TCP state transitions for graceful connection
 * termination, including the TIME_WAIT state that prevents
 * old duplicate segments from confusing new connections. */

#include "teardown.h"

/* ---------------------------------------------------------------
 * Init
 * --------------------------------------------------------------- */

void nfs_conn_init(struct nfs_tcp_conn *c, uint32_t local_seq, uint32_t remote_seq, double msl) {
    c->state = NFS_TCP_ST_ESTABLISHED;
    c->local_seq = local_seq;
    c->remote_seq = remote_seq;
    c->time_wait_start = 0.0;
    c->msl = msl;
}

/* ---------------------------------------------------------------
 * Close (active close — send FIN)
 * --------------------------------------------------------------- */

int nfs_conn_close(struct nfs_tcp_conn *c) {
    switch (c->state) {
    case NFS_TCP_ST_ESTABLISHED:
        /* Application initiates close. Send FIN, transition to FIN_WAIT_1.
         * FIN consumes one sequence number. */
        c->state = NFS_TCP_ST_FIN_WAIT_1;
        c->local_seq++;
        return 0;

    case NFS_TCP_ST_CLOSE_WAIT:
        /* We received peer's FIN, now application closes.
         * Send our FIN, transition to LAST_ACK. */
        c->state = NFS_TCP_ST_LAST_ACK;
        c->local_seq++;
        return 0;

    default:
        return -1; /* invalid state for close */
    }
}

/* ---------------------------------------------------------------
 * Receive FIN
 * --------------------------------------------------------------- */

int nfs_conn_recv_fin(struct nfs_tcp_conn *c) {
    switch (c->state) {
    case NFS_TCP_ST_ESTABLISHED:
        /* Passive close: peer initiated teardown.
         * We ACK the FIN and wait for application to close. */
        c->state = NFS_TCP_ST_CLOSE_WAIT;
        c->remote_seq++;
        return 0;

    case NFS_TCP_ST_FIN_WAIT_1:
        /* Simultaneous close: both sides sent FIN before
         * receiving the other's FIN. */
        c->state = NFS_TCP_ST_CLOSING;
        c->remote_seq++;
        return 0;

    case NFS_TCP_ST_FIN_WAIT_2:
        /* Normal active close: we already got ACK for our FIN,
         * now we receive peer's FIN. Enter TIME_WAIT. */
        c->state = NFS_TCP_ST_TIME_WAIT;
        c->remote_seq++;
        /* time_wait_start will be set by the caller using a real clock.
         * For testability, we set it to 0.0 and let the caller
         * drive the timer via nfs_conn_time_wait_expired(). */
        c->time_wait_start = 0.0; /* caller should set this */
        return 0;

    default:
        return -1; /* invalid state for receiving FIN */
    }
}

/* ---------------------------------------------------------------
 * Receive ACK
 * --------------------------------------------------------------- */

int nfs_conn_recv_ack(struct nfs_tcp_conn *c) {
    switch (c->state) {
    case NFS_TCP_ST_FIN_WAIT_1:
        /* Our FIN was acknowledged. If we haven't received peer's
         * FIN yet, go to FIN_WAIT_2. */
        c->state = NFS_TCP_ST_FIN_WAIT_2;
        return 0;

    case NFS_TCP_ST_CLOSING:
        /* Simultaneous close: our FIN was acknowledged.
         * Enter TIME_WAIT. */
        c->state = NFS_TCP_ST_TIME_WAIT;
        c->time_wait_start = 0.0; /* caller should set this */
        return 0;

    case NFS_TCP_ST_LAST_ACK:
        /* Passive close complete: our FIN was acknowledged.
         * Connection is fully closed. */
        c->state = NFS_TCP_ST_CLOSED;
        return 0;

    default:
        return -1; /* no ACK-driven transition from this state */
    }
}

/* ---------------------------------------------------------------
 * TIME_WAIT expiry check
 * --------------------------------------------------------------- */

int nfs_conn_time_wait_expired(struct nfs_tcp_conn *c, double now) {
    if (c->state != NFS_TCP_ST_TIME_WAIT)
        return -1;

    double elapsed = now - c->time_wait_start;
    double timeout = 2.0 * c->msl;

    if (elapsed >= timeout) {
        c->state = NFS_TCP_ST_CLOSED;
        return 1;
    }

    return 0;
}

/* ---------------------------------------------------------------
 * State name
 * --------------------------------------------------------------- */

const char *nfs_conn_state_name(int state) {
    switch (state) {
    case NFS_TCP_ST_ESTABLISHED:
        return "ESTABLISHED";
    case NFS_TCP_ST_FIN_WAIT_1:
        return "FIN_WAIT_1";
    case NFS_TCP_ST_FIN_WAIT_2:
        return "FIN_WAIT_2";
    case NFS_TCP_ST_CLOSING:
        return "CLOSING";
    case NFS_TCP_ST_TIME_WAIT:
        return "TIME_WAIT";
    case NFS_TCP_ST_CLOSE_WAIT:
        return "CLOSE_WAIT";
    case NFS_TCP_ST_LAST_ACK:
        return "LAST_ACK";
    case NFS_TCP_ST_CLOSED:
        return "CLOSED";
    default:
        return "UNKNOWN";
    }
}
