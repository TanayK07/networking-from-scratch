#include "rdt.h"
#include <string.h>

void nfs_rdt_sender_init(struct nfs_rdt_sender *s, uint32_t window_size)
{
    memset(s, 0, sizeof(*s));
    s->window_size = (window_size > 0) ? window_size : 1;
}

void nfs_rdt_sender_free(struct nfs_rdt_sender *s)
{
    memset(s, 0, sizeof(*s));
}

int nfs_rdt_send(struct nfs_rdt_sender *s, const uint8_t *data, size_t len)
{
    if (!data || len == 0 || len > NFS_RDT_MAX_DATA)
        return -1;

    /* Check if we can send within the window */
    uint32_t in_flight = s->count - s->base;
    if (in_flight >= s->window_size)
        return -1;

    /* Check buffer capacity */
    if (s->count >= NFS_RDT_MAX_SEGS)
        return -1;

    struct nfs_rdt_segment *seg = &s->segments[s->count];
    seg->seq = s->next_seq;
    memcpy(seg->data, data, len);
    seg->data_len = len;
    seg->acked = 0;

    uint32_t assigned_seq = s->next_seq;
    s->next_seq++;
    s->count++;

    return (int)assigned_seq;
}

int nfs_rdt_ack(struct nfs_rdt_sender *s, uint32_t ack_num)
{
    int newly_acked = 0;

    /* Cumulative ACK: all segments with seq < ack_num are acknowledged */
    while (s->base < s->count) {
        struct nfs_rdt_segment *seg = &s->segments[s->base];
        if (seg->seq < ack_num) {
            if (!seg->acked) {
                seg->acked = 1;
                newly_acked++;
            }
            s->base++;
        } else {
            break;
        }
    }

    return newly_acked;
}

int nfs_rdt_timeout(struct nfs_rdt_sender *s)
{
    int retransmitted = 0;

    /* Go-Back-N: retransmit all unacked segments in the window */
    uint32_t end = s->base + s->window_size;
    if (end > s->count)
        end = s->count;

    for (uint32_t i = s->base; i < end; i++) {
        if (!s->segments[i].acked) {
            retransmitted++;
            s->retransmit_count++;
        }
    }

    return retransmitted;
}

int nfs_rdt_in_flight(const struct nfs_rdt_sender *s)
{
    return (int)(s->count - s->base);
}

void nfs_rdt_receiver_init(struct nfs_rdt_receiver *r)
{
    memset(r, 0, sizeof(*r));
}

int nfs_rdt_receive(struct nfs_rdt_receiver *r,
                    const struct nfs_rdt_segment *seg,
                    uint32_t *ack_out)
{
    if (!seg || !ack_out)
        return -1;

    if (seg->seq == r->expected_seq) {
        /* In-order: accept data */
        size_t space = NFS_RDT_RECV_BUF - r->recv_len;
        size_t to_copy = (seg->data_len < space) ? seg->data_len : space;
        if (to_copy > 0) {
            memcpy(r->recv_buf + r->recv_len, seg->data, to_copy);
            r->recv_len += to_copy;
        }
        r->expected_seq++;
        *ack_out = r->expected_seq;
        return 0;
    }

    /* Out-of-order: send duplicate ACK for what we still expect */
    *ack_out = r->expected_seq;
    return -1;
}
