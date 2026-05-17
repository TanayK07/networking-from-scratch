#ifndef NFS_IOVEC_H
#define NFS_IOVEC_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Scatter/Gather I/O (readv/writev)
 *
 * struct iovec describes a buffer:
 *   iov_base: pointer to data
 *   iov_len:  number of bytes
 *
 * Scatter: distribute contiguous data across multiple iovec buffers.
 * Gather:  concatenate multiple iovec buffers into a contiguous buffer.
 * --------------------------------------------------------------- */

/* Our portable iovec (matches POSIX struct iovec). */
struct nfs_iovec {
    void *iov_base;
    size_t iov_len;
};

/* Calculate total byte count across an array of iovecs.
 * Returns total bytes, or 0 if iov is NULL or iovcnt <= 0. */
size_t nfs_iov_total_len(const struct nfs_iovec *iov, int iovcnt);

/* Scatter: copy contiguous data into multiple iovec buffers.
 * Fills each iov buffer in order until data is exhausted.
 * Returns total bytes scattered (may be less than data_len if
 * iovec buffers are too small), or -1 on error. */
int nfs_iov_scatter(const struct nfs_iovec *iov, int iovcnt, const uint8_t *data, size_t data_len);

/* Gather: concatenate iovec buffers into a contiguous output buffer.
 * Returns total bytes gathered, or -1 on error. */
int nfs_iov_gather(const struct nfs_iovec *iov, int iovcnt, uint8_t *out, size_t out_sz);

/* Format iovec array as a human-readable string.
 * e.g. "[0] 5 bytes [1] 10 bytes ..."
 * Returns bytes written (excluding NUL), or -1 on error. */
int nfs_iov_format(const struct nfs_iovec *iov, int iovcnt, char *out, size_t out_sz);

/* Find which iovec buffer and offset a given byte index falls into.
 * byte_index: 0-based index into the logical concatenation.
 * *iov_idx: output iovec index.
 * *offset: output offset within that iovec.
 * Returns 0 on success, -1 if byte_index is out of range. */
int nfs_iov_find_byte(const struct nfs_iovec *iov, int iovcnt, size_t byte_index, int *iov_idx,
                      size_t *offset);

#endif /* NFS_IOVEC_H */
