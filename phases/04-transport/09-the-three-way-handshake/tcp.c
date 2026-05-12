/* TCP three-way handshake — segment building, parsing, and state tracking.
 *
 * This implementation covers the SYN → SYN-ACK → ACK exchange described
 * in RFC 9293 Section 3.5. It operates entirely in user-space on byte
 * buffers, with no socket or kernel dependency.
 */

#define _POSIX_C_SOURCE 199309L   /* clock_gettime */

#include "tcp.h"
#include "../../../common/c/checksum.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------------------------------------------------------------
 * ISN generation
 * --------------------------------------------------------------- */

uint32_t nfs_tcp_generate_isn(void) {
    /* RFC 6528 recommends ISN = F(local_ip, local_port, remote_ip,
     * remote_port, secret_key) where F is a PRF. We approximate this
     * with time + randomness for educational purposes.
     *
     * Real stacks use a keyed hash (SipHash, MD5) over the 4-tuple
     * plus a secret to prevent off-path injection. */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    /* Mix nanoseconds with rand() to get reasonable entropy. */
    uint32_t t = (uint32_t)(ts.tv_nsec ^ (ts.tv_sec * 1000000000LL));
    uint32_t r = (uint32_t)rand();
    return t ^ (r << 16) ^ (r >> 16);
}

/* ---------------------------------------------------------------
 * Context initialization
 * --------------------------------------------------------------- */

void nfs_tcp_handshake_init(struct nfs_tcp_handshake *ctx,
                            uint16_t local_port,
                            uint16_t remote_port) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->state       = NFS_TCP_CLOSED;
    ctx->local_port  = local_port;
    ctx->remote_port = remote_port;
}

/* ---------------------------------------------------------------
 * Parsing
 * --------------------------------------------------------------- */

int nfs_tcp_parse(const uint8_t *buf, size_t len,
                  struct nfs_tcp_hdr *out) {
    if (!buf || !out || len < sizeof(struct nfs_tcp_hdr))
        return -1;

    /* Copy the raw bytes, then convert multi-byte fields to host
     * order so callers don't have to sprinkle ntohs/ntohl. */
    memcpy(out, buf, sizeof(*out));
    out->src_port   = ntohs(out->src_port);
    out->dst_port   = ntohs(out->dst_port);
    out->seq_num    = ntohl(out->seq_num);
    out->ack_num    = ntohl(out->ack_num);
    out->window     = ntohs(out->window);
    out->checksum   = ntohs(out->checksum);
    out->urgent_ptr = ntohs(out->urgent_ptr);

    /* Validate data offset: minimum is 5 (20 bytes). */
    uint8_t data_off = (out->data_off_rsv >> 4) & 0x0F;
    if (data_off < 5)
        return -1;

    return 0;
}

/* ---------------------------------------------------------------
 * Internal: serialize a header struct (host order) into a wire-
 * format buffer (network order).
 * --------------------------------------------------------------- */

static size_t serialize_hdr(const struct nfs_tcp_hdr *h,
                            uint8_t *buf, size_t len) {
    if (len < sizeof(struct nfs_tcp_hdr))
        return 0;

    struct nfs_tcp_hdr wire;
    wire.src_port    = htons(h->src_port);
    wire.dst_port    = htons(h->dst_port);
    wire.seq_num     = htonl(h->seq_num);
    wire.ack_num     = htonl(h->ack_num);
    wire.data_off_rsv = h->data_off_rsv;
    wire.flags       = h->flags;
    wire.window      = htons(h->window);
    wire.checksum    = htons(h->checksum);
    wire.urgent_ptr  = htons(h->urgent_ptr);

    memcpy(buf, &wire, sizeof(wire));
    return sizeof(wire);
}

/* ---------------------------------------------------------------
 * Build SYN (step 1: client → server)
 * --------------------------------------------------------------- */

size_t nfs_tcp_build_syn(struct nfs_tcp_handshake *ctx,
                         uint8_t *buf, size_t len) {
    if (!ctx || !buf || len < sizeof(struct nfs_tcp_hdr))
        return 0;

    ctx->local_seq = nfs_tcp_generate_isn();

    struct nfs_tcp_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.src_port     = ctx->local_port;
    hdr.dst_port     = ctx->remote_port;
    hdr.seq_num      = ctx->local_seq;
    hdr.ack_num      = 0;
    hdr.data_off_rsv = (5 << 4);   /* 20 bytes, no options */
    hdr.flags        = NFS_TCP_SYN;
    hdr.window       = 65535;

    ctx->state = NFS_TCP_SYN_SENT;
    return serialize_hdr(&hdr, buf, len);
}

