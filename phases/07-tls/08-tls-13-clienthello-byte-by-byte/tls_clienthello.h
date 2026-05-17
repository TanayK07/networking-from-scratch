#ifndef NFS_TLS_CLIENTHELLO_H
#define NFS_TLS_CLIENTHELLO_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TLS 1.3 ClientHello (byte-by-byte)
 *
 * Handshake header: msg_type(8) + length(24) = 4 bytes
 * ClientHello body:
 *   legacy_version(16) = 0x0303
 *   random(32)
 *   session_id_len(8) + session_id(0..32)
 *   cipher_suites_len(16) + cipher_suites
 *   compression_len(8) + compression_methods
 *   extensions_len(16) + extensions
 *
 * Extension: type(16) + length(16) + data
 *
 * Key extensions for TLS 1.3:
 *   supported_versions(43) -- negotiates TLS 1.3
 *   key_share(51)          -- X25519 public key
 *   signature_algorithms(13)
 *   server_name(0)         -- SNI
 * --------------------------------------------------------------- */

/* Handshake message types */
#define NFS_TLS_HS_CLIENT_HELLO 1
#define NFS_TLS_HS_SERVER_HELLO 2

/* Cipher suites */
#define NFS_TLS_CS_AES_128_GCM_SHA256       0x1301
#define NFS_TLS_CS_AES_256_GCM_SHA384       0x1302
#define NFS_TLS_CS_CHACHA20_POLY1305_SHA256 0x1303

/* Extension types */
#define NFS_TLS_EXT_SERVER_NAME          0
#define NFS_TLS_EXT_SIGNATURE_ALGORITHMS 13
#define NFS_TLS_EXT_SUPPORTED_VERSIONS   43
#define NFS_TLS_EXT_KEY_SHARE            51

#define NFS_TLS_CH_MAX_SESSION_ID    32
#define NFS_TLS_CH_MAX_CIPHER_SUITES 16
#define NFS_TLS_CH_MAX_EXTENSIONS    16
#define NFS_TLS_CH_RANDOM_SIZE       32

/* A single extension (parsed or to-be-built). */
struct nfs_tls_extension {
    uint16_t type;
    uint16_t length;
    const uint8_t *data; /* pointer into parse buffer (parse only) */
};

/* Parsed ClientHello message. */
struct nfs_tls_client_hello {
    /* Handshake header */
    uint8_t msg_type;
    uint32_t hs_length;

    /* ClientHello fields */
    uint16_t legacy_version;
    uint8_t random[NFS_TLS_CH_RANDOM_SIZE];

    uint8_t session_id_len;
    uint8_t session_id[NFS_TLS_CH_MAX_SESSION_ID];

    uint16_t cipher_suites_count;
    uint16_t cipher_suites[NFS_TLS_CH_MAX_CIPHER_SUITES];

    uint8_t compression_len;
    uint8_t compression[4]; /* typically just {0x00} */

    uint16_t extensions_count;
    struct nfs_tls_extension extensions[NFS_TLS_CH_MAX_EXTENSIONS];
};

/* Extension iterator state. */
struct nfs_tls_ext_iter {
    const uint8_t *data;
    size_t total;
    size_t offset;
};

/* Parse a ClientHello from raw bytes (starting at handshake header).
 * Returns total bytes consumed, or -1 on error. */
int nfs_tls_ch_parse(const uint8_t *buf, size_t len, struct nfs_tls_client_hello *out);

/* Build a ClientHello into buf (including handshake header).
 * ch: pre-filled ClientHello structure (extensions use .data pointers
 *     and .length for each extension's raw data).
 * ext_bufs: array of raw extension data buffers (one per extension).
 * Returns total bytes written, or -1 on error. */
int nfs_tls_ch_build(uint8_t *buf, size_t buf_max, const struct nfs_tls_client_hello *ch,
                     const uint8_t *const *ext_bufs);

/* Initialize an extension iterator over raw extensions data. */
void nfs_tls_ext_iter_init(struct nfs_tls_ext_iter *it, const uint8_t *data, size_t len);

/* Advance iterator; fills ext on success. Returns 0 or -1 at end. */
int nfs_tls_ext_iter_next(struct nfs_tls_ext_iter *it, struct nfs_tls_extension *ext);

/* Return cipher suite name, or "UNKNOWN". */
const char *nfs_tls_cipher_suite_name(uint16_t cs);

/* Return extension type name, or "UNKNOWN". */
const char *nfs_tls_extension_name(uint16_t type);

#endif /* NFS_TLS_CLIENTHELLO_H */
