#include "tcb.h"
#include <stdio.h>

/* Demonstrate TCB lifecycle: init, handshake sequence numbers,
 * send data, receive ACKs, and check 4-tuple matching. */

int main(void) {
    struct nfs_tcb tcb;
    char buf[256];

    /* Local 10.0.0.1:12345 -> Remote 10.0.0.2:80 */
    uint32_t local = (10u << 24) | 1;  /* 10.0.0.1 */
    uint32_t remote = (10u << 24) | 2; /* 10.0.0.2 */

    nfs_tcb_init(&tcb, 12345, 80, local, remote);
    printf("=== TCB Demo ===\n\n");

    nfs_tcb_format(&tcb, buf, sizeof(buf));
    printf("After init:\n  %s\n\n", buf);

    /* Simulate SYN sent: pick ISS */
    nfs_tcb_set_iss(&tcb, 1000);
    tcb.state = NFS_TCB_SYN_SENT;
    nfs_tcb_format(&tcb, buf, sizeof(buf));
    printf("After SYN (ISS=1000):\n  %s\n\n", buf);

    /* Simulate SYN-ACK received */
    nfs_tcb_set_irs(&tcb, 5000);
    nfs_tcb_ack_received(&tcb, 1001); /* ACK our SYN */
    tcb.state = NFS_TCB_ESTABLISHED;
    nfs_tcb_format(&tcb, buf, sizeof(buf));
    printf("After SYN-ACK (IRS=5000, ACK=1001):\n  %s\n\n", buf);

    /* Send 100 bytes */
    printf("Send window available: %d bytes\n", nfs_tcb_snd_available(&tcb));
    nfs_tcb_advance_snd(&tcb, 100);
    printf("After sending 100 bytes, available: %d bytes\n", nfs_tcb_snd_available(&tcb));

    /* Receive ACK for the 100 bytes */
    nfs_tcb_ack_received(&tcb, 1101);
    printf("After ACK 1101, available: %d bytes\n\n", nfs_tcb_snd_available(&tcb));

    /* Receive 50 bytes of data */
    nfs_tcb_data_received(&tcb, 50);
    nfs_tcb_format(&tcb, buf, sizeof(buf));
    printf("After receiving 50 bytes:\n  %s\n\n", buf);

    /* 4-tuple matching */
    printf("Packet from 10.0.0.2:80 -> 10.0.0.1:12345 matches? %s\n",
           nfs_tcb_matches(&tcb, remote, 80, local, 12345) ? "yes" : "no");
    printf("Packet from 10.0.0.3:80 -> 10.0.0.1:12345 matches? %s\n",
           nfs_tcb_matches(&tcb, (10u << 24) | 3, 80, local, 12345) ? "yes" : "no");

    return 0;
}
