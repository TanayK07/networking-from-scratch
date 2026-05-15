#include "nagle.h"

/* ---------------------------------------------------------------
 * Nagle's algorithm implementation.
 * --------------------------------------------------------------- */

void nfs_nagle_init(struct nfs_nagle *n, uint16_t mss)
{
    if (!n)
        return;
    n->enabled = 1;
    n->snd_una = 0;
    n->snd_nxt = 0;
    n->mss     = mss;
}

int nfs_nagle_can_send(const struct nfs_nagle *n, size_t data_len)
{
    if (!n)
        return 0;

    /* (c) Nagle disabled => always send */
    if (!n->enabled)
        return 1;

    /* (a) Full segment => send immediately */
    if (data_len >= n->mss)
        return 1;

    /* (b) No outstanding unacked data => send */
    if (n->snd_una == n->snd_nxt)
        return 1;

    /* Otherwise, buffer the data */
    return 0;
}

void nfs_nagle_enable(struct nfs_nagle *n)
{
    if (n)
        n->enabled = 1;
}

void nfs_nagle_disable(struct nfs_nagle *n)
{
    if (n)
        n->enabled = 0;
}

void nfs_nagle_sent(struct nfs_nagle *n, uint32_t nbytes)
{
    if (n)
        n->snd_nxt += nbytes;
}

void nfs_nagle_acked(struct nfs_nagle *n, uint32_t ack)
{
    if (n)
        n->snd_una = ack;
}

/* ---------------------------------------------------------------
 * Delayed ACK implementation.
 * --------------------------------------------------------------- */

void nfs_delayed_ack_init(struct nfs_delayed_ack *da, double delay)
{
    if (!da)
        return;
    da->pending   = 0;
    da->deadline  = 0.0;
    da->delay     = delay;
    da->seg_count = 0;
}

int nfs_delayed_ack_receive(struct nfs_delayed_ack *da, double now)
{
    if (!da)
        return 0;

    da->seg_count++;

    /* RFC 1122: ACK every second segment immediately */
    if (da->seg_count >= 2) {
        /* Signal immediate ACK */
        return 1;
    }

    /* First segment: start delayed ACK timer */
    da->pending  = 1;
    da->deadline = now + da->delay;
    return 0;
}

int nfs_delayed_ack_check(struct nfs_delayed_ack *da, double now)
{
    if (!da)
        return 0;

    if (da->pending && now >= da->deadline)
        return 1;

    return 0;
}

void nfs_delayed_ack_sent(struct nfs_delayed_ack *da)
{
    if (!da)
        return;
    da->pending   = 0;
    da->deadline  = 0.0;
    da->seg_count = 0;
}
