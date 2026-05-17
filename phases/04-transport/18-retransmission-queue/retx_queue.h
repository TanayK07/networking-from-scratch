#ifndef NFS_RETX_QUEUE_H
#define NFS_RETX_QUEUE_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Retransmission queue: manages TCP segments awaiting ACK.
 *
 * Each unacknowledged segment is stored with its sequence number,
 * payload, send timestamp, and retransmission count.  On cumulative
 * ACK, all segments with seq < ack_num are removed.  On timeout,
 * segments whose send_time is older than RTO are marked for
 * retransmission.
 * --------------------------------------------------------------- */

#define NFS_RETX_MSS 1460

struct nfs_retx_seg {
    uint32_t seq;
    uint8_t data[NFS_RETX_MSS];
    size_t data_len;
    double send_time;
    int retx_count;
};

struct nfs_retx_queue {
    struct nfs_retx_seg *segments;
    size_t count;
    size_t capacity;
};

/* Initialise with a given capacity (max segments). */
void nfs_retx_queue_init(struct nfs_retx_queue *q, size_t capacity);

/* Free internal storage. */
void nfs_retx_queue_free(struct nfs_retx_queue *q);

/* Enqueue a segment.  Return 0 on success, -1 if full or invalid. */
int nfs_retx_queue_push(struct nfs_retx_queue *q, uint32_t seq, const uint8_t *data, size_t len,
                        double send_time);

/* Cumulative ACK: remove all segments with seq < ack_num.
 * Return count of segments removed. */
int nfs_retx_queue_ack(struct nfs_retx_queue *q, uint32_t ack_num);

/* Find segment by exact sequence number.  NULL if not found. */
struct nfs_retx_seg *nfs_retx_queue_find(struct nfs_retx_queue *q, uint32_t seq);

/* Check for timeouts: for each segment where (now - send_time) >= rto,
 * increment retx_count and update send_time to now.
 * Return count of timed-out segments. */
int nfs_retx_queue_timeout(struct nfs_retx_queue *q, double now, double rto);

/* Current number of segments in the queue. */
size_t nfs_retx_queue_size(const struct nfs_retx_queue *q);

/* Return 1 if queue is empty, 0 otherwise. */
int nfs_retx_queue_empty(const struct nfs_retx_queue *q);

#endif /* NFS_RETX_QUEUE_H */
