/* main.c — TCP connection teardown demonstration.
 *
 * Shows the three teardown scenarios:
 *   1. Active close  (client initiates)
 *   2. Passive close (server side)
 *   3. Simultaneous close (both sides) */

#include "teardown.h"

#include <stdio.h>

static void print_state(const char *label, const struct nfs_tcp_conn *c)
{
    printf("  %-20s -> %s  (local_seq=%u, remote_seq=%u)\n",
           label, nfs_conn_state_name(c->state),
           c->local_seq, c->remote_seq);
}

int main(void)
{
    printf("=== TCP Connection Teardown Demo ===\n\n");

    /* Scenario 1: Active close */
    printf("--- Active Close (client initiates) ---\n");
    {
        struct nfs_tcp_conn c;
        nfs_conn_init(&c, 1000, 2000, 30.0);
        print_state("init", &c);

        nfs_conn_close(&c);               /* send FIN */
        print_state("close()", &c);

        nfs_conn_recv_ack(&c);            /* FIN ACKed */
        print_state("recv ACK", &c);

        nfs_conn_recv_fin(&c);            /* peer's FIN */
        c.time_wait_start = 100.0;
        print_state("recv FIN", &c);

        /* TIME_WAIT not expired */
        int expired = nfs_conn_time_wait_expired(&c, 130.0);
        printf("  TIME_WAIT at t=130: expired=%d (need 2*MSL=60s)\n", expired);

        /* TIME_WAIT expired */
        expired = nfs_conn_time_wait_expired(&c, 161.0);
        printf("  TIME_WAIT at t=161: expired=%d\n", expired);
        print_state("after 2*MSL", &c);
    }

    /* Scenario 2: Passive close */
    printf("\n--- Passive Close (server side) ---\n");
    {
        struct nfs_tcp_conn c;
        nfs_conn_init(&c, 5000, 6000, 30.0);
        print_state("init", &c);

        nfs_conn_recv_fin(&c);            /* client sent FIN */
        print_state("recv FIN", &c);

        nfs_conn_close(&c);               /* application closes */
        print_state("close()", &c);

        nfs_conn_recv_ack(&c);            /* our FIN ACKed */
        print_state("recv ACK", &c);
    }

    /* Scenario 3: Simultaneous close */
    printf("\n--- Simultaneous Close ---\n");
    {
        struct nfs_tcp_conn c;
        nfs_conn_init(&c, 8000, 9000, 60.0);
        print_state("init", &c);

        nfs_conn_close(&c);               /* we send FIN */
        print_state("close()", &c);

        nfs_conn_recv_fin(&c);            /* peer also sent FIN */
        print_state("recv FIN", &c);

        nfs_conn_recv_ack(&c);            /* our FIN ACKed */
        c.time_wait_start = 0.0;
        print_state("recv ACK", &c);

        nfs_conn_time_wait_expired(&c, 121.0);
        print_state("after 2*MSL", &c);
    }

    printf("\nDone.\n");
    return 0;
}
