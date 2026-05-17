/* CLI: demonstrate TCP sequence number arithmetic.
 *
 * Usage:
 *   $ ./seqnum compare 0xFFFFFFF0 0x00000010
 *   0xFFFFFFF0 < 0x00000010 = true (diff = -32)
 *
 *   $ ./seqnum add 0xFFFFFFFF 1
 *   0xFFFFFFFF + 1 = 0x00000000
 *
 *   $ ./seqnum window 5 3 10
 *   5 in [3, 3+10) = true
 */

#include "seqnum.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static nfs_seq_t parse_seq(const char *s) {
    return (nfs_seq_t)strtoul(s, NULL, 0);
}

int main(int argc, char **argv) {
    if (argc >= 4 && strcmp(argv[1], "compare") == 0) {
        nfs_seq_t a = parse_seq(argv[2]);
        nfs_seq_t b = parse_seq(argv[3]);

        printf("0x%08X vs 0x%08X:\n", a, b);
        printf("  a < b  = %s\n", nfs_seq_lt(a, b) ? "true" : "false");
        printf("  a <= b = %s\n", nfs_seq_le(a, b) ? "true" : "false");
        printf("  a > b  = %s\n", nfs_seq_gt(a, b) ? "true" : "false");
        printf("  a >= b = %s\n", nfs_seq_ge(a, b) ? "true" : "false");
        printf("  diff   = %d\n", nfs_seq_diff(a, b));
        return 0;
    }

    if (argc >= 4 && strcmp(argv[1], "add") == 0) {
        nfs_seq_t seq = parse_seq(argv[2]);
        uint32_t delta = (uint32_t)strtoul(argv[3], NULL, 0);

        nfs_seq_t result = nfs_seq_add(seq, delta);
        printf("0x%08X + %u = 0x%08X\n", seq, delta, result);
        return 0;
    }

    if (argc >= 5 && strcmp(argv[1], "window") == 0) {
        nfs_seq_t seq = parse_seq(argv[2]);
        nfs_seq_t start = parse_seq(argv[3]);
        uint32_t size = (uint32_t)strtoul(argv[4], NULL, 0);

        int in = nfs_seq_in_window(seq, start, size);
        printf("0x%08X in [0x%08X, 0x%08X + %u) = %s\n", seq, start, start, size,
               in ? "true" : "false");
        return 0;
    }

    if (argc >= 5 && strcmp(argv[1], "between") == 0) {
        nfs_seq_t seq = parse_seq(argv[2]);
        nfs_seq_t lo = parse_seq(argv[3]);
        nfs_seq_t hi = parse_seq(argv[4]);

        int bet = nfs_seq_between(seq, lo, hi);
        printf("0x%08X in [0x%08X, 0x%08X] = %s\n", seq, lo, hi, bet ? "true" : "false");
        return 0;
    }

    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s compare <a> <b>\n", argv[0]);
    fprintf(stderr, "  %s add <seq> <delta>\n", argv[0]);
    fprintf(stderr, "  %s window <seq> <start> <size>\n", argv[0]);
    fprintf(stderr, "  %s between <seq> <lo> <hi>\n", argv[0]);
    return 1;
}
