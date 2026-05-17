#ifndef NFS_CHUNKED_H
#define NFS_CHUNKED_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Chunked transfer encoding (HTTP/1.1 RFC 7230 Section 4.1).
 *
 * Chunk format:
 *   chunk-size (hex) CRLF
 *   chunk-data       CRLF
 *
 * Terminated by a zero-size chunk:
 *   0 CRLF CRLF
 *
 * Optional trailer headers after the final chunk.
 * --------------------------------------------------------------- */

/* Limits */
#define NFS_CHUNKED_MAX_CHUNK_SIZE 65536
#define NFS_CHUNKED_MAX_TRAILERS   16
#define NFS_CHUNKED_MAX_HDR_NAME   128
#define NFS_CHUNKED_MAX_HDR_VALUE  4096

/* A single parsed chunk */
struct nfs_chunk {
    size_t size;         /* chunk data size (0 = last chunk) */
    const uint8_t *data; /* pointer into source buffer */
};

/* Trailer header */
struct nfs_chunked_trailer {
    char name[NFS_CHUNKED_MAX_HDR_NAME];
    char value[NFS_CHUNKED_MAX_HDR_VALUE];
};

/* Decoded chunked data result */
struct nfs_chunked_decoded {
    uint8_t *data;
    size_t data_len;
    uint16_t trailer_count;
    struct nfs_chunked_trailer trailers[NFS_CHUNKED_MAX_TRAILERS];
};

/* --- Encoding --------------------------------------------------- */

/* Encode data into chunked transfer encoding.
 * Splits `data` into chunks of `chunk_size` bytes each.
 * Writes to `out` with hex size + CRLF + data + CRLF format.
 * Appends the terminating 0\r\n\r\n.
 * Returns total bytes written, or -1 on error. */
int nfs_chunked_encode(const uint8_t *data, size_t data_len, size_t chunk_size, uint8_t *out,
                       size_t out_sz);

/* Encode a single chunk (size line + data + trailing CRLF).
 * Returns bytes written, or -1 on error.
 * For the last chunk, pass data=NULL, data_len=0. */
int nfs_chunked_encode_chunk(const uint8_t *data, size_t data_len, uint8_t *out, size_t out_sz);

/* Write the terminating chunk (0\r\n\r\n).
 * Returns bytes written, or -1 on error. */
int nfs_chunked_encode_last(uint8_t *out, size_t out_sz);

/* --- Decoding --------------------------------------------------- */

/* Decode a complete chunked transfer-encoded message.
 * `data` is the raw chunked stream.
 * `decoded` receives the reassembled body in decoded->data
 * (which must point to a pre-allocated buffer) and trailer count.
 * Returns the total bytes consumed from `data`, or -1 on error. */
int nfs_chunked_decode(const uint8_t *data, size_t data_len, struct nfs_chunked_decoded *decoded);

/* Parse the next chunk from a chunked stream.
 * `data` points to the start of a chunk-size line.
 * Fills in the nfs_chunk struct.
 * Returns total bytes consumed (size line + data + CRLF),
 * or -1 on error. */
int nfs_chunk_parse_next(const uint8_t *data, size_t data_len, struct nfs_chunk *chunk);

/* --- Utility ---------------------------------------------------- */

/* Parse a hex string to a size_t value.
 * Returns the parsed value, or (size_t)-1 on error. */
size_t nfs_chunked_parse_hex(const char *hex, size_t len);

#endif /* NFS_CHUNKED_H */
