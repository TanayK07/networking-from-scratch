/* main.c — demonstrate TCP CUBIC congestion control. */

#include "tcp_cubic.h"
#include <stdio.h>

int main(void)
{
    struct nfs_cubic c;
    uint32_t mss = 1460;
    double t = 0.0;
    double rtt = 0.05;  /* 50 ms RTT */

    nfs_cubic_init(&c, mss);

    printf("=== TCP CUBIC Congestion Controller ===\n\n");

    /* --- Slow start phase --- */
    printf("--- Slow Start ---\n");
    int ack = 0;
    while (c.cwnd < c.ssthresh && ack < 50) {
        nfs_cubic_on_ack(&c, t);
        t += rtt;
        ack++;
        if (ack % 5 == 0)
            printf("  ACK %2d: cwnd = %6u  phase = %s\n",
                   ack, nfs_cubic_cwnd(&c), nfs_cubic_phase(&c));
    }
    printf("  Entered avoidance at cwnd = %u\n\n", nfs_cubic_cwnd(&c));

    /* --- Grow in avoidance for a while --- */
    printf("--- Congestion Avoidance (CUBIC growth) ---\n");
    for (int i = 0; i < 20; i++) {
        nfs_cubic_on_ack(&c, t);
        t += rtt;
        if (i % 5 == 0)
            printf("  t=%.2fs: cwnd = %6u  phase = %s\n",
                   t, nfs_cubic_cwnd(&c), nfs_cubic_phase(&c));
    }

    /* --- Loss event --- */
    printf("\n--- Loss Event ---\n");
    printf("  Before: cwnd = %u\n", nfs_cubic_cwnd(&c));
    nfs_cubic_on_loss(&c, t);
    printf("  After:  cwnd = %u  ssthresh = %u  Wmax = %.0f\n",
           nfs_cubic_cwnd(&c), c.ssthresh, c.wmax);

    /* --- CUBIC recovery curve --- */
    printf("\n--- CUBIC Recovery (converging to Wmax) ---\n");
    printf("  K = %.3f seconds\n", c.K);
    for (int i = 0; i < 30; i++) {
        nfs_cubic_on_ack(&c, t);
        t += rtt;
        if (i % 5 == 0)
            printf("  t=%.2fs: cwnd = %6u  (Wmax=%.0f)  phase = %s\n",
                   t, nfs_cubic_cwnd(&c), c.wmax, nfs_cubic_phase(&c));
    }

    /* --- Second loss --- */
    printf("\n--- Second Loss ---\n");
    nfs_cubic_on_loss(&c, t);
    printf("  cwnd = %u  ssthresh = %u  new Wmax = %.0f\n",
           nfs_cubic_cwnd(&c), c.ssthresh, c.wmax);

    return 0;
}
