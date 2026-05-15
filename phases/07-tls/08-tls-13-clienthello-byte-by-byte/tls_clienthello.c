/*
 * tls_clienthello.c -- TLS 1.3 ClientHello parser/builder
 *
 * Parses and builds the TLS ClientHello handshake message,
 * including the handshake header, cipher suites, compression
 * methods, and extensions.
 */

#include "tls_clienthello.h"
#include <string.h>
#include <arpa/inet.h>

/* ---- Helper: read big-endian values ---- */

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t read_u24(const uint8_t *p)
{
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}

static void write_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

static void write_u24(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 16) & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)(v & 0xFF);
}

/* ---- Parser ---- */

int nfs_tls_ch_parse(const uint8_t *buf, size_t len,
                     struct nfs_tls_client_hello *out)
{
    if (!buf || !out) return -1;
    if (len < 4) return -1;  /* handshake header minimum */

    memset(out, 0, sizeof(*out));

    size_t pos = 0;

    /* Handshake header: msg_type(1) + length(3) */
    out->msg_type = buf[pos++];
    if (out->msg_type != NFS_TLS_HS_CLIENT_HELLO) return -1;

    out->hs_length = read_u24(buf + pos);
    pos += 3;

    if (pos + out->hs_length > len) return -1;

    size_t end = pos + out->hs_length;

    /* legacy_version(2) */
    if (pos + 2 > end) return -1;
    out->legacy_version = read_u16(buf + pos);
    pos += 2;

    /* random(32) */
    if (pos + 32 > end) return -1;
    memcpy(out->random, buf + pos, 32);
    pos += 32;

    /* session_id_len(1) + session_id */
    if (pos + 1 > end) return -1;
    out->session_id_len = buf[pos++];
    if (out->session_id_len > NFS_TLS_CH_MAX_SESSION_ID) return -1;
    if (pos + out->session_id_len > end) return -1;
    if (out->session_id_len > 0) {
        memcpy(out->session_id, buf + pos, out->session_id_len);
    }
    pos += out->session_id_len;

    /* cipher_suites_len(2) + cipher_suites */
    if (pos + 2 > end) return -1;
    uint16_t cs_bytes = read_u16(buf + pos);
    pos += 2;
    if (cs_bytes % 2 != 0) return -1;
    uint16_t cs_count = cs_bytes / 2;
    if (cs_count > NFS_TLS_CH_MAX_CIPHER_SUITES) return -1;
    if (pos + cs_bytes > end) return -1;

    out->cipher_suites_count = cs_count;
    for (uint16_t i = 0; i < cs_count; i++) {
        out->cipher_suites[i] = read_u16(buf + pos);
        pos += 2;
    }

    /* compression_len(1) + compression_methods */
    if (pos + 1 > end) return -1;
    out->compression_len = buf[pos++];
    if (out->compression_len > 4) return -1;
    if (pos + out->compression_len > end) return -1;
    memcpy(out->compression, buf + pos, out->compression_len);
    pos += out->compression_len;

    /* extensions_len(2) + extensions */
    if (pos + 2 > end) return -1;
    uint16_t ext_bytes = read_u16(buf + pos);
    pos += 2;
    if (pos + ext_bytes > end) return -1;

    /* Parse individual extensions */
    size_t ext_end = pos + ext_bytes;
    out->extensions_count = 0;
    while (pos + 4 <= ext_end && out->extensions_count < NFS_TLS_CH_MAX_EXTENSIONS) {
        uint16_t ext_type = read_u16(buf + pos);
        pos += 2;
        uint16_t ext_len = read_u16(buf + pos);
        pos += 2;
        if (pos + ext_len > ext_end) return -1;

        struct nfs_tls_extension *e = &out->extensions[out->extensions_count];
        e->type = ext_type;
        e->length = ext_len;
        e->data = buf + pos;
        out->extensions_count++;
        pos += ext_len;
    }

    return (int)pos;
}

/* ---- Builder ---- */

