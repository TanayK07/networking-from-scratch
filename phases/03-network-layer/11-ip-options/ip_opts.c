#include "ip_opts.h"
#include <string.h>

int nfs_ip_opts_parse(const uint8_t *data, size_t len, struct nfs_ip_option *opts, size_t max_opts,
                      size_t *nfound) {
    if (!data || !opts || !nfound)
        return -1;

    *nfound = 0;
    size_t pos = 0;

    while (pos < len) {
        if (*nfound >= max_opts)
            break;

        uint8_t type = data[pos];

        /* End of Options List */
        if (type == NFS_IPOPT_EOL) {
            opts[*nfound].type = NFS_IPOPT_EOL;
            opts[*nfound].length = 0;
            (*nfound)++;
            break;
        }

        /* No Operation (single byte, no length) */
        if (type == NFS_IPOPT_NOP) {
            opts[*nfound].type = NFS_IPOPT_NOP;
            opts[*nfound].length = 0;
            (*nfound)++;
            pos++;
            continue;
        }

        /* Multi-byte option: type + length + data */
        if (pos + 1 >= len)
            return -1; /* Truncated: no length byte */

        uint8_t opt_len = data[pos + 1];

        /* Sanity: length must be >= 2 (type + length fields) */
        if (opt_len < 2)
            return -1;

        /* Sanity: option must not extend past the buffer */
        if (pos + opt_len > len)
            return -1;

        opts[*nfound].type = type;
        opts[*nfound].length = opt_len;

        size_t data_len = opt_len - 2;
        if (data_len > NFS_IPOPT_MAX_DATA)
            data_len = NFS_IPOPT_MAX_DATA;
        memcpy(opts[*nfound].data, data + pos + 2, data_len);

        (*nfound)++;
        pos += opt_len;
    }

    return 0;
}

int nfs_ip_opts_build_rr(int max_hops, uint8_t *out, size_t out_sz) {
    if (!out || max_hops < 1)
        return -1;

    /* Record Route format:
     *   type(1) + length(1) + pointer(1) + route_data(max_hops * 4)
     * Total = 3 + max_hops * 4
     * Pointer starts at 4 (1-based, first route slot is at offset 4) */
    int total = 3 + max_hops * 4;
    if (total > 40 || (size_t)total > out_sz)
        return -1;

    out[0] = NFS_IPOPT_RR;
    out[1] = (uint8_t)total;
    out[2] = 4; /* Pointer: first slot */
    memset(out + 3, 0, (size_t)(max_hops * 4));

    return total;
}

int nfs_ip_opts_build_ts(int max_entries, uint8_t *out, size_t out_sz) {
    if (!out || max_entries < 1)
        return -1;

    /* Timestamp format:
     *   type(1) + length(1) + pointer(1) + oflw_flag(1) + timestamps(max_entries * 4)
     * Total = 4 + max_entries * 4
     * Pointer starts at 5 (1-based, first timestamp at offset 5) */
    int total = 4 + max_entries * 4;
    if (total > 40 || (size_t)total > out_sz)
        return -1;

    out[0] = NFS_IPOPT_TS;
    out[1] = (uint8_t)total;
    out[2] = 5; /* Pointer: first timestamp slot */
    out[3] = 0; /* overflow(4 bits) | flag(4 bits): timestamps only */
    memset(out + 4, 0, (size_t)(max_entries * 4));

    return total;
}

int nfs_ip_opts_pad(uint8_t *opts, size_t len, size_t *padded_len) {
    if (!opts || !padded_len)
        return -1;

    /* Compute padding needed to reach 4-byte boundary */
    size_t target = (len + 3) & ~(size_t)3;
    if (target > 40)
        return -1;

    /* Fill with NOPs, then terminate with EOL */
    if (target == len) {
        /* Already aligned -- but we still need room for at least EOL
         * if the last byte is not already EOL. */
        *padded_len = len;
        return 0;
    }

    /* Pad with NOPs up to target-1, then EOL at target-1 */
    for (size_t i = len; i < target - 1; i++)
        opts[i] = NFS_IPOPT_NOP;
    opts[target - 1] = NFS_IPOPT_EOL;

    *padded_len = target;
    return 0;
}

const char *nfs_ip_opt_name(uint8_t type) {
    switch (type) {
    case NFS_IPOPT_EOL:
        return "EOL";
    case NFS_IPOPT_NOP:
        return "NOP";
    case NFS_IPOPT_RR:
        return "Record Route";
    case NFS_IPOPT_TS:
        return "Timestamp";
    case NFS_IPOPT_LSRR:
        return "Loose Source Route";
    case NFS_IPOPT_SSRR:
        return "Strict Source Route";
    default:
        return "Unknown";
    }
}