/* ---------------------------------------------------------------
 * Build SYN-ACK (step 2: server → client)
 * --------------------------------------------------------------- */

size_t nfs_tcp_build_synack(struct nfs_tcp_handshake *ctx,
                            const struct nfs_tcp_hdr *syn_hdr,
                            uint8_t *buf, size_t len) {
    if (!ctx || !syn_hdr || !buf || len < sizeof(struct nfs_tcp_hdr))
        return 0;

    /* The incoming SYN must have the SYN flag set and ACK clear. */
    if (!(syn_hdr->flags & NFS_TCP_SYN))
        return 0;

    ctx->remote_seq = syn_hdr->seq_num;
    ctx->local_seq  = nfs_tcp_generate_isn();

    struct nfs_tcp_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.src_port     = ctx->local_port;
    hdr.dst_port     = ctx->remote_port;
    hdr.seq_num      = ctx->local_seq;
    hdr.ack_num      = ctx->remote_seq + 1;  /* SYN consumes one seq */
    hdr.data_off_rsv = (5 << 4);
    hdr.flags        = NFS_TCP_SYN | NFS_TCP_ACK;
    hdr.window       = 65535;

    ctx->state = NFS_TCP_SYN_RECEIVED;
    return serialize_hdr(&hdr, buf, len);
}

/* ---------------------------------------------------------------
 * Build ACK (step 3: client → server)
 * --------------------------------------------------------------- */

size_t nfs_tcp_build_ack(struct nfs_tcp_handshake *ctx,
                         const struct nfs_tcp_hdr *synack_hdr,
                         uint8_t *buf, size_t len) {
    if (!ctx || !synack_hdr || !buf || len < sizeof(struct nfs_tcp_hdr))
        return 0;

    /* Must be a SYN-ACK. */
    if ((synack_hdr->flags & (NFS_TCP_SYN | NFS_TCP_ACK))
        != (NFS_TCP_SYN | NFS_TCP_ACK))
        return 0;

    /* Validate: the SYN-ACK must acknowledge our SYN. */
    if (synack_hdr->ack_num != ctx->local_seq + 1)
        return 0;

    ctx->remote_seq = synack_hdr->seq_num;

    struct nfs_tcp_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.src_port     = ctx->local_port;
    hdr.dst_port     = ctx->remote_port;
    hdr.seq_num      = ctx->local_seq + 1;   /* next expected by peer */
    hdr.ack_num      = ctx->remote_seq + 1;  /* SYN-ACK consumes one */
    hdr.data_off_rsv = (5 << 4);
    hdr.flags        = NFS_TCP_ACK;
    hdr.window       = 65535;

    ctx->state = NFS_TCP_ESTABLISHED;
    return serialize_hdr(&hdr, buf, len);
}

/* ---------------------------------------------------------------
 * TCP pseudo-header checksum (IPv4, RFC 9293 Section 3.1)
 *
 * The pseudo-header prepended to the TCP segment for checksumming:
 *
 *     +--------+--------+--------+--------+
 *     |           source address           |
 *     +--------+--------+--------+--------+
 *     |         destination address        |
 *     +--------+--------+--------+--------+
 *     |  zero  | proto  |   TCP length     |
 *     +--------+--------+--------+--------+
 *
 * proto = 6 (IPPROTO_TCP).
 * --------------------------------------------------------------- */

uint16_t nfs_tcp_checksum_pseudo(const uint8_t *src_ip,
                                 const uint8_t *dst_ip,
                                 const uint8_t *tcp_buf,
                                 size_t tcp_len) {
    /* Build the 12-byte pseudo-header. */
    uint8_t pseudo[12];
    memcpy(pseudo + 0, src_ip, 4);
    memcpy(pseudo + 4, dst_ip, 4);
    pseudo[8]  = 0;
    pseudo[9]  = 6;  /* IPPROTO_TCP */
    pseudo[10] = (uint8_t)(tcp_len >> 8);
    pseudo[11] = (uint8_t)(tcp_len & 0xFF);

    /* Accumulate: pseudo-header, then the TCP segment. */
    uint32_t sum = 0;
    sum = internet_checksum_partial(pseudo, sizeof(pseudo), sum);
    sum = internet_checksum_partial(tcp_buf, tcp_len, sum);
    return internet_checksum_fold(sum);
}
