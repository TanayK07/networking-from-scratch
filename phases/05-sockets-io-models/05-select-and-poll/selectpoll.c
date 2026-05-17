/*
 * selectpoll.c -- select() and poll() educational mock
 *
 * Implements mock versions of fd_set manipulation, select(), and
 * poll() that consult an in-memory readiness table. This allows
 * testing multiplexing logic without actual system calls.
 */

#include "selectpoll.h"
#include <string.h>

/* ---- Context management ---- */

int nfs_sp_init(struct nfs_sp_ctx *ctx) {
    if (!ctx)
        return -1;
    memset(ctx, 0, sizeof(*ctx));
    return 0;
}

int nfs_sp_open(struct nfs_sp_ctx *ctx) {
    if (!ctx)
        return -1;
    if (ctx->next_fd >= NFS_SP_MAX_FDS)
        return -1;

    int fd = ctx->next_fd++;
    ctx->fds[fd].active = 1;
    ctx->fds[fd].ready_events = 0;
    return fd;
}

int nfs_sp_close(struct nfs_sp_ctx *ctx, int fd) {
    if (!ctx || fd < 0 || fd >= NFS_SP_MAX_FDS)
        return -1;
    if (!ctx->fds[fd].active)
        return -1;
    ctx->fds[fd].active = 0;
    ctx->fds[fd].ready_events = 0;
    return 0;
}

int nfs_sp_set_ready(struct nfs_sp_ctx *ctx, int fd, uint16_t events) {
    if (!ctx || fd < 0 || fd >= NFS_SP_MAX_FDS)
        return -1;
    if (!ctx->fds[fd].active)
        return -1;
    ctx->fds[fd].ready_events |= events;
    return 0;
}

int nfs_sp_clear_ready(struct nfs_sp_ctx *ctx, int fd, uint16_t events) {
    if (!ctx || fd < 0 || fd >= NFS_SP_MAX_FDS)
        return -1;
    if (!ctx->fds[fd].active)
        return -1;
    ctx->fds[fd].ready_events &= (uint16_t)~events;
    return 0;
}

int nfs_sp_get_ready(const struct nfs_sp_ctx *ctx, int fd) {
    if (!ctx || fd < 0 || fd >= NFS_SP_MAX_FDS)
        return -1;
    if (!ctx->fds[fd].active)
        return -1;
    return ctx->fds[fd].ready_events;
}

/* ---- fd_set operations ---- */

void nfs_fdset_zero(struct nfs_fdset *set) {
    if (set)
        set->bits = 0;
}

void nfs_fdset_set(struct nfs_fdset *set, int fd) {
    if (set && fd >= 0 && fd < NFS_SP_MAX_FDS)
        set->bits |= (1ULL << fd);
}

void nfs_fdset_clr(struct nfs_fdset *set, int fd) {
    if (set && fd >= 0 && fd < NFS_SP_MAX_FDS)
        set->bits &= ~(1ULL << fd);
}

int nfs_fdset_isset(const struct nfs_fdset *set, int fd) {
    if (!set || fd < 0 || fd >= NFS_SP_MAX_FDS)
        return 0;
    return (set->bits & (1ULL << fd)) ? 1 : 0;
}

int nfs_fdset_count(const struct nfs_fdset *set) {
    if (!set)
        return 0;
    /* popcount of bits */
    uint64_t v = set->bits;
    int count = 0;
    while (v) {
        count += (int)(v & 1);
        v >>= 1;
    }
    return count;
}

/* ---- Mock select() ---- */

int nfs_select(const struct nfs_sp_ctx *ctx, int nfds, struct nfs_fdset *readfds,
               struct nfs_fdset *writefds) {
    if (!ctx)
        return -1;
    if (nfds < 0 || nfds > NFS_SP_MAX_FDS)
        return -1;

    int ready_count = 0;

    struct nfs_fdset read_result, write_result;
    nfs_fdset_zero(&read_result);
    nfs_fdset_zero(&write_result);

    for (int fd = 0; fd < nfds; fd++) {
        int is_ready = 0;

        /* Check readfds */
        if (readfds && nfs_fdset_isset(readfds, fd)) {
            if (!ctx->fds[fd].active) {
                /* Bad fd -- treat as error, skip */
                continue;
            }
            if (ctx->fds[fd].ready_events & NFS_POLLIN) {
                nfs_fdset_set(&read_result, fd);
                is_ready = 1;
            }
        }

        /* Check writefds */
        if (writefds && nfs_fdset_isset(writefds, fd)) {
            if (!ctx->fds[fd].active)
                continue;
            if (ctx->fds[fd].ready_events & NFS_POLLOUT) {
                nfs_fdset_set(&write_result, fd);
                is_ready = 1;
            }
        }

        if (is_ready)
            ready_count++;
    }

    /* Replace input sets with result sets */
    if (readfds)
        *readfds = read_result;
    if (writefds)
        *writefds = write_result;

    return ready_count;
}

/* ---- Mock poll() ---- */

void nfs_pollfd_init(struct nfs_pollfd *pfd, int fd, uint16_t events) {
    if (pfd) {
        pfd->fd = fd;
        pfd->events = events;
        pfd->revents = 0;
    }
}

int nfs_poll(const struct nfs_sp_ctx *ctx, struct nfs_pollfd *pfds, int nfds) {
    if (!ctx || !pfds || nfds < 0)
        return -1;

    int ready_count = 0;

    for (int i = 0; i < nfds; i++) {
        pfds[i].revents = 0;
        int fd = pfds[i].fd;

        if (fd < 0)
            continue;

        if (fd >= NFS_SP_MAX_FDS || !ctx->fds[fd].active) {
            pfds[i].revents = NFS_POLLNVAL;
            ready_count++;
            continue;
        }

        uint16_t ready = ctx->fds[fd].ready_events;

        /* Report requested events that are ready */
        pfds[i].revents = (uint16_t)(ready & pfds[i].events);

        /* POLLERR and POLLHUP are always reported, even if not requested */
        pfds[i].revents |= (uint16_t)(ready & (NFS_POLLERR | NFS_POLLHUP));

        if (pfds[i].revents)
            ready_count++;
    }

    return ready_count;
}

int nfs_poll_check_ready(const struct nfs_pollfd *pfd, uint16_t event) {
    if (!pfd)
        return 0;
    return (pfd->revents & event) ? 1 : 0;
}

/* ---- Event name helper ---- */

const char *nfs_poll_event_name(uint16_t event) {
    switch (event) {
    case NFS_POLLIN:
        return "POLLIN";
    case NFS_POLLOUT:
        return "POLLOUT";
    case NFS_POLLERR:
        return "POLLERR";
    case NFS_POLLHUP:
        return "POLLHUP";
    case NFS_POLLNVAL:
        return "POLLNVAL";
    default:
        return "UNKNOWN";
    }
}
