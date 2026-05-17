#include "iovec.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Total length across all iovecs.
 * --------------------------------------------------------------- */

size_t nfs_iov_total_len(const struct nfs_iovec *iov, int iovcnt) {
    if (!iov || iovcnt <= 0)
        return 0;

    size_t total = 0;
    for (int i = 0; i < iovcnt; i++)
        total += iov[i].iov_len;

    return total;
}

/* ---------------------------------------------------------------
 * Scatter: copy contiguous data into iovec buffers.
 * --------------------------------------------------------------- */

int nfs_iov_scatter(const struct nfs_iovec *iov, int iovcnt, const uint8_t *data, size_t data_len) {
    if (!iov || iovcnt <= 0 || (!data && data_len > 0))
        return -1;

    size_t copied = 0;

    for (int i = 0; i < iovcnt && copied < data_len; i++) {
        if (!iov[i].iov_base || iov[i].iov_len == 0)
            continue;

        size_t chunk = data_len - copied;
        if (chunk > iov[i].iov_len)
            chunk = iov[i].iov_len;

        memcpy(iov[i].iov_base, data + copied, chunk);
        copied += chunk;
    }

    return (int)copied;
}

/* ---------------------------------------------------------------
 * Gather: concatenate iovec buffers into output.
 * --------------------------------------------------------------- */

int nfs_iov_gather(const struct nfs_iovec *iov, int iovcnt, uint8_t *out, size_t out_sz) {
    if (!iov || iovcnt <= 0 || !out)
        return -1;

    size_t total = nfs_iov_total_len(iov, iovcnt);
    if (total > out_sz)
        return -1;

    size_t pos = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len == 0 || !iov[i].iov_base)
            continue;
        memcpy(out + pos, iov[i].iov_base, iov[i].iov_len);
        pos += iov[i].iov_len;
    }

    return (int)pos;
}

/* ---------------------------------------------------------------
 * Format iovec array as string.
 * --------------------------------------------------------------- */

int nfs_iov_format(const struct nfs_iovec *iov, int iovcnt, char *out, size_t out_sz) {
    if (!iov || !out || out_sz == 0)
        return -1;

    size_t pos = 0;

    for (int i = 0; i < iovcnt; i++) {
        int n = snprintf(out + pos, out_sz - pos, "[%d] %zu bytes", i, iov[i].iov_len);
        if (n < 0 || (size_t)n >= out_sz - pos)
            return -1;
        pos += (size_t)n;

        if (i < iovcnt - 1) {
            if (pos + 2 >= out_sz)
                return -1;
            out[pos++] = ' ';
        }
    }

    return (int)pos;
}

/* ---------------------------------------------------------------
 * Find which iovec buffer a byte index falls into.
 * --------------------------------------------------------------- */

int nfs_iov_find_byte(const struct nfs_iovec *iov, int iovcnt, size_t byte_index, int *iov_idx,
                      size_t *offset) {
    if (!iov || iovcnt <= 0 || !iov_idx || !offset)
        return -1;

    size_t cumulative = 0;

    for (int i = 0; i < iovcnt; i++) {
        if (byte_index < cumulative + iov[i].iov_len) {
            *iov_idx = i;
            *offset = byte_index - cumulative;
            return 0;
        }
        cumulative += iov[i].iov_len;
    }

    return -1; /* out of range */
}
