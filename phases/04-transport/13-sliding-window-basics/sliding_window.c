/* sliding_window.c — Send-side sliding window implementation.
 *
 * This implements the core mechanism TCP uses to pipeline data on the
 * wire while respecting the receiver's advertised window. The sender
 * can have up to window_size bytes in-flight (sent but not ACKed).
 *
 * Cumulative ACKs slide the window forward, freeing buffer space for
 * new data. */

#include "sliding_window.h"

#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Init / Free
 * --------------------------------------------------------------- */

void nfs_send_window_init(struct nfs_send_window *w, uint32_t isn, uint32_t win_size,
                          size_t buf_cap) {
    w->base = isn;
    w->next_seq = isn;
    w->window_size = win_size;
    w->buf_cap = buf_cap;
    w->buffer = calloc(buf_cap, sizeof(uint8_t));
    w->acked = calloc(buf_cap, sizeof(int));
}

void nfs_send_window_free(struct nfs_send_window *w) {
    free(w->buffer);
    free(w->acked);
    w->buffer = NULL;
    w->acked = NULL;
}

/* ---------------------------------------------------------------
 * Window queries
 * --------------------------------------------------------------- */

uint32_t nfs_send_window_in_flight(const struct nfs_send_window *w) {
    return w->next_seq - w->base;
}

uint32_t nfs_send_window_available(const struct nfs_send_window *w) {
    return w->window_size - nfs_send_window_in_flight(w);
}

int nfs_send_window_can_send(const struct nfs_send_window *w) {
    return nfs_send_window_in_flight(w) < w->window_size;
}

/* ---------------------------------------------------------------
 * Send
 * --------------------------------------------------------------- */

int nfs_send_window_send(struct nfs_send_window *w, uint8_t byte) {
    if (!nfs_send_window_can_send(w))
        return -1;

    size_t idx = (size_t)(w->next_seq % w->buf_cap);
    w->buffer[idx] = byte;
    w->acked[idx] = 0;
    w->next_seq++;
    return 0;
}

/* ---------------------------------------------------------------
 * ACK processing — cumulative acknowledgement
 * --------------------------------------------------------------- */

int nfs_send_window_ack(struct nfs_send_window *w, uint32_t ack_num) {
    /* ack_num must be within [base, next_seq].
     * ack_num == base means duplicate ACK (0 new bytes).
     * ack_num > next_seq is invalid (acking data we never sent). */
    if (ack_num < w->base || ack_num > w->next_seq)
        return -1;

    uint32_t newly_acked = ack_num - w->base;

    /* Mark bytes as acknowledged */
    for (uint32_t seq = w->base; seq < ack_num; seq++) {
        size_t idx = (size_t)(seq % w->buf_cap);
        w->acked[idx] = 1;
    }

    w->base = ack_num;
    return (int)newly_acked;
}

/* ---------------------------------------------------------------
 * Get buffered data for retransmission
 * --------------------------------------------------------------- */

const uint8_t *nfs_send_window_get_data(const struct nfs_send_window *w, uint32_t seq,
                                        size_t *len) {
    /* seq must be within the in-flight range [base, next_seq) */
    if (seq < w->base || seq >= w->next_seq) {
        *len = 0;
        return NULL;
    }

    size_t idx = (size_t)(seq % w->buf_cap);
    /* Contiguous bytes available from idx to either end of buffer
     * or end of in-flight data, whichever comes first. */
    size_t to_end_of_buf = w->buf_cap - idx;
    size_t to_end_of_flight = (size_t)(w->next_seq - seq);
    *len = (to_end_of_buf < to_end_of_flight) ? to_end_of_buf : to_end_of_flight;
    return &w->buffer[idx];
}
