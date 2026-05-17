/* tcp_opts.c — TCP option parser and builder.
 *
 * TCP options live between the 20-byte fixed header and the payload.
 * The data_offset field tells us how many 32-bit words the header spans;
 * options occupy (data_offset - 5) * 4 bytes.
 *
 * Options are TLV (Type-Length-Value) encoded, except for the single-byte
 * EOL (0) and NOP (1) options which have no length/value. */

#include "tcp_opts.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Parsing
 * --------------------------------------------------------------- */

int nfs_tcp_opts_parse(const uint8_t *data, size_t len, struct nfs_tcp_option *opts,
                       size_t max_opts, size_t *nfound) {
    size_t pos = 0;
    size_t count = 0;

    while (pos < len && count < max_opts) {
        uint8_t kind = data[pos];

        /* End of Option List */
        if (kind == NFS_TCPOPT_EOL)
            break;

        /* NOP — single byte, no length field */
        if (kind == NFS_TCPOPT_NOP) {
            opts[count].kind = NFS_TCPOPT_NOP;
            opts[count].length = 1;
            count++;
            pos++;
            continue;
        }

        /* All other options have at least kind + length */
        if (pos + 1 >= len)
            return -1; /* truncated */

        uint8_t opt_len = data[pos + 1];

        /* Length must be at least 2 (kind + length bytes) */
        if (opt_len < 2)
            return -1; /* malformed */

        /* Enough data remaining? */
        if (pos + opt_len > len)
            return -1; /* truncated */

        opts[count].kind = kind;
        opts[count].length = opt_len;

        switch (kind) {
        case NFS_TCPOPT_MSS:
            if (opt_len != 4)
                return -1;
            memcpy(&opts[count].data.mss, &data[pos + 2], 2);
            opts[count].data.mss = ntohs(opts[count].data.mss);
            break;

        case NFS_TCPOPT_WSCALE:
            if (opt_len != 3)
                return -1;
            opts[count].data.wscale = data[pos + 2];
            break;

        case NFS_TCPOPT_SACK_PERM:
            if (opt_len != 2)
                return -1;
            break;

        case NFS_TCPOPT_SACK: {
            /* SACK: kind=5, len=variable, each block is 8 bytes (left+right) */
            size_t block_bytes = (size_t)(opt_len - 2);
            if (block_bytes % 8 != 0)
                return -1;
            size_t nblocks = block_bytes / 8;
            if (nblocks > NFS_TCPOPT_MAX_SACK_BLOCKS)
                return -1;
            opts[count].data.sack.nblocks = nblocks;
            for (size_t b = 0; b < nblocks; b++) {
                uint32_t left, right;
                memcpy(&left, &data[pos + 2 + b * 8], 4);
                memcpy(&right, &data[pos + 2 + b * 8 + 4], 4);
                opts[count].data.sack.blocks[b * 2] = ntohl(left);
                opts[count].data.sack.blocks[b * 2 + 1] = ntohl(right);
            }
            break;
        }

        case NFS_TCPOPT_TIMESTAMPS:
            if (opt_len != 10)
                return -1;
            memcpy(&opts[count].data.timestamps.ts_val, &data[pos + 2], 4);
            memcpy(&opts[count].data.timestamps.ts_ecr, &data[pos + 6], 4);
            opts[count].data.timestamps.ts_val = ntohl(opts[count].data.timestamps.ts_val);
            opts[count].data.timestamps.ts_ecr = ntohl(opts[count].data.timestamps.ts_ecr);
            break;

        default:
            /* Unknown option — skip it (we still record kind/length) */
            break;
        }

        count++;
        pos += opt_len;
    }

    *nfound = count;
    return 0;
}

/* ---------------------------------------------------------------
 * Building
 * --------------------------------------------------------------- */

int nfs_tcp_opts_build_mss(uint16_t mss, uint8_t *out, size_t out_sz) {
    if (out_sz < 4)
        return -1;
    out[0] = NFS_TCPOPT_MSS;
    out[1] = 4;
    uint16_t net_mss = htons(mss);
    memcpy(&out[2], &net_mss, 2);
    return 4;
}