int nfs_tls_ch_build(uint8_t *buf, size_t buf_max,
                     const struct nfs_tls_client_hello *ch,
                     const uint8_t *const *ext_bufs)
{
    if (!buf || !ch) return -1;

    /* Calculate total extensions size */
    size_t ext_total = 0;
    for (uint16_t i = 0; i < ch->extensions_count; i++) {
        ext_total += 4 + ch->extensions[i].length;  /* type(2) + len(2) + data */
    }

    /* Calculate body size (everything after handshake header) */
    size_t body_size = 2  /* legacy_version */
        + 32              /* random */
        + 1 + ch->session_id_len
        + 2 + (size_t)ch->cipher_suites_count * 2
        + 1 + ch->compression_len
        + 2 + ext_total;

    size_t total = 4 + body_size;  /* handshake header(4) + body */
    if (total > buf_max) return -1;

    size_t pos = 0;

    /* Handshake header */
    buf[pos++] = NFS_TLS_HS_CLIENT_HELLO;
    write_u24(buf + pos, (uint32_t)body_size);
    pos += 3;

    /* legacy_version */
    write_u16(buf + pos, ch->legacy_version);
    pos += 2;

    /* random */
    memcpy(buf + pos, ch->random, 32);
    pos += 32;

    /* session_id */
    buf[pos++] = ch->session_id_len;
    if (ch->session_id_len > 0) {
        memcpy(buf + pos, ch->session_id, ch->session_id_len);
        pos += ch->session_id_len;
    }

    /* cipher_suites */
    write_u16(buf + pos, ch->cipher_suites_count * 2);
    pos += 2;
    for (uint16_t i = 0; i < ch->cipher_suites_count; i++) {
        write_u16(buf + pos, ch->cipher_suites[i]);
        pos += 2;
    }

    /* compression_methods */
    buf[pos++] = ch->compression_len;
    memcpy(buf + pos, ch->compression, ch->compression_len);
    pos += ch->compression_len;

    /* extensions */
    write_u16(buf + pos, (uint16_t)ext_total);
    pos += 2;
    for (uint16_t i = 0; i < ch->extensions_count; i++) {
        write_u16(buf + pos, ch->extensions[i].type);
        pos += 2;
        write_u16(buf + pos, ch->extensions[i].length);
        pos += 2;
        if (ch->extensions[i].length > 0 && ext_bufs && ext_bufs[i]) {
            memcpy(buf + pos, ext_bufs[i], ch->extensions[i].length);
        }
        pos += ch->extensions[i].length;
    }

    return (int)pos;
}

/* ---- Extension iterator ---- */

void nfs_tls_ext_iter_init(struct nfs_tls_ext_iter *it,
                           const uint8_t *data, size_t len)
{
    if (it) {
        it->data = data;
        it->total = len;
        it->offset = 0;
    }
}

int nfs_tls_ext_iter_next(struct nfs_tls_ext_iter *it,
                          struct nfs_tls_extension *ext)
{
    if (!it || !ext || !it->data) return -1;
    if (it->offset + 4 > it->total) return -1;

    ext->type = read_u16(it->data + it->offset);
    it->offset += 2;
    ext->length = read_u16(it->data + it->offset);
    it->offset += 2;

    if (it->offset + ext->length > it->total) return -1;

    ext->data = it->data + it->offset;
    it->offset += ext->length;
    return 0;
}

/* ---- Name lookups ---- */

const char *nfs_tls_cipher_suite_name(uint16_t cs)
{
    switch (cs) {
    case NFS_TLS_CS_AES_128_GCM_SHA256:       return "TLS_AES_128_GCM_SHA256";
    case NFS_TLS_CS_AES_256_GCM_SHA384:       return "TLS_AES_256_GCM_SHA384";
    case NFS_TLS_CS_CHACHA20_POLY1305_SHA256:  return "TLS_CHACHA20_POLY1305_SHA256";
    default:                                   return "UNKNOWN";
    }
}

const char *nfs_tls_extension_name(uint16_t type)
{
    switch (type) {
    case NFS_TLS_EXT_SERVER_NAME:          return "server_name";
    case NFS_TLS_EXT_SIGNATURE_ALGORITHMS: return "signature_algorithms";
    case NFS_TLS_EXT_SUPPORTED_VERSIONS:   return "supported_versions";
    case NFS_TLS_EXT_KEY_SHARE:            return "key_share";
    default:                               return "UNKNOWN";
    }
}
