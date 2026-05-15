/* DNS resolver implementation -- builds queries and parses responses. */

#include "dns_resolver.h"
#include <string.h>
#include <arpa/inet.h>

/* ---------------------------------------------------------------
 * Flag helpers
 * --------------------------------------------------------------- */

int nfs_dns_is_response(const struct nfs_dns_header *h)
{
    uint16_t flags = ntohs(h->flags);
    return (flags >> 15) & 1;
}

int nfs_dns_is_query(const struct nfs_dns_header *h)
{
    return !nfs_dns_is_response(h);
}

uint8_t nfs_dns_rcode(const struct nfs_dns_header *h)
{
    uint16_t flags = ntohs(h->flags);
    return (uint8_t)(flags & 0x000F);
}

const char *nfs_dns_rcode_str(uint8_t rcode)
{
    switch (rcode) {
    case NFS_DNS_RCODE_NOERROR:  return "NOERROR";
    case NFS_DNS_RCODE_FORMERR:  return "FORMERR";
    case NFS_DNS_RCODE_SERVFAIL: return "SERVFAIL";
    case NFS_DNS_RCODE_NXDOMAIN: return "NXDOMAIN";
    case NFS_DNS_RCODE_NOTIMP:   return "NOTIMP";
    case NFS_DNS_RCODE_REFUSED:  return "REFUSED";
    default:                     return "UNKNOWN";
    }
}

const char *nfs_dns_type_str(uint16_t type)
{
    switch (type) {
    case NFS_DNS_TYPE_A:     return "A";
    case NFS_DNS_TYPE_NS:    return "NS";
    case NFS_DNS_TYPE_CNAME: return "CNAME";
    case NFS_DNS_TYPE_SOA:   return "SOA";
    case NFS_DNS_TYPE_PTR:   return "PTR";
    case NFS_DNS_TYPE_MX:    return "MX";
    case NFS_DNS_TYPE_TXT:   return "TXT";
    case NFS_DNS_TYPE_AAAA:  return "AAAA";
    default:                 return "UNKNOWN";
    }
}

/* ---------------------------------------------------------------
 * Name encoding
 * --------------------------------------------------------------- */

int nfs_dns_name_encode(const char *name, uint8_t *out, size_t out_sz)
{
    if (!name || !out) return -1;

    /* Handle root domain "." or empty string */
    if (name[0] == '\0' || (name[0] == '.' && name[1] == '\0')) {
        if (out_sz < 1) return -1;
        out[0] = 0;
        return 1;
    }

    size_t name_len = strlen(name);
    size_t pos = 0;  /* write position in out */
    size_t i = 0;    /* read position in name */

    while (i < name_len) {
        /* Find the end of this label */
        const char *dot = strchr(name + i, '.');
        size_t label_len;
        if (dot) {
            label_len = (size_t)(dot - (name + i));
        } else {
            label_len = name_len - i;
        }

        if (label_len == 0) {
            /* Skip trailing dot */
            i++;
            continue;
        }
        if (label_len > NFS_DNS_MAX_LABEL) return -1;

        /* length byte + label bytes */
        if (pos + 1 + label_len >= out_sz) return -1;

        out[pos++] = (uint8_t)label_len;
        memcpy(out + pos, name + i, label_len);
        pos += label_len;

        i += label_len;
        if (dot) i++; /* skip the dot */
    }

    /* Terminating zero-length label */
    if (pos >= out_sz) return -1;
    out[pos++] = 0;

    return (int)pos;
}

/* ---------------------------------------------------------------
 * Name decoding (with compression pointer support)
 * --------------------------------------------------------------- */

int nfs_dns_name_decode(const uint8_t *data, size_t data_len,
                        size_t offset, char *out, size_t out_sz)
{
    if (!data || !out || out_sz == 0) return -1;
    if (offset >= data_len) return -1;

    size_t out_pos = 0;
    size_t cur = offset;
    int jumped = 0;
    int bytes_consumed = 0;
    int max_jumps = 128;
    int jumps = 0;

    while (cur < data_len) {
        uint8_t len = data[cur];

        if (len == 0) {
            /* End of name */
            if (!jumped) {
                bytes_consumed = (int)(cur - offset + 1);
            }
            break;
        }

        /* Compression pointer? Top 2 bits == 11 */
        if ((len & 0xC0) == 0xC0) {
            if (cur + 1 >= data_len) return -1;
            if (!jumped) {
                bytes_consumed = (int)(cur - offset + 2);
            }
            uint16_t ptr = (uint16_t)(((len & 0x3F) << 8) | data[cur + 1]);
            if (ptr >= data_len) return -1;
            cur = ptr;
            jumped = 1;
            if (++jumps > max_jumps) return -1;
            continue;
        }

        /* Reserved bits check */
        if ((len & 0xC0) != 0) return -1;

        cur++;
        if (cur + len > data_len) return -1;

        /* Add dot separator if not first label */
        if (out_pos > 0) {
            if (out_pos >= out_sz - 1) return -1;
            out[out_pos++] = '.';
        }

        if (out_pos + len >= out_sz) return -1;
        memcpy(out + out_pos, data + cur, len);
        out_pos += len;
        cur += len;
    }

    if (out_pos >= out_sz) return -1;
    out[out_pos] = '\0';

    if (!jumped && bytes_consumed == 0) return -1;

    return bytes_consumed;
}

