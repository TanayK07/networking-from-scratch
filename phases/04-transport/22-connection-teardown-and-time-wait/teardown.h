#ifndef NFS_TEARDOWN_H
#define NFS_TEARDOWN_H

#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP Connection Teardown FSM and TIME_WAIT State.
 *
 * Implements the graceful close sequence from RFC 9293:
 *
 * Active close (initiator sends FIN):
 *   ESTABLISHED -> FIN_WAIT_1 -> FIN_WAIT_2 -> TIME_WAIT -> CLOSED
 *
 * Passive close (receives FIN first):
 *   ESTABLISHED -> CLOSE_WAIT -> LAST_ACK -> CLOSED
 *
 * Simultaneous close (both sides send FIN):
 *   FIN_WAIT_1 -> CLOSING -> TIME_WAIT -> CLOSED
 *
 * TIME_WAIT lasts 2 * MSL (Maximum Segment Lifetime) to ensure
 * old duplicate segments have expired before the 4-tuple can be
 * reused.
 * --------------------------------------------------------------- */

/* TCP connection states relevant to teardown. */
enum nfs_tcp_teardown_state {
    NFS_TCP_ST_ESTABLISHED = 0,
    NFS_TCP_ST_FIN_WAIT_1,
    NFS_TCP_ST_FIN_WAIT_2,
    NFS_TCP_ST_CLOSING,
    NFS_TCP_ST_TIME_WAIT,
    NFS_TCP_ST_CLOSE_WAIT,
    NFS_TCP_ST_LAST_ACK,
    NFS_TCP_ST_CLOSED,
};

/* Connection context for teardown tracking. */
struct nfs_tcp_conn {
    int      state;           /* enum nfs_tcp_teardown_state */
    uint32_t local_seq;       /* local sequence number */
    uint32_t remote_seq;      /* remote sequence number */
    double   time_wait_start; /* timestamp when TIME_WAIT was entered */
    double   msl;             /* Maximum Segment Lifetime (seconds) */
};

/* Initialize a connection in ESTABLISHED state.
 * msl = Maximum Segment Lifetime in seconds (typically 30-120s). */
void nfs_conn_init(struct nfs_tcp_conn *c, uint32_t local_seq,
                   uint32_t remote_seq, double msl);

/* Initiate active close (application calls close()).
 * ESTABLISHED -> FIN_WAIT_1  (send FIN)
 * CLOSE_WAIT  -> LAST_ACK    (send FIN)
 * Returns 0 on success, -1 if transition is invalid. */
int nfs_conn_close(struct nfs_tcp_conn *c);

/* Process a received FIN segment.
 * ESTABLISHED  -> CLOSE_WAIT  (passive close start)
 * FIN_WAIT_1   -> CLOSING     (simultaneous close)
 * FIN_WAIT_2   -> TIME_WAIT   (normal active close completion)
 * Returns 0 on success, -1 if transition is invalid. */
int nfs_conn_recv_fin(struct nfs_tcp_conn *c);

/* Process a received ACK segment.
 * FIN_WAIT_1  -> FIN_WAIT_2  (our FIN was ACKed)
 * CLOSING     -> TIME_WAIT   (simultaneous close ACK)
 * LAST_ACK    -> CLOSED      (passive close completion)
 * Returns 0 on success, -1 if transition is invalid. */
int nfs_conn_recv_ack(struct nfs_tcp_conn *c);

/* Check if the TIME_WAIT timer has expired (2 * MSL).
 * If expired, transitions to CLOSED and returns 1.
 * If not expired, returns 0.
 * If not in TIME_WAIT, returns -1. */
int nfs_conn_time_wait_expired(struct nfs_tcp_conn *c, double now);

/* Return a human-readable name for the state. */
const char *nfs_conn_state_name(int state);

#endif /* NFS_TEARDOWN_H */
