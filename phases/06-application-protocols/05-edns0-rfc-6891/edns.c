/* EDNS(0) (RFC 6891) implementation. */

#include "edns.h"
#include <string.h>
#include <arpa/inet.h>

/* ---------------------------------------------------------------
 * Option encoder
 * --------------------------------------------------------------- */

int nfs_edns_encode_option(uint16_t code, const uint8_t *odata,
                           uint16_t length,
                           uint8_t *out, size_t out_sz)
{
    if (!out) return -1;
    if (length > 0 && !odata) return -1;
    if (4 + (size_t)length > out_sz) return -1;

    uint16_t tmp;
    tmp = htons(code);
    memcpy(out, &tmp, 2);
    tmp = htons(length);
    memcpy(out + 2, &tmp, 2);
    if (length > 0) {
        memcpy(out + 4, odata, length);
    }

    return (int)(4 + length);
}

/* ---------------------------------------------------------------
 * Option decoder
 * --------------------------------------------------------------- */

int nfs_edns_decode_option(const uint8_t *data, size_t data_len,
                           struct nfs_edns_option *opt)
{
    if (!data || !opt) return -1;
    if (data_len < 4) return -1;

    uint16_t code, length;
    memcpy(&code, data, 2);
    memcpy(&length, data + 2, 2);
    code = ntohs(code);
    length = ntohs(length);

    if (4 + (size_t)length > data_len) return -1;
    if (length > sizeof(opt->data)) return -1;

    opt->code = code;
    opt->length = length;
    if (length > 0) {
        memcpy(opt->data, data + 4, length);
    }

    return (int)(4 + length);
}

/* ---------------------------------------------------------------
 * OPT RR building
 * --------------------------------------------------------------- */

int nfs_edns_build_opt(uint16_t udp_payload_size,
                       uint8_t ext_rcode, uint8_t version,
                       int do_bit,
                       const struct nfs_edns_option *options,
                       uint16_t option_count,
                       uint8_t *out, size_t out_sz)
{
    if (!out) return -1;
    if (option_count > 0 && !options) return -1;

    size_t pos = 0;

    /* NAME = 0x00 (root domain) */
    if (pos >= out_sz) return -1;
    out[pos++] = 0x00;

    /* TYPE = 41 (OPT) */
    if (pos + 2 > out_sz) return -1;
    uint16_t tmp = htons(NFS_EDNS_OPT_TYPE);
    memcpy(out + pos, &tmp, 2);
    pos += 2;

    /* CLASS = UDP payload size */
    if (pos + 2 > out_sz) return -1;
    tmp = htons(udp_payload_size);
    memcpy(out + pos, &tmp, 2);
    pos += 2;

    /* TTL = extended RCODE(8) | version(8) | DO(1) | Z(15) */
    if (pos + 4 > out_sz) return -1;
    uint32_t ttl_val = ((uint32_t)ext_rcode << 24) |
                       ((uint32_t)version << 16) |
                       (do_bit ? (uint32_t)0x8000 : 0);
    uint32_t net_ttl = htonl(ttl_val);
    memcpy(out + pos, &net_ttl, 4);
    pos += 4;

    /* RDLENGTH placeholder */
    size_t rdlen_pos = pos;
    if (pos + 2 > out_sz) return -1;
    pos += 2;

    /* Encode options into RDATA */
    size_t rdata_start = pos;
    for (uint16_t i = 0; i < option_count; i++) {
        int olen = nfs_edns_encode_option(options[i].code,
                                           options[i].data,
                                           options[i].length,
                                           out + pos, out_sz - pos);
        if (olen < 0) return -1;
        pos += (size_t)olen;
    }

    /* Fill in RDLENGTH */
    uint16_t rdlen = (uint16_t)(pos - rdata_start);
    tmp = htons(rdlen);
    memcpy(out + rdlen_pos, &tmp, 2);

    return (int)pos;
}

/* ---------------------------------------------------------------
 * OPT RR parsing
 * --------------------------------------------------------------- */

int nfs_edns_parse_opt(const uint8_t *data, size_t data_len,
                       struct nfs_edns_opt *opt)
{
    if (!data || !opt) return -1;

    memset(opt, 0, sizeof(*opt));
    size_t pos = 0;

