#ifndef NFS_SACK_H
#define NFS_SACK_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP Selective Acknowledgment (SACK) — RFC 2018.
 *
 * SACK allows a receiver to inform the sender about non-contiguous
 * blocks of data that have been received. This lets the sender
 * retransmit only the missing segments (holes) instead of
 * everything after a loss.
 *
 * A SACK block [left, right) indicates that bytes from left
 * (inclusive) to right (exclusive) have been received.
 *
 * The SACK scoreboard tracks up to 4 blocks (the TCP option field
 * limit with timestamps).
 * --------------------------------------------------------------- */

/* A single SACK block: [left, right) — left inclusive, right exclusive. */
struct nfs_sack_block {
    uint32_t left;
    uint32_t right;
};

/* SACK scoreboard — tracks received out-of-order blocks.
 * TCP allows a maximum of 4 SACK blocks per option (with timestamps
 * taking 10 bytes, only 40 - 10 - 2 = 28 bytes remain = 3.5 blocks,
 * so 3 with timestamps; without timestamps, 4 is the max). */
#define NFS_SACK_MAX_BLOCKS 4

struct nfs_sack_scoreboard {
    struct nfs_sack_block blocks[NFS_SACK_MAX_BLOCKS];
    size_t nblocks;   /* number of active SACK blocks */
    uint32_t cum_ack; /* cumulative ACK (everything below is received) */
};

/* Initialize the scoreboard with an initial cumulative ACK. */
void nfs_sack_init(struct nfs_sack_scoreboard *sb, uint32_t initial_ack);

/* Add a SACK block. Merges overlapping and adjacent blocks.
 * Keeps the most recently added block first (per RFC 2018).
 * Returns the current number of blocks. */
int nfs_sack_add_block(struct nfs_sack_scoreboard *sb, uint32_t left, uint32_t right);

/* Check if a specific sequence number is covered by any SACK block.
 * Returns 1 if SACKed, 0 if not. */
int nfs_sack_is_sacked(const struct nfs_sack_scoreboard *sb, uint32_t seq);

/* Advance the cumulative ACK. Removes any SACK blocks that fall
 * entirely below the new cumulative ACK. */
void nfs_sack_advance_cumack(struct nfs_sack_scoreboard *sb, uint32_t new_ack);

/* Build a SACK TCP option in wire format.
 * Format: kind=5, len=2+8*nblocks, then nblocks pairs of (left, right)
 * in network byte order.
 * Returns bytes written, or -1 if buffer too small. */
int nfs_sack_build_option(const struct nfs_sack_scoreboard *sb, uint8_t *out, size_t out_sz);

/* Parse a SACK option from wire bytes.
 * data points to the option starting at kind byte.
 * Fills blocks array, sets *nfound.
 * Returns 0 on success, -1 on error. */
int nfs_sack_parse_option(const uint8_t *data, size_t len, struct nfs_sack_block *blocks,
                          size_t max_blocks, size_t *nfound);

/* Find holes between cum_ack and SACK blocks (these need retransmission).
 * A hole is a gap [hole.left, hole.right) between cum_ack and a SACK block,
 * or between two SACK blocks.
 * Returns the number of holes found. */
size_t nfs_sack_holes(const struct nfs_sack_scoreboard *sb, struct nfs_sack_block *holes,
                      size_t max_holes);

#endif /* NFS_SACK_H */
