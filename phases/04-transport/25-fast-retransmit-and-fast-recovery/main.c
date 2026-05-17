#include "fast_recovery.h"
#include <stdio.h>

/* ---------------------------------------------------------------
 * Demo: TCP Reno fast retransmit / fast recovery scenario.
 *
 * Scenario:
 *   Sender has cwnd=10*MSS.  One segment is lost.  Receiver
 *   sends duplicate ACKs.  After 3 dup ACKs, fast retransmit
 *   triggers.  Then a new ACK covers the recovery point.
 * --------------------------------------------------------------- */

static const char *result_str(int r) {
    switch (r) {
    case NFS_FR_NORMAL:
        return "NORMAL";
    case NFS_FR_FAST_RETRANSMIT:
        return "FAST_RETRANSMIT";
    case NFS_FR_RECOVERY_EXIT:
        return "RECOVERY_EXIT";
    default:
        return "UNKNOWN";
    }
}

int main(void) {
    printf("=== Fast Retransmit & Fast Recovery (Reno) Demo ===\n\n");

    const uint32_t mss = 1460;
    struct nfs_fr_state fr;
    nfs_fr_init(&fr, mss, 10 * mss);

    printf("Initial: cwnd=%u, ssthresh=%u\n\n", nfs_fr_cwnd(&fr), fr.ssthresh);

    /* Simulate a new ACK advancing to seq 5000 */
    int r = nfs_fr_on_ack(&fr, 5000);
    printf("ACK 5000: %s, cwnd=%u\n", result_str(r), nfs_fr_cwnd(&fr));

    /* Segment at seq 5000 is lost.  Receiver keeps ACKing 5000. */
    printf("\n--- Segment 5000 lost, receiver sends dup ACKs ---\n");
    for (int i = 1; i <= 5; i++) {
        r = nfs_fr_on_ack(&fr, 5000);
        printf("Dup ACK #%d: %s, cwnd=%u, in_recovery=%d\n", i, result_str(r), nfs_fr_cwnd(&fr),
               nfs_fr_in_recovery(&fr));
        if (r == NFS_FR_FAST_RETRANSMIT) {
            /* Caller would set recovery_point = snd_nxt here */
            fr.recovery_point = 10000; /* example snd_nxt */
            printf("  -> ssthresh=%u, recovery_point=%u\n", fr.ssthresh, fr.recovery_point);
        }
    }

    /* Retransmitted segment arrives; receiver ACKs past recovery_point */
    printf("\n--- New ACK covering recovery_point ---\n");
    r = nfs_fr_on_ack(&fr, 10000);
    printf("ACK 10000: %s, cwnd=%u, in_recovery=%d\n", result_str(r), nfs_fr_cwnd(&fr),
           nfs_fr_in_recovery(&fr));

    printf("\nDone.\n");
    return 0;
}