    /* NAME must be 0x00 (root) */
    if (pos >= data_len) return -1;
    if (data[pos] != 0x00) return -1;
    pos++;

    /* TYPE must be 41 */
    if (pos + 2 > data_len) return -1;
    uint16_t rtype;
    memcpy(&rtype, data + pos, 2);
    rtype = ntohs(rtype);
    if (rtype != NFS_EDNS_OPT_TYPE) return -1;
    pos += 2;

    /* CLASS = UDP payload size */
    if (pos + 2 > data_len) return -1;
    uint16_t rclass;
    memcpy(&rclass, data + pos, 2);
    opt->udp_payload_size = ntohs(rclass);
    pos += 2;

    /* TTL = extended flags */
    if (pos + 4 > data_len) return -1;
    uint32_t ttl_val;
    memcpy(&ttl_val, data + pos, 4);
    ttl_val = ntohl(ttl_val);
    opt->flags.ext_rcode = (uint8_t)(ttl_val >> 24);
    opt->flags.version = (uint8_t)((ttl_val >> 16) & 0xFF);
    opt->flags.do_bit = (ttl_val & 0x8000) ? 1 : 0;
    opt->flags.z = (uint16_t)(ttl_val & 0x7FFF);
    pos += 4;

    /* RDLENGTH */
    if (pos + 2 > data_len) return -1;
    uint16_t rdlength;
    memcpy(&rdlength, data + pos, 2);
    rdlength = ntohs(rdlength);
    pos += 2;

    if (pos + rdlength > data_len) return -1;

    /* Parse options from RDATA */
    size_t rdata_end = pos + rdlength;
    opt->option_count = 0;

    while (pos < rdata_end) {
        if (opt->option_count >= NFS_EDNS_MAX_OPTIONS) return -1;

        struct nfs_edns_option *o = &opt->options[opt->option_count];
        int consumed = nfs_edns_decode_option(data + pos, rdata_end - pos, o);
        if (consumed < 0) return -1;
        pos += (size_t)consumed;
        opt->option_count++;
    }

    return (int)pos;
}

/* ---------------------------------------------------------------
 * Convenience builders
 * --------------------------------------------------------------- */

int nfs_edns_build_nsid(const char *nsid_str, struct nfs_edns_option *opt)
{
    if (!nsid_str || !opt) return -1;

    size_t len = strlen(nsid_str);
    if (len > sizeof(opt->data)) return -1;

    opt->code = NFS_EDNS_OPT_NSID;
    opt->length = (uint16_t)len;
    memcpy(opt->data, nsid_str, len);

    return 0;
}

int nfs_edns_build_cookie(const uint8_t *client_cookie,
                          const uint8_t *server_cookie,
                          uint16_t server_cookie_len,
                          struct nfs_edns_option *opt)
{
    if (!client_cookie || !opt) return -1;
    /* Client cookie is always 8 bytes */
    /* Server cookie is 8-32 bytes (optional) */
    if (server_cookie && (server_cookie_len < 8 || server_cookie_len > 32))
        return -1;

    uint16_t total = 8;
    memcpy(opt->data, client_cookie, 8);
    if (server_cookie && server_cookie_len > 0) {
        if (8 + (size_t)server_cookie_len > sizeof(opt->data)) return -1;
        memcpy(opt->data + 8, server_cookie, server_cookie_len);
        total += server_cookie_len;
    }

    opt->code = NFS_EDNS_OPT_COOKIE;
    opt->length = total;

    return 0;
}

int nfs_edns_build_padding(uint16_t padding_len,
                           struct nfs_edns_option *opt)
{
    if (!opt) return -1;
    if (padding_len > sizeof(opt->data)) return -1;

    opt->code = NFS_EDNS_OPT_PADDING;
    opt->length = padding_len;
    memset(opt->data, 0, padding_len);

    return 0;
}

/* ---------------------------------------------------------------
 * Utility
 * --------------------------------------------------------------- */

const char *nfs_edns_option_str(uint16_t code)
{
    switch (code) {
    case NFS_EDNS_OPT_NSID:    return "NSID";
    case NFS_EDNS_OPT_COOKIE:  return "Cookie";
    case NFS_EDNS_OPT_PADDING: return "Padding";
    default:                    return "UNKNOWN";
    }
}
