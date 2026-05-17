#ifndef NFS_RECV_WIN_H
#define NFS_RECV_WIN_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP receive window management.
 *
 * The receiver maintains a buffer and advertises how much space
 * is available (the "receive window").  When the buffer is full,
 * the advertised window is zero and the sender enters the
 * zero-window probe state per RFC 9293 section 3.8.6.
 * --------------------------------------------------------------- */

struct nfs_recv_window {
    uint8_t *buffer;  /* receive buffer */
    size_t capacity;  /* total buffer size */
    size_t used;      /* bytes currently in buffer */
    uint32_t rcv_nxt; /* next expected sequence number */
};

/* Initialise receive window with given capacity. */
void nfs_recv_window_init(struct nfs_recv_window *w, size_t capacity);

/* Free internal buffer. */
void nfs_recv_window_free(struct nfs_recv_window *w);

/* Return advertised window size (capacity - used). */
uint32_t nfs_recv_window_advertised(const struct nfs_recv_window *w);

/* Receive data from the network.  Data is accepted only if
 * seq == rcv_nxt and there is space.
 * Return: bytes accepted (>= 0), or -1 if seq is wrong. */
int nfs_recv_window_receive(struct nfs_recv_window *w, uint32_t seq, const uint8_t *data,
                            size_t len);

/* Application reads data from the buffer (frees space).
 * Return bytes actually read. */
size_t nfs_recv_window_read(struct nfs_recv_window *w, uint8_t *out, size_t max);

/* Return 1 if advertised window is zero. */
int nfs_recv_window_is_zero(const struct nfs_recv_window *w);

/* ---------------------------------------------------------------
 * Zero-window probe (ZWP) timer.
 *
 * When the receiver advertises window=0, the sender periodically
 * sends 1-byte probes to discover when the window reopens.
 * --------------------------------------------------------------- */

struct nfs_zwp {
    int active;             /* probing is active */
    double interval;        /* probe interval in seconds */
    double last_probe_time; /* timestamp of last probe sent */
    int probe_count;        /* number of probes sent */
};

/* Initialise ZWP timer with the given probe interval. */
void nfs_zwp_init(struct nfs_zwp *z, double interval);

/* Start probing (called when window goes to zero). */
int nfs_zwp_start(struct nfs_zwp *z, double now);

/* Check if a probe should be sent now.  Return 1 if yes. */
int nfs_zwp_check(struct nfs_zwp *z, double now);

/* Stop probing (called when window reopens). */
void nfs_zwp_stop(struct nfs_zwp *z);

#endif /* NFS_RECV_WIN_H */
