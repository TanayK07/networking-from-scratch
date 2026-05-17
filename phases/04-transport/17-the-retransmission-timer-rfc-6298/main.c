/* main.c — RFC 6298 RTO calculator demonstration.
 *
 * Simulates a series of RTT measurements and shows how SRTT, RTTVAR,
 * and RTO evolve over time. */

#include "rto.h"

#include <stdio.h>

int main(void) {
    printf("=== RFC 6298 RTO Calculator Demo ===\n\n");

    struct nfs_rto_state s;
    nfs_rto_init(&s);

    printf("Initial state: RTO=%.3f s\n\n", nfs_rto_get(&s));

    /* Simulate RTT samples from a stable connection (~50ms) */
    double samples[] = {0.050, 0.048, 0.052, 0.047, 0.055, 0.051, 0.049, 0.053, 0.048, 0.050};
    int n = (int)(sizeof(samples) / sizeof(samples[0]));

    printf("%-6s  %-10s  %-10s  %-10s  %-10s\n", "Sample", "RTT(ms)", "SRTT(ms)", "RTTVAR(ms)",
           "RTO(ms)");
    printf("------  ----------  ----------  ----------  ----------\n");

    for (int i = 0; i < n; i++) {
        nfs_rto_update(&s, samples[i]);
        printf("%-6d  %-10.1f  %-10.3f  %-10.3f  %-10.3f\n", i + 1, samples[i] * 1000.0,
               nfs_rto_srtt(&s) * 1000.0, nfs_rto_rttvar(&s) * 1000.0, nfs_rto_get(&s) * 1000.0);
    }

    /* Demonstrate exponential backoff */
    printf("\nExponential backoff sequence:\n");
    for (int i = 0; i < 8; i++) {
        printf("  Backoff %d: RTO=%.3f s\n", i, nfs_rto_get(&s));
        nfs_rto_backoff(&s);
    }
    printf("  Final:     RTO=%.3f s (capped at %.1f s)\n", nfs_rto_get(&s), NFS_RTO_MAX);

    printf("\nDone.\n");
    return 0;
}
