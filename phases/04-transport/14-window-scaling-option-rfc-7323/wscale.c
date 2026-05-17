#include "wscale.h"
#include <stdio.h>

uint32_t nfs_wscale_apply(uint16_t window, uint8_t shift) {
    if (shift > NFS_WSCALE_MAX)
        shift = NFS_WSCALE_MAX;
    return (uint32_t)window << shift;
}

uint16_t nfs_wscale_compress(uint32_t real_window, uint8_t shift) {
    if (shift > NFS_WSCALE_MAX)
        shift = NFS_WSCALE_MAX;
    uint32_t val = real_window >> shift;
    if (val > 0xFFFF)
        return 0xFFFF;
    return (uint16_t)val;
}

uint8_t nfs_wscale_compute(uint32_t desired_window) {
    /* Find minimum shift such that desired_window >> shift <= 0xFFFF.
     * Equivalently, find shift such that desired_window fits in
     * 16 + shift bits. */
    if (desired_window <= 0xFFFF)
        return 0;

    uint8_t shift = 0;
    uint32_t w = desired_window;
    while (w > 0xFFFF && shift < NFS_WSCALE_MAX) {
        w >>= 1;
        shift++;
    }
    return shift;
}

int nfs_wscale_build_option(uint8_t shift, uint8_t *out, size_t out_sz) {
    if (!out || out_sz < 3)
        return -1;
    if (shift > NFS_WSCALE_MAX)
        return -1;

    out[0] = NFS_WSCALE_OPT_KIND; /* kind = 3 */
    out[1] = NFS_WSCALE_OPT_LEN;  /* len  = 3 */
    out[2] = shift;
    return 3;
}

int nfs_wscale_parse_option(const uint8_t *data, size_t len, uint8_t *shift_out) {
    if (!data || !shift_out || len < 3)
        return -1;
    if (data[0] != NFS_WSCALE_OPT_KIND)
        return -1;
    if (data[1] != NFS_WSCALE_OPT_LEN)
        return -1;
    *shift_out = data[2];
    return 0;
}

int nfs_wscale_valid(uint8_t shift) {
    return (shift <= NFS_WSCALE_MAX) ? 1 : 0;
}

void nfs_wscale_format(uint8_t shift, char *buf, size_t sz) {
    if (!buf || sz == 0)
        return;
    if (shift > NFS_WSCALE_MAX) {
        snprintf(buf, sz, "WScale=%u (INVALID, max is %d)", shift, NFS_WSCALE_MAX);
        return;
    }
    uint32_t mult = 1u << shift;
    snprintf(buf, sz, "WScale=%u (multiply by %u)", shift, mult);
}
