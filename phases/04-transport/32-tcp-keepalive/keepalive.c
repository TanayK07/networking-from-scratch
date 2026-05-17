#include "keepalive.h"
#include <stdio.h>
#include <string.h>

void nfs_keepalive_init(struct nfs_keepalive *ka, double idle_timeout, double interval,
                        int max_probes) {
    memset(ka, 0, sizeof(*ka));
    ka->idle_timeout = idle_timeout;
    ka->interval = interval;
    ka->max_probes = max_probes;
    ka->last_data_time = 0.0;
    ka->probes_sent = 0;
    ka->enabled = 0;
}

void nfs_keepalive_enable(struct nfs_keepalive *ka) {
    ka->enabled = 1;
}

void nfs_keepalive_disable(struct nfs_keepalive *ka) {
    ka->enabled = 0;
}

void nfs_keepalive_data_received(struct nfs_keepalive *ka, double now) {
    ka->last_data_time = now;
    ka->probes_sent = 0;
}

int nfs_keepalive_check(struct nfs_keepalive *ka, double now) {
    if (!ka->enabled)
        return 0;

    /* Check if all probes exhausted */
    if (ka->probes_sent >= ka->max_probes)
        return -1;

    double idle_duration = now - ka->last_data_time;

    if (ka->probes_sent == 0) {
        /* No probes sent yet: check if idle timeout has elapsed */
        if (idle_duration >= ka->idle_timeout) {
            ka->probes_sent++;
            return 1;
        }
    } else {
        /* Already probing: check if interval has elapsed since
         * the probe phase started.  The first probe fires at
         * idle_timeout, subsequent probes at idle_timeout + n*interval. */
        double probe_threshold = ka->idle_timeout + (double)ka->probes_sent * ka->interval;
        if (idle_duration >= probe_threshold) {
            ka->probes_sent++;
            /* Re-check: did we just exhaust all probes? */
            if (ka->probes_sent >= ka->max_probes)
                return -1;
            return 1;
        }
    }

    return 0;
}

int nfs_keepalive_is_dead(const struct nfs_keepalive *ka) {
    return (ka->probes_sent >= ka->max_probes) ? 1 : 0;
}

int nfs_keepalive_build_probe(uint32_t seq_minus_one, uint8_t *out, size_t out_sz) {
    if (!out || out_sz < NFS_TCP_HDR_MIN)
        return -1;

    memset(out, 0, NFS_TCP_HDR_MIN);

    /* TCP header fields (network byte order):
     * Bytes 0-1:  Source port (0 = placeholder)
     * Bytes 2-3:  Destination port (0 = placeholder)
     * Bytes 4-7:  Sequence number = seq_minus_one
     * Bytes 8-11: Acknowledgment number (0)
     * Byte  12:   Data offset (5 << 4 = 0x50 for 20 bytes) + reserved
     * Byte  13:   Flags (ACK = 0x10)
     * Bytes 14-15: Window (0)
     * Bytes 16-17: Checksum (0, computed later)
     * Bytes 18-19: Urgent pointer (0)
     */

    /* Sequence number (big-endian) */
    out[4] = (uint8_t)(seq_minus_one >> 24);
    out[5] = (uint8_t)(seq_minus_one >> 16);
    out[6] = (uint8_t)(seq_minus_one >> 8);
    out[7] = (uint8_t)(seq_minus_one);

    /* Data offset = 5 (20 bytes / 4), shifted to upper nibble */
    out[12] = 0x50;

    /* Flags: ACK */
    out[13] = 0x10;

    return NFS_TCP_HDR_MIN;
}

void nfs_keepalive_format(const struct nfs_keepalive *ka, char *buf, size_t sz) {
    if (!buf || sz == 0)
        return;
    snprintf(buf, sz,
             "keepalive %s: idle=%.0fs interval=%.0fs "
             "probes=%d/%d last_data=%.1fs",
             ka->enabled ? "enabled" : "disabled", ka->idle_timeout, ka->interval, ka->probes_sent,
             ka->max_probes, ka->last_data_time);
}