/* ---------------------------------------------------------------
 * Query building
 * --------------------------------------------------------------- */

int nfs_dns_query_build(const char *name, uint16_t qtype,
                        uint16_t id, uint8_t *out, size_t out_sz)
{
    if (!name || !out) return -1;
    if (out_sz < NFS_DNS_HEADER_SIZE) return -1;

    /* Build header */
    struct nfs_dns_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.id = htons(id);
    /* QR=0 (query), OPCODE=0, RD=1 (recursion desired) */
    hdr.flags = htons(0x0100);
    hdr.qdcount = htons(1);

    memcpy(out, &hdr, NFS_DNS_HEADER_SIZE);

    /* Encode question name */
    int name_len = nfs_dns_name_encode(name, out + NFS_DNS_HEADER_SIZE,
                                        out_sz - NFS_DNS_HEADER_SIZE);
    if (name_len < 0) return -1;

    size_t pos = NFS_DNS_HEADER_SIZE + (size_t)name_len;

    /* QTYPE + QCLASS (4 bytes) */
    if (pos + 4 > out_sz) return -1;
    uint16_t qt = htons(qtype);
    uint16_t qc = htons(NFS_DNS_CLASS_IN);
    memcpy(out + pos, &qt, 2);
    pos += 2;
    memcpy(out + pos, &qc, 2);
    pos += 2;

    return (int)pos;
}

/* ---------------------------------------------------------------
 * Response parsing
 * --------------------------------------------------------------- */

int nfs_dns_response_parse(const uint8_t *data, size_t len,
                           struct nfs_dns_response *resp)
{
    if (!data || !resp) return -1;
    if (len < NFS_DNS_HEADER_SIZE) return -1;

    memset(resp, 0, sizeof(*resp));

    /* Parse header */
    memcpy(&resp->header, data, NFS_DNS_HEADER_SIZE);

    /* Verify it's a response */
    if (!nfs_dns_is_response(&resp->header)) return -1;

    uint16_t qdcount = ntohs(resp->header.qdcount);
    uint16_t ancount = ntohs(resp->header.ancount);

    size_t offset = NFS_DNS_HEADER_SIZE;

    /* Parse question section */
    for (uint16_t i = 0; i < qdcount; i++) {
        char qname[NFS_DNS_MAX_NAME];
        int consumed = nfs_dns_name_decode(data, len, offset, qname, sizeof(qname));
        if (consumed < 0) return -1;
        offset += (size_t)consumed;

        if (offset + 4 > len) return -1;

        if (i == 0) {
            /* Save first question */
            memcpy(resp->qname, qname, sizeof(resp->qname));
            uint16_t qt, qc;
            memcpy(&qt, data + offset, 2);
            memcpy(&qc, data + offset + 2, 2);
            resp->qtype = ntohs(qt);
            resp->qclass = ntohs(qc);
        }

        offset += 4; /* QTYPE + QCLASS */
    }

    /* Parse answer section */
    resp->ancount = (ancount > NFS_DNS_MAX_RR) ? NFS_DNS_MAX_RR : ancount;

    for (uint16_t i = 0; i < resp->ancount; i++) {
        struct nfs_dns_rr *rr = &resp->answers[i];

        /* Parse name */
        int consumed = nfs_dns_name_decode(data, len, offset,
                                            rr->name, sizeof(rr->name));
        if (consumed < 0) return -1;
        offset += (size_t)consumed;

        /* TYPE(2) + CLASS(2) + TTL(4) + RDLENGTH(2) = 10 bytes */
        if (offset + 10 > len) return -1;

        uint16_t rtype, rclass, rdlen;
        uint32_t ttl;
        memcpy(&rtype, data + offset, 2);
        memcpy(&rclass, data + offset + 2, 2);
        memcpy(&ttl, data + offset + 4, 4);
        memcpy(&rdlen, data + offset + 8, 2);

        rr->type = ntohs(rtype);
        rr->rclass = ntohs(rclass);
        rr->ttl = ntohl(ttl);
        rr->rdlength = ntohs(rdlen);

        offset += 10;

        if (offset + rr->rdlength > len) return -1;
        if (rr->rdlength > sizeof(rr->rdata)) return -1;

        memcpy(rr->rdata, data + offset, rr->rdlength);
        offset += rr->rdlength;
    }

    return 0;
}
