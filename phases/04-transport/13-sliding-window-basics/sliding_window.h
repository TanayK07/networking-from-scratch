#ifndef NFS_SLIDING_WINDOW_H
#define NFS_SLIDING_WINDOW_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Send-side sliding window for reliable data transfer.
 *
 * Tracks which bytes have been sent but not yet acknowledged (in-flight),
 * and enforces the receiver's advertised window size.
 *
 *     [--- acked ---|--- in-flight ---|--- available ---|--- beyond window ---]
 *                   ^base             ^next_seq         ^base+window_size
 *
 * base     = oldest unacknowledged sequence number
 * next_seq = next sequence number to be sent
 * --------------------------------------------------------------- */

struct nfs_send_window {
    uint32_t base;        /* oldest unacknowledged seq */
    uint32_t next_seq;    /* next seq to send */
    uint32_t window_size; /* receiver advertised window */
    uint8_t *buffer;      /* circular buffer for unacked data */
    size_t buf_cap;       /* capacity of buffer */
    int *acked;           /* per-byte ACK tracking (for future selective ACK) */
};

/* Initialize the send window.
 * isn      = initial sequence number
 * win_size = advertised window size (in bytes)
 * buf_cap  = buffer capacity (must be >= win_size) */
void nfs_send_window_init(struct nfs_send_window *w, uint32_t isn, uint32_t win_size,
                          size_t buf_cap);

/* Free dynamically allocated buffers. */
void nfs_send_window_free(struct nfs_send_window *w);

/* Can we send more data? (is the window not full?) */
int nfs_send_window_can_send(const struct nfs_send_window *w);

/* Buffer one byte for sending and advance next_seq.
 * Returns 0 on success, -1 if the window is full. */
int nfs_send_window_send(struct nfs_send_window *w, uint8_t byte);

/* Process a cumulative ACK: acknowledge all bytes up to ack_num.
 * Returns the number of bytes newly acknowledged, or -1 on error. */
int nfs_send_window_ack(struct nfs_send_window *w, uint32_t ack_num);

/* Number of bytes currently in-flight (sent but unacked). */
uint32_t nfs_send_window_in_flight(const struct nfs_send_window *w);

/* Number of bytes still available in the window. */
uint32_t nfs_send_window_available(const struct nfs_send_window *w);

/* Get a pointer to buffered data at the given sequence number.
 * Useful for retransmission. Sets *len to available contiguous bytes.
 * Returns NULL if seq is outside the in-flight range. */
const uint8_t *nfs_send_window_get_data(const struct nfs_send_window *w, uint32_t seq, size_t *len);

#endif /* NFS_SLIDING_WINDOW_H */
