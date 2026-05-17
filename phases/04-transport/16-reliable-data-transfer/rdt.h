#ifndef NFS_RDT_H
#define NFS_RDT_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Reliable Data Transfer (RDT) — Stop-and-Wait & Go-Back-N
 *
 * A simulated reliable data transfer layer (no real networking).
 * The sender buffers segments, sends within a sliding window,
 * processes cumulative ACKs, and retransmits on timeout.
 * The receiver accepts in-order segments and generates ACKs.
 * --------------------------------------------------------------- */

#define NFS_RDT_MAX_DATA 1024  /* max payload per segment */
#define NFS_RDT_MAX_SEGS 256   /* max buffered segments */
#define NFS_RDT_RECV_BUF 65536 /* receiver reassembly buffer */

struct nfs_rdt_segment {
    uint32_t seq;
    uint8_t data[NFS_RDT_MAX_DATA];
    size_t data_len;
    int acked;
};

struct nfs_rdt_sender {
    struct nfs_rdt_segment segments[NFS_RDT_MAX_SEGS];
    uint32_t count;            /* total segments buffered */
    uint32_t base;             /* index of oldest unacked segment */
    uint32_t next_seq;         /* next sequence number to assign */
    uint32_t window_size;      /* max segments in flight */
    uint32_t retransmit_count; /* total retransmissions */
};

struct nfs_rdt_receiver {
    uint32_t expected_seq;
    uint8_t recv_buf[NFS_RDT_RECV_BUF];
    size_t recv_len;
};

/* Initialize sender with a given window size (1 = stop-and-wait). */
void nfs_rdt_sender_init(struct nfs_rdt_sender *s, uint32_t window_size);

/* Free/reset sender state. */
void nfs_rdt_sender_free(struct nfs_rdt_sender *s);

/* Buffer a segment for sending.
 * Returns the assigned sequence number, or -1 if the window is full. */
int nfs_rdt_send(struct nfs_rdt_sender *s, const uint8_t *data, size_t len);

/* Process a cumulative ACK.
 * All segments with seq < ack_num are marked acknowledged.
 * Returns the number of segments newly acked. */
int nfs_rdt_ack(struct nfs_rdt_sender *s, uint32_t ack_num);

/* Simulate a timeout: retransmit all unacked segments in the current
 * window (Go-Back-N behavior).
 * Returns the number of segments retransmitted. */
int nfs_rdt_timeout(struct nfs_rdt_sender *s);

/* Return the number of unacked (in-flight) segments. */
int nfs_rdt_in_flight(const struct nfs_rdt_sender *s);

/* Initialize receiver. */
void nfs_rdt_receiver_init(struct nfs_rdt_receiver *r);

/* Process a received segment.
 * If seq == expected_seq, accept data and set *ack_out = expected_seq + 1.
 * Otherwise, send duplicate ACK: *ack_out = expected_seq.
 * Returns 0 if accepted, -1 if out-of-order. */
int nfs_rdt_receive(struct nfs_rdt_receiver *r, const struct nfs_rdt_segment *seg,
                    uint32_t *ack_out);

#endif /* NFS_RDT_H */
