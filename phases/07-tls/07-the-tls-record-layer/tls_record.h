#ifndef NFS_TLS_RECORD_H
#define NFS_TLS_RECORD_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TLS Record Layer
 *
 * Record header: 5 bytes
 *   content_type(8) + legacy_version(16) + length(16)
 *
 * Content types:
 *   20 = change_cipher_spec
 *   21 = alert
 *   22 = handshake
 *   23 = application_data
 *
 * Max record payload: 2^14 = 16384 bytes (plaintext)
 * Max with overhead:  16384 + 256 = 16640 (encrypted)
 *
 * Alert record body: level(8) + description(8)
 *   level: 1=warning, 2=fatal
 * --------------------------------------------------------------- */

#define NFS_TLS_RECORD_HDR_SIZE 5
#define NFS_TLS_MAX_PLAINTEXT   16384
#define NFS_TLS_MAX_CIPHERTEXT  (16384 + 256)

/* Content types */
#define NFS_TLS_CT_CHANGE_CIPHER 20
#define NFS_TLS_CT_ALERT         21
#define NFS_TLS_CT_HANDSHAKE     22
#define NFS_TLS_CT_APPLICATION   23

/* Common TLS versions (in wire format, i.e. already in order) */
#define NFS_TLS_VERSION_10 0x0301
#define NFS_TLS_VERSION_11 0x0302
#define NFS_TLS_VERSION_12 0x0303
#define NFS_TLS_VERSION_13 0x0304 /* only in supported_versions ext */

/* Alert levels */
#define NFS_TLS_ALERT_WARNING 1
#define NFS_TLS_ALERT_FATAL   2

/* Alert descriptions */
#define NFS_TLS_ALERT_CLOSE_NOTIFY        0
#define NFS_TLS_ALERT_UNEXPECTED_MESSAGE  10
#define NFS_TLS_ALERT_BAD_RECORD_MAC      20
#define NFS_TLS_ALERT_RECORD_OVERFLOW     22
#define NFS_TLS_ALERT_HANDSHAKE_FAILURE   40
#define NFS_TLS_ALERT_BAD_CERTIFICATE     42
#define NFS_TLS_ALERT_CERTIFICATE_EXPIRED 45
#define NFS_TLS_ALERT_UNKNOWN_CA          48
#define NFS_TLS_ALERT_DECODE_ERROR        50
#define NFS_TLS_ALERT_DECRYPT_ERROR       51
#define NFS_TLS_ALERT_PROTOCOL_VERSION    70
#define NFS_TLS_ALERT_INTERNAL_ERROR      80

/* Parsed TLS record header. */
struct nfs_tls_record {
    uint8_t content_type;
    uint16_t version;        /* e.g. 0x0303 for TLS 1.2 */
    uint16_t length;         /* fragment length */
    const uint8_t *fragment; /* pointer into original buffer */
};

/* Parsed TLS alert. */
struct nfs_tls_alert {
    uint8_t level;
    uint8_t description;
};

/* Parse a TLS record from raw bytes.
 * buf/len: raw data (must contain at least the 5-byte header + fragment).
 * out: parsed record on success.
 * Returns total consumed bytes on success, -1 on error. */
int nfs_tls_record_parse(const uint8_t *buf, size_t len, struct nfs_tls_record *out);

/* Build a TLS record into buf.
 * Returns total bytes written, or -1 on error. */
int nfs_tls_record_build(uint8_t *buf, size_t buf_max, uint8_t content_type, uint16_t version,
                         const uint8_t *fragment, uint16_t frag_len);

/* Parse a TLS alert from a record's fragment.
 * Returns 0 on success, -1 on error. */
int nfs_tls_alert_parse(const uint8_t *fragment, size_t len, struct nfs_tls_alert *out);

/* Build a TLS alert fragment (2 bytes).
 * Returns 2 on success, -1 on error. */
int nfs_tls_alert_build(uint8_t *buf, size_t buf_max, uint8_t level, uint8_t description);

/* Return a human-readable name for a content type. */
const char *nfs_tls_content_type_name(uint8_t ct);

/* Return a human-readable name for a TLS version. */
const char *nfs_tls_version_name(uint16_t version);

/* Return a human-readable name for an alert description. */
const char *nfs_tls_alert_desc_name(uint8_t desc);

/* Return "warning" or "fatal" for an alert level. */
const char *nfs_tls_alert_level_name(uint8_t level);

#endif /* NFS_TLS_RECORD_H */
