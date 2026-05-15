/*
 * main.c -- ECN demo driver
 */
#include "ecn.h"
#include <stdio.h>

int main(void)
{
    printf("=== ECN (Explicit Congestion Notification) ===\n\n");

    /* IP ECN field */
    printf("--- IP ECN Field ---\n");
    uint8_t tos = 0x00;
    printf("TOS=0x%02X -> ECN: %s\n", tos, nfs_ecn_codepoint_name(nfs_ecn_get_ip(tos)));

    tos = nfs_ecn_set_ip(0xB8, NFS_ECN_ECT0); /* DSCP EF + ECT(0) */
    printf("TOS=0x%02X -> ECN: %s, ECT=%d, CE=%d\n",
           tos, nfs_ecn_codepoint_name(nfs_ecn_get_ip(tos)),
           nfs_ecn_is_ect(tos), nfs_ecn_is_ce(tos));

    int ce_tos = nfs_ecn_mark_ce(tos);
    printf("After CE mark: TOS=0x%02X -> ECN: %s\n",
           (uint8_t)ce_tos, nfs_ecn_codepoint_name(nfs_ecn_get_ip((uint8_t)ce_tos)));

    /* TCP ECN flags */
    printf("\n--- TCP ECN Negotiation ---\n");
    uint16_t syn_flags = nfs_ecn_build_syn_flags();
    printf("SYN flags:          0x%03X (ECE=%d, CWR=%d, SYN=%d)\n",
           syn_flags,
           (syn_flags & NFS_TCP_FLAG_ECE) ? 1 : 0,
           (syn_flags & NFS_TCP_FLAG_CWR) ? 1 : 0,
           (syn_flags & NFS_TCP_FLAG_SYN) ? 1 : 0);

    uint16_t synack_flags = nfs_ecn_build_synack_accept_flags();
    printf("SYN-ACK (accept):   0x%03X (ECE=%d, CWR=%d)\n",
           synack_flags,
           (synack_flags & NFS_TCP_FLAG_ECE) ? 1 : 0,
           (synack_flags & NFS_TCP_FLAG_CWR) ? 1 : 0);

    /* Negotiate */
    struct nfs_ecn_negotiation nego;
    nfs_ecn_nego_init(&nego);
    printf("\nNegotiation:\n");
    printf("  Initial state: %s\n", nfs_ecn_state_name(nego.state));

    nfs_ecn_nego_process(&nego, synack_flags, 1);
    printf("  After SYN-ACK:  %s (ecn_capable=%d)\n",
           nfs_ecn_state_name(nego.state), nego.ecn_capable);

    return 0;
}
