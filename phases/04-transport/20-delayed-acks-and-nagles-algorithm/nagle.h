#ifndef NFS_NAGLE_H
#define NFS_NAGLE_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Nagle's algorithm (RFC 896) decision logic.
 *
 * Nagle prevents sending many small segments when there is
 * outstanding unacknowledged data.  A segment can be sent if:
 *   (a) data_len >= MSS (a full segment), OR
 *   (b) no unACKed data (snd_una == snd_nxt), OR
 *   (c) Nagle is disabled (TCP_NODELAY).
 * --------------------------------------------------------------- */

struct nfs_nagle {
    int enabled;      /* 1 = Nagle on (default), 0 = TCP_NODELAY */
    uint32_t snd_una; /* oldest unacknowledged sequence number */
    uint32_t snd_nxt; /* next sequence number to send */
    uint16_t mss;     /* maximum segment size */
};

/* Initialise Nagle state.  Enabled by default. */
void nfs_nagle_init(struct nfs_nagle *n, uint16_t mss);

/* Return 1 if data of data_len bytes may be sent now, 0 if it
 * should be buffered. */
int nfs_nagle_can_send(const struct nfs_nagle *n, size_t data_len);

/* Enable Nagle's algorithm. */
void nfs_nagle_enable(struct nfs_nagle *n);

/* Disable Nagle's algorithm (TCP_NODELAY). */
void nfs_nagle_disable(struct nfs_nagle *n);

/* Record that nbytes were sent (advance snd_nxt). */
void nfs_nagle_sent(struct nfs_nagle *n, uint32_t nbytes);

/* Record cumulative ACK (advance snd_una). */
void nfs_nagle_acked(struct nfs_nagle *n, uint32_t ack);

/* ---------------------------------------------------------------
 * Delayed ACK timer (RFC 1122, section 4.2.3.2).
 *
 * ACKs should be delayed up to 200ms.  However, an ACK must be
 * sent immediately when a second full-sized segment arrives before
 * the timer expires (every-other-segment rule).
 * --------------------------------------------------------------- */

struct nfs_delayed_ack {
    int pending;     /* ACK waiting to be sent */
    double deadline; /* when the delayed ACK timer fires */
    double delay;    /* configured delay (typically 200ms) */
    int seg_count;   /* segments received since last ACK */
};

/* Initialise delayed ACK state with given delay in seconds. */
void nfs_delayed_ack_init(struct nfs_delayed_ack *da, double delay);

/* Data segment received at time 'now'.
 * Return 1 if ACK should be sent immediately (2nd segment rule).
 * Return 0 if ACK is delayed (timer started/running). */
int nfs_delayed_ack_receive(struct nfs_delayed_ack *da, double now);

/* Check if delayed ACK timer has expired.
 * Return 1 if ACK should be sent now. */
int nfs_delayed_ack_check(struct nfs_delayed_ack *da, double now);

/* Reset state after ACK is actually sent. */
void nfs_delayed_ack_sent(struct nfs_delayed_ack *da);

#endif /* NFS_NAGLE_H */
