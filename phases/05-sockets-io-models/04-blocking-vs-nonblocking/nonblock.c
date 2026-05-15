/*
 * nonblock.c -- Blocking vs Nonblocking I/O mock
 *
 * Educational mock that simulates blocking/nonblocking fd behavior
 * without actual system calls. Demonstrates the key concepts:
 * O_NONBLOCK, EAGAIN/EWOULDBLOCK, and retry patterns.
 */

#include "nonblock.h"
#include <string.h>

int nfs_nb_init(struct nfs_nb_ctx *ctx)
{
    if (!ctx) return NFS_NB_EINVAL;
    memset(ctx, 0, sizeof(*ctx));
    return NFS_NB_OK;
}

int nfs_nb_open(struct nfs_nb_ctx *ctx)
{
    if (!ctx) return NFS_NB_EINVAL;
    if (ctx->next_fd >= NFS_NB_MAX_FDS) return NFS_NB_EINVAL;

    int fd = ctx->next_fd++;
    struct nfs_nb_fd *f = &ctx->fds[fd];
    memset(f, 0, sizeof(*f));
    f->active = 1;
    f->nonblocking = 0;
    f->readable = 0;
    f->writable = 1;  /* default: writable */
    return fd;
}

int nfs_nb_close(struct nfs_nb_ctx *ctx, int fd)
{
    if (!ctx || fd < 0 || fd >= NFS_NB_MAX_FDS) return NFS_NB_EBADF;
    if (!ctx->fds[fd].active) return NFS_NB_EBADF;
    ctx->fds[fd].active = 0;
    return NFS_NB_OK;
}

int nfs_nb_set_nonblock(struct nfs_nb_ctx *ctx, int fd, int nonblock)
{
    if (!ctx || fd < 0 || fd >= NFS_NB_MAX_FDS) return NFS_NB_EBADF;
    if (!ctx->fds[fd].active) return NFS_NB_EBADF;
    ctx->fds[fd].nonblocking = nonblock ? 1 : 0;
    return NFS_NB_OK;
}

int nfs_nb_is_nonblock(const struct nfs_nb_ctx *ctx, int fd)
{
    if (!ctx || fd < 0 || fd >= NFS_NB_MAX_FDS) return -1;
    if (!ctx->fds[fd].active) return -1;
    return ctx->fds[fd].nonblocking;
}

int nfs_nb_inject_readable(struct nfs_nb_ctx *ctx, int fd,
                           const uint8_t *data, size_t len)
{
    if (!ctx || fd < 0 || fd >= NFS_NB_MAX_FDS) return NFS_NB_EBADF;
    if (!ctx->fds[fd].active) return NFS_NB_EBADF;
    if (!data && len > 0) return NFS_NB_EINVAL;

    struct nfs_nb_fd *f = &ctx->fds[fd];
    size_t space = NFS_NB_BUF_SIZE - f->read_avail;
    if (len > space) len = space;

    if (len > 0) {
        memcpy(f->read_buf + f->read_avail, data, len);
        f->read_avail += len;
        f->readable = 1;
    }

    return (int)len;
}

int nfs_nb_set_writable(struct nfs_nb_ctx *ctx, int fd, int writable)
{
    if (!ctx || fd < 0 || fd >= NFS_NB_MAX_FDS) return NFS_NB_EBADF;
    if (!ctx->fds[fd].active) return NFS_NB_EBADF;
    ctx->fds[fd].writable = writable ? 1 : 0;
    return NFS_NB_OK;
}

int nfs_nb_read(struct nfs_nb_ctx *ctx, int fd,
                uint8_t *buf, size_t max)
{
    if (!ctx || fd < 0 || fd >= NFS_NB_MAX_FDS) return NFS_NB_EBADF;
    if (!ctx->fds[fd].active) return NFS_NB_CLOSED;
    if (!buf || max == 0) return NFS_NB_EINVAL;

    struct nfs_nb_fd *f = &ctx->fds[fd];

    if (f->read_avail == 0) {
        /* No data available */
        return NFS_NB_EAGAIN;
    }

    /* Read up to max bytes */
    size_t to_read = f->read_avail < max ? f->read_avail : max;
    memcpy(buf, f->read_buf, to_read);

    /* Shift remaining data */
    size_t remaining = f->read_avail - to_read;
    if (remaining > 0) {
        memmove(f->read_buf, f->read_buf + to_read, remaining);
    }
    f->read_avail = remaining;
    f->readable = (remaining > 0) ? 1 : 0;
    f->read_total += to_read;

    return (int)to_read;
}

int nfs_nb_write(struct nfs_nb_ctx *ctx, int fd,
                 const uint8_t *buf, size_t len)
{
    if (!ctx || fd < 0 || fd >= NFS_NB_MAX_FDS) return NFS_NB_EBADF;
    if (!ctx->fds[fd].active) return NFS_NB_CLOSED;
    if (!buf && len > 0) return NFS_NB_EINVAL;
    if (len == 0) return 0;

    struct nfs_nb_fd *f = &ctx->fds[fd];

    if (!f->writable) {
        return NFS_NB_EAGAIN;
    }

    /* Mock: accept all bytes */
    f->write_total += len;
    return (int)len;
}

int nfs_nb_would_block(const struct nfs_nb_ctx *ctx, int fd)
{
    if (!ctx || fd < 0 || fd >= NFS_NB_MAX_FDS) return 0;
    if (!ctx->fds[fd].active) return 0;
    return ctx->fds[fd].nonblocking && ctx->fds[fd].read_avail == 0;
}

int nfs_nb_read_retry(struct nfs_nb_ctx *ctx, int fd,
                      uint8_t *buf, size_t max, int max_retries)
{
    if (!ctx || fd < 0 || fd >= NFS_NB_MAX_FDS) return NFS_NB_EBADF;

    for (int attempt = 0; attempt <= max_retries; attempt++) {
        int rc = nfs_nb_read(ctx, fd, buf, max);
        if (rc != NFS_NB_EAGAIN) return rc;
        /* In a real system, we would sleep or poll here */
    }
    return NFS_NB_EAGAIN;  /* all retries exhausted */
}
