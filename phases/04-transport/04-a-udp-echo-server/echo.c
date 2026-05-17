/*
 * echo.c -- UDP Echo Server (RFC 862) protocol logic
 *
 * Pure protocol implementation: receives a datagram payload and
 * produces an identical copy as the response. Tracks statistics
 * for packets processed, bytes echoed, and errors.
 */

#include "echo.h"
#include <string.h>

int nfs_echo_init(struct nfs_echo_server *srv, uint16_t max_payload) {
    if (!srv)
        return -1;

    memset(srv, 0, sizeof(*srv));
    srv->max_payload = max_payload ? max_payload : NFS_ECHO_MAX_PAYLOAD;
    srv->running = 1;
    return 0;
}

int nfs_echo_process(struct nfs_echo_server *srv, const uint8_t *in, size_t in_len, uint8_t *out,
                     size_t out_max, size_t *out_len) {
    if (!srv || !srv->running)
        return -1;

    /* NULL input pointer with non-zero length is an error */
    if (!in && in_len > 0) {
        srv->stats.errors++;
        return -1;
    }

    /* NULL output pointers are errors */
    if (!out || !out_len) {
        srv->stats.errors++;
        return -1;
    }

    /* Validate payload size */
    if (in_len > srv->max_payload) {
        srv->stats.errors++;
        return -1;
    }

    /* Output buffer too small */
    if (out_max < in_len) {
        srv->stats.errors++;
        return -1;
    }

    /* Echo: copy input to output unchanged */
    if (in_len > 0) {
        memcpy(out, in, in_len);
    }
    *out_len = in_len;

    srv->stats.packets_processed++;
    srv->stats.bytes_echoed += in_len;
    return 0;
}

int nfs_echo_stats(const struct nfs_echo_server *srv, struct nfs_echo_stats *out) {
    if (!srv || !out)
        return -1;
    *out = srv->stats;
    return 0;
}

int nfs_echo_reset_stats(struct nfs_echo_server *srv) {
    if (!srv)
        return -1;
    memset(&srv->stats, 0, sizeof(srv->stats));
    return 0;
}

int nfs_echo_shutdown(struct nfs_echo_server *srv) {
    if (!srv)
        return -1;
    srv->running = 0;
    return 0;
}
