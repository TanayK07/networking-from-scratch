#ifndef NFS_KEEPALIVE_H
#define NFS_KEEPALIVE_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP Keepalive (RFC 1122, Section 4.2.3.6)
 *
 * TCP keepalive probes detect dead connections by sending periodic
 * segments when a connection has been idle.
 *
 * Linux defaults:
 *   tcp_keepalive_time     = 7200 seconds (2 hours idle before probing)
 *   tcp_keepalive_intvl    = 75 seconds (between probes)
 *   tcp_keepalive_probes   = 9 (max probes before declaring dead)
 *
 * A keepalive probe is a TCP segment with:
 *   seq = snd_una - 1, len = 0  (or 1 byte of garbage)
 * This forces the peer to ACK with its current state.
 * --------------------------------------------------------------- */

#define NFS_KA_DEFAULT_IDLE     7200.0   /* seconds */
#define NFS_KA_DEFAULT_INTERVAL 75.0     /* seconds */
#define NFS_KA_DEFAULT_PROBES   9

/* Minimum TCP header size (no options) */
#define NFS_TCP_HDR_MIN         20

struct nfs_keepalive {
    double idle_timeout;    /* seconds idle before first probe */
    double interval;        /* seconds between probes */
    int    max_probes;      /* max probes before declaring dead */
    double last_data_time;  /* timestamp of last data activity */
    int    probes_sent;     /* probes sent since last data */
    int    enabled;         /* 1 = enabled, 0 = disabled */
};

/* Initialize with given parameters. Disabled by default. */
void nfs_keepalive_init(struct nfs_keepalive *ka,
                        double idle_timeout,
                        double interval,
                        int max_probes);

/* Enable keepalive on this connection. */
void nfs_keepalive_enable(struct nfs_keepalive *ka);

/* Disable keepalive on this connection. */
void nfs_keepalive_disable(struct nfs_keepalive *ka);

/* Called when data is received on the connection.
 * Resets the idle timer and probe count. */
void nfs_keepalive_data_received(struct nfs_keepalive *ka, double now);

/* Check if we need to send a probe at time `now`.
 * Returns:
 *    0 = no action needed
 *    1 = send a keepalive probe (probes_sent incremented)
 *   -1 = connection is dead (all probes exhausted) */
int nfs_keepalive_check(struct nfs_keepalive *ka, double now);

/* Returns 1 if the connection should be considered dead. */
int nfs_keepalive_is_dead(const struct nfs_keepalive *ka);

/* Build a keepalive probe TCP segment.
 * The probe has seq = seq_minus_one (snd_una - 1), no payload.
 * Writes a minimal 20-byte TCP header into out[].
 * Returns 20 on success, -1 if out_sz < 20. */
int nfs_keepalive_build_probe(uint32_t seq_minus_one,
                              uint8_t *out, size_t out_sz);

/* Format keepalive state into buf. */
void nfs_keepalive_format(const struct nfs_keepalive *ka,
                          char *buf, size_t sz);

#endif /* NFS_KEEPALIVE_H */
