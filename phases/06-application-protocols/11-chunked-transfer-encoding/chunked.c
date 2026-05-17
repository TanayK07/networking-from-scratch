/* Chunked transfer encoding implementation. */

#include "chunked.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Utility: hex parsing
 * --------------------------------------------------------------- */

size_t nfs_chunked_parse_hex(const char *hex, size_t len) {
    if (!hex || len == 0)
        return (size_t)-1;

    size_t val = 0;
    for (size_t i = 0; i < len; i++) {
        char c = hex[i];
        int digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else
            return (size_t)-1;

        /* Overflow check */
        if (val > (size_t)-1 / 16)
            return (size_t)-1;
        val = val * 16 + (size_t)digit;
    }
    return val;
}

/* ---------------------------------------------------------------
 * Helper: find CRLF
 * --------------------------------------------------------------- */

static const uint8_t *find_crlf(const uint8_t *data, size_t len) {
    for (size_t i = 0; i + 1 < len; i++) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            return data + i;
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------
 * Encoding
 * --------------------------------------------------------------- */

int nfs_chunked_encode_chunk(const uint8_t *data, size_t data_len, uint8_t *out, size_t out_sz) {
    if (!out)
        return -1;

    /* Format: hex-size CRLF data CRLF */
    char size_line[32];
    int slen = snprintf(size_line, sizeof(size_line), "%zx\r\n", data_len);
    if (slen < 0)
        return -1;

    size_t total = (size_t)slen + data_len + 2; /* +2 for trailing CRLF */
    if (total > out_sz)
        return -1;

    /* Write size line */
    memcpy(out, size_line, (size_t)slen);
    size_t pos = (size_t)slen;

    /* Write data */
    if (data_len > 0) {
        if (!data)
            return -1;
        memcpy(out + pos, data, data_len);
        pos += data_len;
    }

    /* Trailing CRLF */
    out[pos++] = '\r';
    out[pos++] = '\n';

    return (int)pos;
}

int nfs_chunked_encode_last(uint8_t *out, size_t out_sz) {
    if (!out || out_sz < 5)
        return -1;
    /* "0\r\n\r\n" */
    memcpy(out, "0\r\n\r\n", 5);
    return 5;
}

int nfs_chunked_encode(const uint8_t *data, size_t data_len, size_t chunk_size, uint8_t *out,
                       size_t out_sz) {
    if (!out)
        return -1;
    if (chunk_size == 0)
        return -1;
    if (data_len > 0 && !data)
        return -1;

    size_t pos = 0;
    size_t offset = 0;

    while (offset < data_len) {
        size_t remaining = data_len - offset;
        size_t csz = (remaining < chunk_size) ? remaining : chunk_size;

        int written = nfs_chunked_encode_chunk(data + offset, csz, out + pos, out_sz - pos);
        if (written < 0)
            return -1;
        pos += (size_t)written;
        offset += csz;
    }

    /* Terminating chunk */
    int written = nfs_chunked_encode_last(out + pos, out_sz - pos);
    if (written < 0)
        return -1;
    pos += (size_t)written;

    return (int)pos;
}

/* ---------------------------------------------------------------
 * Decoding
 * --------------------------------------------------------------- */

int nfs_chunk_parse_next(const uint8_t *data, size_t data_len, struct nfs_chunk *chunk) {
    if (!data || !chunk || data_len == 0)
        return -1;

    /* Find the CRLF after the size line */
    const uint8_t *crlf = find_crlf(data, data_len);
    if (!crlf)
        return -1;

    size_t size_line_len = (size_t)(crlf - data);
    if (size_line_len == 0)
        return -1;

    /* Parse hex size (ignore chunk extensions after ';') */
    size_t hex_len = size_line_len;
    for (size_t i = 0; i < size_line_len; i++) {
        if (data[i] == ';') {
            hex_len = i;
            break;
        }
    }

    size_t chunk_size = nfs_chunked_parse_hex((const char *)data, hex_len);
    if (chunk_size == (size_t)-1)
        return -1;

    /* After size line: CRLF + data + CRLF */
    size_t consumed = size_line_len + 2; /* size line + CRLF */

    if (chunk_size == 0) {
        /* Last chunk: just the size line + CRLF */
        chunk->size = 0;
        chunk->data = NULL;
        return (int)consumed;
    }

    /* Check we have enough data */
    if (consumed + chunk_size + 2 > data_len)
        return -1;

    chunk->size = chunk_size;
    chunk->data = data + consumed;

    /* Verify trailing CRLF after data */
    if (data[consumed + chunk_size] != '\r' || data[consumed + chunk_size + 1] != '\n') {
        return -1;
    }

    consumed += chunk_size + 2; /* data + trailing CRLF */
    return (int)consumed;
}

int nfs_chunked_decode(const uint8_t *data, size_t data_len, struct nfs_chunked_decoded *decoded) {
    if (!data || !decoded || !decoded->data)
        return -1;

    size_t pos = 0;
    size_t out_pos = 0;
    decoded->trailer_count = 0;

    while (pos < data_len) {
        struct nfs_chunk chunk;
        int consumed = nfs_chunk_parse_next(data + pos, data_len - pos, &chunk);
        if (consumed < 0)
            return -1;

        if (chunk.size == 0) {
            pos += (size_t)consumed;
            /* Parse optional trailers */
            while (pos < data_len) {
                /* Empty line = end of trailers */
                if (pos + 1 < data_len && data[pos] == '\r' && data[pos + 1] == '\n') {
                    pos += 2;
                    break;
                }

                const uint8_t *crlf = find_crlf(data + pos, data_len - pos);
                if (!crlf)
                    break;

                size_t line_len = (size_t)(crlf - (data + pos));
                if (line_len == 0) {
                    pos += 2;
                    break;
                }

                /* Parse trailer: name: value */
                if (decoded->trailer_count < NFS_CHUNKED_MAX_TRAILERS) {
                    const uint8_t *colon = memchr(data + pos, ':', line_len);
                    if (colon) {
                        struct nfs_chunked_trailer *t = &decoded->trailers[decoded->trailer_count];
                        size_t nlen = (size_t)(colon - (data + pos));
                        if (nlen < NFS_CHUNKED_MAX_HDR_NAME) {
                            memcpy(t->name, data + pos, nlen);
                            t->name[nlen] = '\0';

                            const uint8_t *vstart = colon + 1;
                            while (vstart < crlf && *vstart == ' ')
                                vstart++;
                            size_t vlen = (size_t)(crlf - vstart);
                            if (vlen < NFS_CHUNKED_MAX_HDR_VALUE) {
                                memcpy(t->value, vstart, vlen);
                                t->value[vlen] = '\0';
                                decoded->trailer_count++;
                            }
                        }
                    }
                }
                pos = (size_t)(crlf - data) + 2;
            }
            break;
        }

        /* Copy chunk data to output */
        memcpy(decoded->data + out_pos, chunk.data, chunk.size);
        out_pos += chunk.size;

        pos += (size_t)consumed;
    }

    decoded->data_len = out_pos;
    return (int)pos;
}