int nfs_tcp_opts_build_wscale(uint8_t shift, uint8_t *out, size_t out_sz) {
    if (out_sz < 3)
        return -1;
    out[0] = NFS_TCPOPT_WSCALE;
    out[1] = 3;
    out[2] = shift;
    return 3;
}

int nfs_tcp_opts_build_timestamps(uint32_t ts_val, uint32_t ts_ecr, uint8_t *out, size_t out_sz) {
    if (out_sz < 10)
        return -1;
    out[0] = NFS_TCPOPT_TIMESTAMPS;
    out[1] = 10;
    uint32_t net_val = htonl(ts_val);
    uint32_t net_ecr = htonl(ts_ecr);
    memcpy(&out[2], &net_val, 4);
    memcpy(&out[6], &net_ecr, 4);
    return 10;
}

int nfs_tcp_opts_build_sack_perm(uint8_t *out, size_t out_sz) {
    if (out_sz < 2)
        return -1;
    out[0] = NFS_TCPOPT_SACK_PERM;
    out[1] = 2;
    return 2;
}

/* ---------------------------------------------------------------
 * Padding to 4-byte boundary
 * --------------------------------------------------------------- */

int nfs_tcp_opts_pad(uint8_t *opts, size_t opts_len, size_t *padded_len) {
    size_t aligned = (opts_len + 3) & ~(size_t)3;

    /* Fill remaining bytes: NOPs then EOL at the end.
     * Actually, the convention is NOP padding before options, but
     * for trailing padding we use EOL + NOP fill. The EOL means
     * "stop parsing", and we pad with NOPs to alignment. */
    size_t pad = aligned - opts_len;

    if (pad == 0) {
        *padded_len = opts_len;
        return 0;
    }

    /* Fill pad bytes with EOL (first) then NOP */
    opts[opts_len] = NFS_TCPOPT_EOL;
    for (size_t i = 1; i < pad; i++) {
        opts[opts_len + i] = NFS_TCPOPT_NOP;
    }

    *padded_len = aligned;
    return 0;
}

/* ---------------------------------------------------------------
 * Formatting
 * --------------------------------------------------------------- */

void nfs_tcp_opt_format(const struct nfs_tcp_option *opt, char *buf, size_t sz) {
    switch (opt->kind) {
    case NFS_TCPOPT_EOL:
        snprintf(buf, sz, "EOL");
        break;
    case NFS_TCPOPT_NOP:
        snprintf(buf, sz, "NOP");
        break;
    case NFS_TCPOPT_MSS:
        snprintf(buf, sz, "MSS=%u", opt->data.mss);
        break;
    case NFS_TCPOPT_WSCALE:
        snprintf(buf, sz, "WScale=%u", opt->data.wscale);
        break;
    case NFS_TCPOPT_SACK_PERM:
        snprintf(buf, sz, "SACK-Permitted");
        break;
    case NFS_TCPOPT_SACK: {
        int off = snprintf(buf, sz, "SACK[");
        for (size_t i = 0; i < opt->data.sack.nblocks && (size_t)off < sz; i++) {
            if (i > 0)
                off += snprintf(buf + off, sz - (size_t)off, ", ");
            off += snprintf(buf + off, sz - (size_t)off, "%u-%u", opt->data.sack.blocks[i * 2],
                            opt->data.sack.blocks[i * 2 + 1]);
        }
        if ((size_t)off < sz)
            snprintf(buf + off, sz - (size_t)off, "]");
        break;
    }
    case NFS_TCPOPT_TIMESTAMPS:
        snprintf(buf, sz, "TS val=%u ecr=%u", opt->data.timestamps.ts_val,
                 opt->data.timestamps.ts_ecr);
        break;
    default:
        snprintf(buf, sz, "Unknown(kind=%u, len=%u)", opt->kind, opt->length);
        break;
    }
}
