/* What is a Network — implementation. */

#include "network.h"
#include <string.h>
#include <arpa/inet.h>

/* ---- Packet build -------------------------------------------- */

int nfs_pkt_build(uint8_t *buf, size_t buf_sz,
                  uint8_t version, uint8_t type, uint8_t flags,
                  uint32_t id,
                  const uint8_t *payload, size_t payload_len)
{
    if (!buf)
        return -1;
    if (payload_len > 0 && !payload)
        return -1;

    size_t total = NFS_PKT_HDR_SIZE + payload_len;
    if (total > buf_sz || total > 0xFFFF)
        return -1;

    struct nfs_pkt_header *h = (struct nfs_pkt_header *)buf;
    h->ver_type = (uint8_t)(((version & 0x0F) << 4) | (type & 0x0F));
    h->flags    = flags;
    h->length   = htons((uint16_t)total);
    h->id       = htonl(id);

    if (payload_len > 0)
        memcpy(buf + NFS_PKT_HDR_SIZE, payload, payload_len);

    return (int)total;
}

/* ---- Packet parse -------------------------------------------- */

int nfs_pkt_parse(const uint8_t *buf, size_t buf_len,
                  struct nfs_pkt_header *hdr,
                  const uint8_t **payload_out, size_t *payload_len_out)
{
    if (!buf || !hdr || !payload_out || !payload_len_out)
        return -1;

    if (buf_len < NFS_PKT_HDR_SIZE)
        return -1;

    /* Copy raw header */
    memcpy(hdr, buf, NFS_PKT_HDR_SIZE);

    /* Validate length field against actual buffer */
    uint16_t wire_len = ntohs(hdr->length);
    if (wire_len > buf_len || wire_len < NFS_PKT_HDR_SIZE)
        return -1;

    *payload_out     = buf + NFS_PKT_HDR_SIZE;
    *payload_len_out = wire_len - NFS_PKT_HDR_SIZE;

    return 0;
}

/* ---- Internet checksum (RFC 1071) ----------------------------- */

uint16_t nfs_inet_checksum(const uint8_t *data, size_t len)
{
    if (!data || len == 0)
        return 0xFFFF;  /* all-zeros data => 0xFFFF */

    uint32_t sum = 0;

    /* Sum 16-bit words */
    size_t i;
    for (i = 0; i + 1 < len; i += 2) {
        uint16_t word = (uint16_t)((data[i] << 8) | data[i + 1]);
        sum += word;
    }

    /* If odd length, pad last byte with zero */
    if (len & 1) {
        uint16_t word = (uint16_t)(data[len - 1] << 8);
        sum += word;
    }

    /* Fold 32-bit sum to 16 bits */
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)(~sum & 0xFFFF);
}

/* ---- Encapsulate / decapsulate -------------------------------- */

int nfs_encapsulate(uint8_t *buf, size_t buf_sz,
                    uint8_t type, uint8_t flags, uint32_t id,
                    const uint8_t *payload, size_t payload_len)
{
    return nfs_pkt_build(buf, buf_sz,
                         NFS_PKT_VERSION, type, flags, id,
                         payload, payload_len);
}

int nfs_decapsulate(const uint8_t *buf, size_t buf_len,
                    struct nfs_pkt_header *hdr,
                    const uint8_t **payload_out, size_t *payload_len_out)
{
    return nfs_pkt_parse(buf, buf_len, hdr, payload_out, payload_len_out);
}
