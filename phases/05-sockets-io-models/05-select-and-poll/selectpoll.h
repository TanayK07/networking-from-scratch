#ifndef NFS_SELECTPOLL_H
#define NFS_SELECTPOLL_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * select() and poll() -- Educational Mock
 *
 * Models the I/O multiplexing APIs without actual system calls.
 *
 * fd_set manipulation:
 *   FD_ZERO, FD_SET, FD_CLR, FD_ISSET
 *
 * select(): nfds, readfds, writefds, exceptfds, timeout
 *
 * poll(): struct pollfd (fd, events, revents)
 *   POLLIN, POLLOUT, POLLERR, POLLHUP, POLLNVAL
 *
 * The mock tracks a readiness table: for each fd, which events
 * are currently ready. The mock select/poll consult this table.
 * --------------------------------------------------------------- */

#define NFS_SP_MAX_FDS 64

/* Event flags (compatible with real poll flags) */
#define NFS_POLLIN   0x0001
#define NFS_POLLOUT  0x0004
#define NFS_POLLERR  0x0008
#define NFS_POLLHUP  0x0010
#define NFS_POLLNVAL 0x0020

/* Mock fd_set: a simple bitmask. */
struct nfs_fdset {
    uint64_t bits; /* supports up to 64 fds */
};

/* Mock pollfd structure. */
struct nfs_pollfd {
    int fd;
    uint16_t events;  /* requested events */
    uint16_t revents; /* returned events */
};

/* Per-fd readiness state. */
struct nfs_sp_fd {
    int active;
    uint16_t ready_events; /* currently ready events */
};

/* Multiplexer context. */
struct nfs_sp_ctx {
    struct nfs_sp_fd fds[NFS_SP_MAX_FDS];
    int next_fd;
};

/* ---- Context management ---- */

int nfs_sp_init(struct nfs_sp_ctx *ctx);
int nfs_sp_open(struct nfs_sp_ctx *ctx);
int nfs_sp_close(struct nfs_sp_ctx *ctx, int fd);

/* Set readiness events on an fd (simulates kernel notification). */
int nfs_sp_set_ready(struct nfs_sp_ctx *ctx, int fd, uint16_t events);

/* Clear readiness events on an fd. */
int nfs_sp_clear_ready(struct nfs_sp_ctx *ctx, int fd, uint16_t events);

/* Query current readiness events. Returns events mask or -1. */
int nfs_sp_get_ready(const struct nfs_sp_ctx *ctx, int fd);

/* ---- fd_set operations ---- */

void nfs_fdset_zero(struct nfs_fdset *set);
void nfs_fdset_set(struct nfs_fdset *set, int fd);
void nfs_fdset_clr(struct nfs_fdset *set, int fd);
int nfs_fdset_isset(const struct nfs_fdset *set, int fd);
int nfs_fdset_count(const struct nfs_fdset *set);

/* ---- Mock select() ---- */

/* Check readfds and writefds against context readiness.
 * On return, readfds/writefds are modified to only contain ready fds.
 * Returns number of ready fds, or -1 on error.
 * nfds: highest fd + 1.
 * readfds/writefds: may be NULL. */
int nfs_select(const struct nfs_sp_ctx *ctx, int nfds, struct nfs_fdset *readfds,
               struct nfs_fdset *writefds);

/* ---- Mock poll() ---- */

/* Initialize a pollfd entry. */
void nfs_pollfd_init(struct nfs_pollfd *pfd, int fd, uint16_t events);

/* Check readiness for an array of pollfds.
 * Updates revents for each entry.
 * Returns number of fds with non-zero revents, or -1 on error. */
int nfs_poll(const struct nfs_sp_ctx *ctx, struct nfs_pollfd *pfds, int nfds);

/* Check if a specific event is set in revents. */
int nfs_poll_check_ready(const struct nfs_pollfd *pfd, uint16_t event);

/* ---- Event name helpers ---- */

const char *nfs_poll_event_name(uint16_t event);

#endif /* NFS_SELECTPOLL_H */
