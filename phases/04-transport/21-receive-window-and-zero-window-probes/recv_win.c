#include "recv_win.h"
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Receive window implementation.
 *
 * The buffer is a simple flat array.  Data is appended at offset
 * 'used' and consumed from offset 0 (with a memmove to compact).
 * --------------------------------------------------------------- */

void nfs_recv_window_init(struct nfs_recv_window *w, size_t capacity)
{
    if (!w)
        return;
    w->buffer   = calloc(1, capacity);
    w->capacity = capacity;
    w->used     = 0;
    w->rcv_nxt  = 0;
}

void nfs_recv_window_free(struct nfs_recv_window *w)
{
    if (!w)
        return;
    free(w->buffer);
    w->buffer   = NULL;
    w->capacity = 0;
    w->used     = 0;
}

uint32_t nfs_recv_window_advertised(const struct nfs_recv_window *w)
{
    if (!w)
        return 0;
    return (uint32_t)(w->capacity - w->used);
}

int nfs_recv_window_receive(struct nfs_recv_window *w, uint32_t seq,
                            const uint8_t *data, size_t len)
{
    if (!w || !w->buffer)
        return -1;

    /* Must be in-order delivery */
    if (seq != w->rcv_nxt)
        return -1;

    if (len == 0)
        return 0;

    if (!data)
        return -1;

    /* How much can we accept? */
    size_t space = w->capacity - w->used;
    if (space == 0)
        return 0;

    size_t accept = len < space ? len : space;
    memcpy(w->buffer + w->used, data, accept);
    w->used    += accept;
    w->rcv_nxt += (uint32_t)accept;

    return (int)accept;
}

size_t nfs_recv_window_read(struct nfs_recv_window *w, uint8_t *out,
                            size_t max)
{
    if (!w || !w->buffer || !out)
        return 0;

    size_t to_read = w->used < max ? w->used : max;
    if (to_read == 0)
        return 0;

    memcpy(out, w->buffer, to_read);

    /* Compact the buffer */
    size_t remaining = w->used - to_read;
    if (remaining > 0)
        memmove(w->buffer, w->buffer + to_read, remaining);
    w->used = remaining;

    return to_read;
}

int nfs_recv_window_is_zero(const struct nfs_recv_window *w)
{
    if (!w)
        return 1;
    return (w->capacity - w->used) == 0 ? 1 : 0;
}

/* ---------------------------------------------------------------
 * Zero-window probe timer.
 * --------------------------------------------------------------- */

void nfs_zwp_init(struct nfs_zwp *z, double interval)
{
    if (!z)
        return;
    z->active          = 0;
    z->interval        = interval;
    z->last_probe_time = 0.0;
    z->probe_count     = 0;
}

int nfs_zwp_start(struct nfs_zwp *z, double now)
{
    if (!z)
        return -1;
    z->active          = 1;
    z->last_probe_time = now;
    z->probe_count     = 0;
    return 0;
}

int nfs_zwp_check(struct nfs_zwp *z, double now)
{
    if (!z || !z->active)
        return 0;

    if ((now - z->last_probe_time) >= z->interval) {
        z->probe_count++;
        z->last_probe_time = now;
        return 1;
    }
    return 0;
}

void nfs_zwp_stop(struct nfs_zwp *z)
{
    if (!z)
        return;
    z->active          = 0;
    z->probe_count     = 0;
    z->last_probe_time = 0.0;
}
