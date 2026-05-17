/* main.c — SACK scoreboard demonstration.
 *
 * Simulates a receiver getting out-of-order segments and tracking
 * them with SACK blocks. Shows merging, hole detection, and
 * building SACK options for the wire. */

#include "sack.h"

#include <stdio.h>

static void print_scoreboard(const struct nfs_sack_scoreboard *sb) {
    printf("  cum_ack=%u, %zu blocks:", sb->cum_ack, sb->nblocks);
    for (size_t i = 0; i < sb->nblocks; i++)
        printf(" [%u, %u)", sb->blocks[i].left, sb->blocks[i].right);
    printf("\n");
}

int main(void) {
    printf("=== TCP SACK Scoreboard Demo ===\n\n");

    struct nfs_sack_scoreboard sb;
    nfs_sack_init(&sb, 1000);

    printf("Initial state:\n");
    print_scoreboard(&sb);

    /* Receiver got bytes 1000-1099, then a gap, then 1200-1299 */
    printf("\nReceived [1200, 1300):\n");
    nfs_sack_add_block(&sb, 1200, 1300);
    print_scoreboard(&sb);

    printf("\nReceived [1500, 1600):\n");
    nfs_sack_add_block(&sb, 1500, 1600);
    print_scoreboard(&sb);

    printf("\nReceived [1100, 1250) — overlaps with first block:\n");
    nfs_sack_add_block(&sb, 1100, 1250);
    print_scoreboard(&sb);

    /* Show holes */
    struct nfs_sack_block holes[8];
    size_t nholes = nfs_sack_holes(&sb, holes, 8);
    printf("\nHoles (need retransmission): %zu\n", nholes);
    for (size_t i = 0; i < nholes; i++)
        printf("  hole [%u, %u)\n", holes[i].left, holes[i].right);

    /* Build wire option */
    uint8_t wire[64];
    int n = nfs_sack_build_option(&sb, wire, sizeof(wire));
    printf("\nSACK option wire bytes (%d bytes):\n  ", n);
    for (int i = 0; i < n; i++)
        printf("%02x ", wire[i]);
    printf("\n");

    /* Advance cumulative ACK */
    printf("\nAdvance cum_ack to 1300:\n");
    nfs_sack_advance_cumack(&sb, 1300);
    print_scoreboard(&sb);

    nholes = nfs_sack_holes(&sb, holes, 8);
    printf("Remaining holes: %zu\n", nholes);
    for (size_t i = 0; i < nholes; i++)
        printf("  hole [%u, %u)\n", holes[i].left, holes[i].right);

    printf("\nDone.\n");
    return 0;
}
