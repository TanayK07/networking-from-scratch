#ifndef NFS_ECHO_H
#define NFS_ECHO_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * UDP Echo Server (RFC 862)
 *
 * The Echo protocol is trivially simple: any data received by the
 * server is sent back unchanged. This implementation models the
 * echo protocol as pure logic -- no actual sockets -- so it can
 * be tested deterministically.
 *
 * Max payload: 65507 bytes (UDP max: 65535 - 8 UDP header - 20 IP)
 * --------------------------------------------------------------- */

#define NFS_ECHO_MAX_PAYLOAD 65507

/* Statistics tracked by the echo server. */
struct nfs_echo_stats {
    uint64_t packets_processed;
    uint64_t bytes_echoed;
    uint64_t errors;
};

/* Opaque echo server context. */
struct nfs_echo_server {
    struct nfs_echo_stats stats;
    int running;          /* 1 = initialized, 0 = not */
    uint16_t max_payload; /* configurable max payload */
};

/* Initialize an echo server context.
 * max_payload: maximum accepted datagram payload (0 = use default 65507).
 * Returns 0 on success, -1 on error. */
int nfs_echo_init(struct nfs_echo_server *srv, uint16_t max_payload);

/* Process one datagram: copy input to output unchanged.
 * in/in_len:   received datagram payload
 * out/out_max: buffer for echo response
 * out_len:     on success, set to number of bytes written to out
 * Returns 0 on success, -1 on error (NULL ptrs, too large, etc). */
int nfs_echo_process(struct nfs_echo_server *srv, const uint8_t *in, size_t in_len, uint8_t *out,
                     size_t out_max, size_t *out_len);

/* Retrieve accumulated statistics.
 * Returns 0 on success, -1 if srv or out is NULL. */
int nfs_echo_stats(const struct nfs_echo_server *srv, struct nfs_echo_stats *out);

/* Reset statistics to zero. Returns 0 on success. */
int nfs_echo_reset_stats(struct nfs_echo_server *srv);

/* Shut down the echo server context. Returns 0 on success. */
int nfs_echo_shutdown(struct nfs_echo_server *srv);

#endif /* NFS_ECHO_H */
