/* TCP header parser, builder, and formatter (RFC 9293).
 *
 * struct nfs_tcp_hdr stores fields in host byte order after parsing.
 * nfs_tcp_build() converts back to network order for the wire.
 */

#include "tcp_hdr.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t rd32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static void wr16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

static void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

/* ------------------------------------------------------------------ */
/*  Data offset / flags extraction                                     */
/* ------------------------------------------------------------------ */

uint8_t nfs_tcp_data_offset(const struct nfs_tcp_hdr *h) {
    /* data_off_flags is in host byte order after parsing.
     * Top 4 bits = data offset. */
    return (uint8_t)((h->data_off_flags >> 12) & 0x0F);
}

uint16_t nfs_tcp_flags(const struct nfs_tcp_hdr *h) {
    /* Bottom 9 bits of data_off_flags (in host byte order). */
    return h->data_off_flags & 0x01FF;
}

int nfs_tcp_has_flag(const struct nfs_tcp_hdr *h, uint16_t flag) {
    return (nfs_tcp_flags(h) & flag) != 0;
}

/* ------------------------------------------------------------------ */
/*  Parse                                                              */
/* ------------------------------------------------------------------ */

int nfs_tcp_parse(const uint8_t *data, size_t len, struct nfs_tcp_hdr *hdr) {
    if (!data || !hdr || len < NFS_TCP_HDR_MIN_LEN)
        return -1;

    hdr->src_port       = rd16(data + 0);
    hdr->dst_port       = rd16(data + 2);
    hdr->seq            = rd32(data + 4);
    hdr->ack            = rd32(data + 8);
    hdr->data_off_flags = rd16(data + 12);
    hdr->window         = rd16(data + 14);
    hdr->checksum       = rd16(data + 16);
    hdr->urgent         = rd16(data + 18);

    /* Validate: data offset >= 5 (minimum 20 bytes). */
    uint8_t doff = nfs_tcp_data_offset(hdr);
    if (doff < 5)
        return -1;

    /* Also validate that we have enough bytes for the declared header. */
    if (len < (size_t)(doff * 4))
        return -1;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Build                                                              */
/* ------------------------------------------------------------------ */

int nfs_tcp_build(uint16_t src_port, uint16_t dst_port, uint32_t seq,
                  uint32_t ack, uint16_t flags, uint16_t window,
                  uint8_t *out, size_t out_sz) {
    if (!out || out_sz < NFS_TCP_HDR_MIN_LEN)
        return -1;

    memset(out, 0, NFS_TCP_HDR_MIN_LEN);

    wr16(out + 0, src_port);
    wr16(out + 2, dst_port);
    wr32(out + 4, seq);
    wr32(out + 8, ack);

    /* data_off_flags: data offset = 5 (20 bytes / 4), flags in bottom 9 bits.
     * Top 4 bits = 5 = 0101, shift left 12 -> 0x5000. */
    uint16_t dof = (uint16_t)(0x5000 | (flags & 0x01FF));
    wr16(out + 12, dof);

    wr16(out + 14, window);
    /* checksum = 0 (caller must compute separately) */
    /* urgent = 0 */

    return NFS_TCP_HDR_MIN_LEN;
}

/* ------------------------------------------------------------------ */
/*  Flag string                                                        */
/* ------------------------------------------------------------------ */

const char *nfs_tcp_flag_str(uint16_t flags) {
    /* Static buffer -- not thread-safe, but sufficient for this lesson. */
    static char buf[64];
    buf[0] = '\0';
    size_t pos = 0;

    struct { uint16_t mask; const char *name; } flag_table[] = {
        { NFS_TCP_CWR, "CWR" },
        { NFS_TCP_ECE, "ECE" },
        { NFS_TCP_URG, "URG" },
        { NFS_TCP_ACK, "ACK" },
        { NFS_TCP_PSH, "PSH" },
        { NFS_TCP_RST, "RST" },
        { NFS_TCP_SYN, "SYN" },
        { NFS_TCP_FIN, "FIN" },
    };

    for (size_t i = 0; i < sizeof(flag_table) / sizeof(flag_table[0]); i++) {
        if (flags & flag_table[i].mask) {
            if (pos > 0) {
                buf[pos++] = ',';
            }
            size_t nlen = strlen(flag_table[i].name);
            if (pos + nlen < sizeof(buf) - 1) {
                memcpy(buf + pos, flag_table[i].name, nlen);
                pos += nlen;
            }
        }
    }

    buf[pos] = '\0';
    return buf;
}

/* ------------------------------------------------------------------ */
/*  Format                                                             */
/* ------------------------------------------------------------------ */

void nfs_tcp_format(const struct nfs_tcp_hdr *h, char *buf, size_t sz) {
    if (!h || !buf || sz == 0)
        return;

    const char *fstr = nfs_tcp_flag_str(nfs_tcp_flags(h));
    snprintf(buf, sz, "%u\xe2\x86\x92%u seq=%u ack=%u [%s] win=%u",
             h->src_port, h->dst_port, h->seq, h->ack, fstr, h->window);
}
