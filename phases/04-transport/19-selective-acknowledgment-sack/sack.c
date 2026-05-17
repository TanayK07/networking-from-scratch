/* sack.c — TCP SACK scoreboard implementation.
 *
 * Implements SACK block tracking per RFC 2018:
 *   - Maintains up to 4 SACK blocks
 *   - Merges overlapping/adjacent blocks
 *   - Most recently added block goes first
 *   - Finds retransmission holes between cumulative ACK and SACK blocks */

#include "sack.h"

#include <arpa/inet.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Init
 * --------------------------------------------------------------- */

void nfs_sack_init(struct nfs_sack_scoreboard *sb, uint32_t initial_ack) {
    memset(sb, 0, sizeof(*sb));
    sb->cum_ack = initial_ack;
}

/* ---------------------------------------------------------------
 * Block management — sort by left edge ascending (for hole finding)
 * then put most recent first for RFC 2018 compliance when building.
 * --------------------------------------------------------------- */

/* Sort blocks by left edge ascending. */
static void sort_blocks(struct nfs_sack_block *blocks, size_t n) {
    /* Simple insertion sort — n <= 4 */
    for (size_t i = 1; i < n; i++) {
        struct nfs_sack_block tmp = blocks[i];
        size_t j = i;
        while (j > 0 && blocks[j - 1].left > tmp.left) {
            blocks[j] = blocks[j - 1];
            j--;
        }
        blocks[j] = tmp;
    }
}

/* Merge overlapping and adjacent blocks. Returns new count. */
static size_t merge_blocks(struct nfs_sack_block *blocks, size_t n) {
    if (n <= 1)
        return n;

    sort_blocks(blocks, n);

    size_t out = 0;
    for (size_t i = 1; i < n; i++) {
        if (blocks[i].left <= blocks[out].right) {
            /* Overlapping or adjacent — merge */
            if (blocks[i].right > blocks[out].right)
                blocks[out].right = blocks[i].right;
        } else {
            out++;
            blocks[out] = blocks[i];
        }
    }
    return out + 1;
}

int nfs_sack_add_block(struct nfs_sack_scoreboard *sb, uint32_t left, uint32_t right) {
    /* Add the new block */
    if (sb->nblocks < NFS_SACK_MAX_BLOCKS) {
        sb->blocks[sb->nblocks].left = left;
        sb->blocks[sb->nblocks].right = right;
        sb->nblocks++;
    } else {
        /* Replace the last block (least recent after we rearrange) */
        sb->blocks[NFS_SACK_MAX_BLOCKS - 1].left = left;
        sb->blocks[NFS_SACK_MAX_BLOCKS - 1].right = right;
    }

    /* Merge overlapping blocks */
    sb->nblocks = merge_blocks(sb->blocks, sb->nblocks);

    /* Move the block containing [left, right) to position 0 (most recent first).
     * Find which merged block covers the new range. */
    size_t recent_idx = 0;
    for (size_t i = 0; i < sb->nblocks; i++) {
        if (sb->blocks[i].left <= left && sb->blocks[i].right >= right) {
            recent_idx = i;
            break;
        }
    }

    if (recent_idx != 0) {
        struct nfs_sack_block tmp = sb->blocks[recent_idx];
        /* Shift others down */
        for (size_t i = recent_idx; i > 0; i--)
            sb->blocks[i] = sb->blocks[i - 1];
        sb->blocks[0] = tmp;
    }

    return (int)sb->nblocks;
}

/* ---------------------------------------------------------------
 * Queries
 * --------------------------------------------------------------- */

int nfs_sack_is_sacked(const struct nfs_sack_scoreboard *sb, uint32_t seq) {
    for (size_t i = 0; i < sb->nblocks; i++) {
        if (seq >= sb->blocks[i].left && seq < sb->blocks[i].right)
            return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------
 * Cumulative ACK advancement
 * --------------------------------------------------------------- */

void nfs_sack_advance_cumack(struct nfs_sack_scoreboard *sb, uint32_t new_ack) {
    if (new_ack <= sb->cum_ack)
        return;

    sb->cum_ack = new_ack;

    /* Remove blocks that fall entirely below cum_ack */
    size_t write = 0;
    for (size_t i = 0; i < sb->nblocks; i++) {
        if (sb->blocks[i].right > new_ack) {
            /* Trim left edge if needed */
            if (sb->blocks[i].left < new_ack)
                sb->blocks[i].left = new_ack;
            sb->blocks[write++] = sb->blocks[i];
        }
    }
    sb->nblocks = write;
}

/* ---------------------------------------------------------------
 * Build SACK option for wire
 * --------------------------------------------------------------- */

int nfs_sack_build_option(const struct nfs_sack_scoreboard *sb, uint8_t *out, size_t out_sz) {
    if (sb->nblocks == 0)
        return 0;

    size_t needed = 2 + 8 * sb->nblocks; /* kind + len + blocks */
    if (out_sz < needed)
        return -1;

    out[0] = 5; /* SACK kind */
    out[1] = (uint8_t)needed;

    for (size_t i = 0; i < sb->nblocks; i++) {
        uint32_t left = htonl(sb->blocks[i].left);
        uint32_t right = htonl(sb->blocks[i].right);
        memcpy(&out[2 + i * 8], &left, 4);
        memcpy(&out[2 + i * 8 + 4], &right, 4);
    }

    return (int)needed;
}

/* ---------------------------------------------------------------
 * Parse SACK option from wire
 * --------------------------------------------------------------- */

int nfs_sack_parse_option(const uint8_t *data, size_t len, struct nfs_sack_block *blocks,
                          size_t max_blocks, size_t *nfound) {
    if (len < 2)
        return -1;

    if (data[0] != 5)
        return -1;

    uint8_t opt_len = data[1];
    if (opt_len < 2 || (size_t)opt_len > len)
        return -1;

    size_t block_bytes = (size_t)(opt_len - 2);
    if (block_bytes % 8 != 0)
        return -1;

    size_t nblocks = block_bytes / 8;
    if (nblocks > max_blocks)
        nblocks = max_blocks;

    for (size_t i = 0; i < nblocks; i++) {
        uint32_t left, right;
        memcpy(&left, &data[2 + i * 8], 4);
        memcpy(&right, &data[2 + i * 8 + 4], 4);
        blocks[i].left = ntohl(left);
        blocks[i].right = ntohl(right);
    }

    *nfound = nblocks;
    return 0;
}

/* ---------------------------------------------------------------
 * Find retransmission holes
 * --------------------------------------------------------------- */

size_t nfs_sack_holes(const struct nfs_sack_scoreboard *sb, struct nfs_sack_block *holes,
                      size_t max_holes) {
    if (sb->nblocks == 0)
        return 0;

    /* We need blocks sorted by left edge for hole detection.
     * Copy and sort since blocks[0] may be out of order (most recent first). */
    struct nfs_sack_block sorted[NFS_SACK_MAX_BLOCKS];
    memcpy(sorted, sb->blocks, sb->nblocks * sizeof(struct nfs_sack_block));
    sort_blocks(sorted, sb->nblocks);

    size_t nholes = 0;
    uint32_t cursor = sb->cum_ack;

    for (size_t i = 0; i < sb->nblocks && nholes < max_holes; i++) {
        if (sorted[i].left > cursor) {
            holes[nholes].left = cursor;
            holes[nholes].right = sorted[i].left;
            nholes++;
        }
        if (sorted[i].right > cursor)
            cursor = sorted[i].right;
    }

    return nholes;
}
