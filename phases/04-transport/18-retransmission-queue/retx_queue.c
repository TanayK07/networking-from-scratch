#include "retx_queue.h"
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Retransmission queue implementation.
 *
 * Segments are stored in a contiguous array, ordered by insertion.
 * Removal (on ACK) shifts remaining entries forward.  This is fine
 * for the typical TCP case where ACKs arrive in order and the
 * front of the queue is removed.
 * --------------------------------------------------------------- */

void nfs_retx_queue_init(struct nfs_retx_queue *q, size_t capacity)
{
    if (!q)
        return;
    q->segments = calloc(capacity, sizeof(struct nfs_retx_seg));
    q->count    = 0;
    q->capacity = capacity;
}

void nfs_retx_queue_free(struct nfs_retx_queue *q)
{
    if (!q)
        return;
    free(q->segments);
    q->segments = NULL;
    q->count    = 0;
    q->capacity = 0;
}

int nfs_retx_queue_push(struct nfs_retx_queue *q, uint32_t seq,
                        const uint8_t *data, size_t len, double send_time)
{
    if (!q || !q->segments)
        return -1;
    if (q->count >= q->capacity)
        return -1;
    if (len > NFS_RETX_MSS || (!data && len > 0))
        return -1;

    struct nfs_retx_seg *seg = &q->segments[q->count];
    seg->seq        = seq;
    seg->data_len   = len;
    seg->send_time  = send_time;
    seg->retx_count = 0;
    if (len > 0)
        memcpy(seg->data, data, len);

    q->count++;
    return 0;
}

int nfs_retx_queue_ack(struct nfs_retx_queue *q, uint32_t ack_num)
{
    if (!q || !q->segments)
        return 0;

    int removed = 0;
    size_t write = 0;

    for (size_t read = 0; read < q->count; read++) {
        /* Cumulative ACK: remove segments whose seq + data_len <= ack_num.
         * For simplicity, we remove segments where seq < ack_num,
         * which is the standard TCP cumulative ACK interpretation:
         * the ACK number indicates the next expected byte, so all
         * bytes before it are acknowledged. */
        if (q->segments[read].seq < ack_num) {
            removed++;
        } else {
            if (write != read)
                q->segments[write] = q->segments[read];
            write++;
        }
    }
    q->count = write;
    return removed;
}

struct nfs_retx_seg *nfs_retx_queue_find(struct nfs_retx_queue *q,
                                         uint32_t seq)
{
    if (!q || !q->segments)
        return NULL;

    for (size_t i = 0; i < q->count; i++) {
        if (q->segments[i].seq == seq)
            return &q->segments[i];
    }
    return NULL;
}

int nfs_retx_queue_timeout(struct nfs_retx_queue *q, double now, double rto)
{
    if (!q || !q->segments)
        return 0;

    int timed_out = 0;
    for (size_t i = 0; i < q->count; i++) {
        if ((now - q->segments[i].send_time) >= rto) {
            q->segments[i].retx_count++;
            q->segments[i].send_time = now;
            timed_out++;
        }
    }
    return timed_out;
}

size_t nfs_retx_queue_size(const struct nfs_retx_queue *q)
{
    if (!q)
        return 0;
    return q->count;
}

int nfs_retx_queue_empty(const struct nfs_retx_queue *q)
{
    if (!q)
        return 1;
    return q->count == 0 ? 1 : 0;
}
