#ifndef NFS_NONBLOCK_H
#define NFS_NONBLOCK_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Blocking vs Nonblocking I/O (educational mock)
 *
 * Since we cannot test actual sockets in a pure unit-test
 * environment, this module provides a mock file descriptor state
 * machine that simulates blocking/nonblocking behavior.
 *
 * Key concepts modeled:
 *   - O_NONBLOCK flag and fcntl() behavior
 *   - EAGAIN/EWOULDBLOCK error semantics
 *   - Retry loop patterns for nonblocking I/O
 *   - Readiness state transitions
 *
 * The mock tracks per-fd state:
 *   - blocking/nonblocking mode
 *   - readable/writable readiness
 *   - pending data buffer
 * --------------------------------------------------------------- */

#define NFS_NB_MAX_FDS    64
#define NFS_NB_BUF_SIZE   1024

/* Error codes returned by mock operations */
#define NFS_NB_OK          0
#define NFS_NB_EAGAIN     -1   /* would block */
#define NFS_NB_EBADF      -2   /* bad fd */
#define NFS_NB_EINVAL     -3   /* invalid argument */
#define NFS_NB_CLOSED     -4   /* fd closed / EOF */

/* Mock fd state */
struct nfs_nb_fd {
    int      active;        /* 1 = fd is open */
    int      nonblocking;   /* 1 = O_NONBLOCK set */
    int      readable;      /* 1 = data ready to read */
    int      writable;      /* 1 = can write without blocking */
    uint8_t  read_buf[NFS_NB_BUF_SIZE];
    size_t   read_avail;    /* bytes available to read */
    uint64_t read_total;    /* total bytes read from this fd */
    uint64_t write_total;   /* total bytes written to this fd */
};

/* Mock I/O context. */
struct nfs_nb_ctx {
    struct nfs_nb_fd fds[NFS_NB_MAX_FDS];
    int              next_fd;   /* next fd to allocate */
};

/* Initialize the mock context. Returns 0 on success. */
int nfs_nb_init(struct nfs_nb_ctx *ctx);

/* Open a new mock fd. Returns the fd number, or -1 on error. */
int nfs_nb_open(struct nfs_nb_ctx *ctx);

/* Close a mock fd. Returns 0 on success. */
int nfs_nb_close(struct nfs_nb_ctx *ctx, int fd);

/* Set nonblocking mode on fd. nonblock: 1=set, 0=clear.
 * Mimics: fcntl(fd, F_SETFL, flags | O_NONBLOCK)
 * Returns 0 on success. */
int nfs_nb_set_nonblock(struct nfs_nb_ctx *ctx, int fd, int nonblock);

/* Query whether fd is in nonblocking mode. Returns 1 or 0, or -1 on error. */
int nfs_nb_is_nonblock(const struct nfs_nb_ctx *ctx, int fd);

/* Inject data into the fd's read buffer (simulates network arrival).
 * Returns bytes injected, or negative error code. */
int nfs_nb_inject_readable(struct nfs_nb_ctx *ctx, int fd,
                           const uint8_t *data, size_t len);

/* Set the writable readiness of an fd (simulates send buffer space). */
int nfs_nb_set_writable(struct nfs_nb_ctx *ctx, int fd, int writable);

/* Mock read: attempts to read up to max bytes from fd.
 * Blocking mode: if no data, returns NFS_NB_EAGAIN (simulates block).
 *   (In a real system, blocking read would sleep. We cannot block,
 *    so we return EAGAIN to indicate "would have blocked".)
 * Nonblocking mode: if no data, returns NFS_NB_EAGAIN immediately.
 * On success, returns bytes read (>0). */
int nfs_nb_read(struct nfs_nb_ctx *ctx, int fd,
                uint8_t *buf, size_t max);

/* Mock write: attempts to write len bytes to fd.
 * If not writable and nonblocking: returns NFS_NB_EAGAIN.
 * On success, returns bytes written. */
int nfs_nb_write(struct nfs_nb_ctx *ctx, int fd,
                 const uint8_t *buf, size_t len);

/* Check if an operation would block on this fd.
 * Returns 1 if the fd is nonblocking and has no data, 0 otherwise. */
int nfs_nb_would_block(const struct nfs_nb_ctx *ctx, int fd);

/* Retry loop helper: attempt a read with retry logic.
 * max_retries: maximum number of EAGAIN retries before giving up.
 * Returns bytes read on success, or negative error code. */
int nfs_nb_read_retry(struct nfs_nb_ctx *ctx, int fd,
                      uint8_t *buf, size_t max, int max_retries);

#endif /* NFS_NONBLOCK_H */
