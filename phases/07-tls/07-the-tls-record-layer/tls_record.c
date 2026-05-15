/*
 * tls_record.c -- TLS Record Layer parser/builder
 *
 * Implements parsing and building of TLS record headers, plus
 * alert record parsing. All multi-byte fields use big-endian
 * (network byte order).
 */

#include "tls_record.h"
#include <string.h>
#include <arpa/inet.h>

int nfs_tls_record_parse(const uint8_t *buf, size_t len,
                         struct nfs_tls_record *out)
{
    if (!buf || !out) return -1;
    if (len < NFS_TLS_RECORD_HDR_SIZE) return -1;

    out->content_type = buf[0];

    /* Validate content type */
    if (out->content_type < NFS_TLS_CT_CHANGE_CIPHER ||
        out->content_type > NFS_TLS_CT_APPLICATION)
        return -1;

    uint16_t ver;
    memcpy(&ver, buf + 1, 2);
    out->version = ntohs(ver);

    uint16_t flen;
    memcpy(&flen, buf + 3, 2);
    out->length = ntohs(flen);

    /* Validate length */
    if (out->length > NFS_TLS_MAX_CIPHERTEXT) return -1;
    if (NFS_TLS_RECORD_HDR_SIZE + (size_t)out->length > len) return -1;

    out->fragment = buf + NFS_TLS_RECORD_HDR_SIZE;

    return NFS_TLS_RECORD_HDR_SIZE + (int)out->length;
}

int nfs_tls_record_build(uint8_t *buf, size_t buf_max,
                         uint8_t content_type, uint16_t version,
                         const uint8_t *fragment, uint16_t frag_len)
{
    if (!buf) return -1;
    if (frag_len > 0 && !fragment) return -1;
    if (frag_len > NFS_TLS_MAX_CIPHERTEXT) return -1;

    size_t total = NFS_TLS_RECORD_HDR_SIZE + frag_len;
    if (total > buf_max) return -1;

    buf[0] = content_type;

    uint16_t ver_net = htons(version);
    memcpy(buf + 1, &ver_net, 2);

    uint16_t len_net = htons(frag_len);
    memcpy(buf + 3, &len_net, 2);

    if (frag_len > 0) {
        memcpy(buf + NFS_TLS_RECORD_HDR_SIZE, fragment, frag_len);
    }

    return (int)total;
}

int nfs_tls_alert_parse(const uint8_t *fragment, size_t len,
                        struct nfs_tls_alert *out)
{
    if (!fragment || !out) return -1;
    if (len < 2) return -1;

    out->level = fragment[0];
    out->description = fragment[1];

    /* Validate level */
    if (out->level != NFS_TLS_ALERT_WARNING &&
        out->level != NFS_TLS_ALERT_FATAL)
        return -1;

    return 0;
}

int nfs_tls_alert_build(uint8_t *buf, size_t buf_max,
                        uint8_t level, uint8_t description)
{
    if (!buf || buf_max < 2) return -1;
    buf[0] = level;
    buf[1] = description;
    return 2;
}

const char *nfs_tls_content_type_name(uint8_t ct)
{
    switch (ct) {
    case NFS_TLS_CT_CHANGE_CIPHER: return "change_cipher_spec";
    case NFS_TLS_CT_ALERT:         return "alert";
    case NFS_TLS_CT_HANDSHAKE:     return "handshake";
    case NFS_TLS_CT_APPLICATION:   return "application_data";
    default:                       return "unknown";
    }
}

const char *nfs_tls_version_name(uint16_t version)
{
    switch (version) {
    case NFS_TLS_VERSION_10: return "TLS 1.0";
    case NFS_TLS_VERSION_11: return "TLS 1.1";
    case NFS_TLS_VERSION_12: return "TLS 1.2";
    case NFS_TLS_VERSION_13: return "TLS 1.3";
    default:                 return "unknown";
    }
}

const char *nfs_tls_alert_desc_name(uint8_t desc)
{
    switch (desc) {
    case NFS_TLS_ALERT_CLOSE_NOTIFY:       return "close_notify";
    case NFS_TLS_ALERT_UNEXPECTED_MESSAGE: return "unexpected_message";
    case NFS_TLS_ALERT_BAD_RECORD_MAC:     return "bad_record_mac";
    case NFS_TLS_ALERT_RECORD_OVERFLOW:    return "record_overflow";
    case NFS_TLS_ALERT_HANDSHAKE_FAILURE:  return "handshake_failure";
    case NFS_TLS_ALERT_BAD_CERTIFICATE:    return "bad_certificate";
    case NFS_TLS_ALERT_CERTIFICATE_EXPIRED: return "certificate_expired";
    case NFS_TLS_ALERT_UNKNOWN_CA:         return "unknown_ca";
    case NFS_TLS_ALERT_DECODE_ERROR:       return "decode_error";
    case NFS_TLS_ALERT_DECRYPT_ERROR:      return "decrypt_error";
    case NFS_TLS_ALERT_PROTOCOL_VERSION:   return "protocol_version";
    case NFS_TLS_ALERT_INTERNAL_ERROR:     return "internal_error";
    default:                               return "unknown";
    }
}

const char *nfs_tls_alert_level_name(uint8_t level)
{
    switch (level) {
    case NFS_TLS_ALERT_WARNING: return "warning";
    case NFS_TLS_ALERT_FATAL:   return "fatal";
    default:                    return "unknown";
    }
}
