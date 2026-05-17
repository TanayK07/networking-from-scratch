#ifndef NFS_TCB_H
#define NFS_TCB_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Transmission Control Block (TCB)
 *
 * The TCB stores per-connection TCP state as defined in RFC 9293.
 * It tracks the 4-tuple (local/remote addr+port), sequence numbers
 * for both send and receive directions, and window sizes.
 * --------------------------------------------------------------- */

/* TCP connection states (subset for this lesson) */
#define NFS_TCB_CLOSED       0
#define NFS_TCB_LISTEN       1
#define NFS_TCB_SYN_SENT     2
#define NFS_TCB_SYN_RECEIVED 3
#define NFS_TCB_ESTABLISHED  4
#define NFS_TCB_FIN_WAIT_1   5
#define NFS_TCB_FIN_WAIT_2   6
#define NFS_TCB_CLOSE_WAIT   7
#define NFS_TCB_CLOSING      8
#define NFS_TCB_LAST_ACK     9
#define NFS_TCB_TIME_WAIT    10

struct nfs_tcb {
    int state;        /* TCP state (NFS_TCB_*) */
    uint32_t snd_una; /* oldest unacknowledged sequence number */
    uint32_t snd_nxt; /* next sequence number to send */
    uint32_t snd_wnd; /* send window (bytes peer will accept) */
    uint32_t rcv_nxt; /* next expected receive sequence number */
    uint32_t rcv_wnd; /* receive window (bytes we will accept) */
    uint32_t iss;     /* initial send sequence number */
    uint32_t irs;     /* initial receive sequence number */
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t local_addr; /* IPv4 address in host byte order */
    uint32_t remote_addr;
};

/* Initialize a TCB with the 4-tuple.  All sequence numbers start at 0,
 * state is CLOSED, default windows are 65535. */
void nfs_tcb_init(struct nfs_tcb *tcb, uint16_t local_port, uint16_t remote_port,
                  uint32_t local_addr, uint32_t remote_addr);

/* Set the Initial Send Sequence number.
 * Also sets snd_una = iss, snd_nxt = iss + 1. */
void nfs_tcb_set_iss(struct nfs_tcb *tcb, uint32_t iss);

/* Set the Initial Receive Sequence number.
 * Also sets rcv_nxt = irs + 1. */
void nfs_tcb_set_irs(struct nfs_tcb *tcb, uint32_t irs);

/* Return the number of bytes we can still send within the window.
 * snd_wnd - (snd_nxt - snd_una). */
int nfs_tcb_snd_available(const struct nfs_tcb *tcb);

/* Advance snd_nxt by nbytes (data queued for sending). */
void nfs_tcb_advance_snd(struct nfs_tcb *tcb, uint32_t nbytes);

/* Process an incoming ACK.  Advances snd_una if ack_num > snd_una. */
void nfs_tcb_ack_received(struct nfs_tcb *tcb, uint32_t ack_num);

/* Record that nbytes of data were received in order. rcv_nxt += nbytes. */
void nfs_tcb_data_received(struct nfs_tcb *tcb, uint32_t nbytes);

/* Format the TCB state into buf: 4-tuple + sequence state.
 * Writes at most sz bytes (including NUL). */
void nfs_tcb_format(const struct nfs_tcb *tcb, char *buf, size_t sz);

/* Check if a packet with the given 4-tuple belongs to this connection.
 * Returns 1 if it matches, 0 otherwise. */
int nfs_tcb_matches(const struct nfs_tcb *tcb, uint32_t src_addr, uint16_t src_port,
                    uint32_t dst_addr, uint16_t dst_port);

#endif /* NFS_TCB_H */
