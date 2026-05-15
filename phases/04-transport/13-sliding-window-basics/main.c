/* main.c — Sliding window demonstration.
 *
 * Shows the send window in action: sending bytes, ACKing, and
 * observing the window slide forward. */

#include "sliding_window.h"

#include <stdio.h>
#include <string.h>

static void print_window(const struct nfs_send_window *w)
{
    printf("  base=%u  next_seq=%u  in_flight=%u  available=%u  can_send=%d\n",
           w->base, w->next_seq,
           nfs_send_window_in_flight(w),
           nfs_send_window_available(w),
           nfs_send_window_can_send(w));
}

int main(void)
{
    printf("=== TCP Sliding Window Demo ===\n\n");

    struct nfs_send_window w;
    uint32_t isn = 1000;
    uint32_t win_size = 4;  /* small window for demo */

    nfs_send_window_init(&w, isn, win_size, 64);

    printf("Initial state (ISN=%u, window=%u):\n", isn, win_size);
    print_window(&w);

    /* Send some data */
    const char *msg = "HELLO";
    printf("\nSending '%s' (5 bytes, window=4):\n", msg);
    for (size_t i = 0; i < strlen(msg); i++) {
        int rc = nfs_send_window_send(&w, (uint8_t)msg[i]);
        if (rc == 0)
            printf("  Sent '%c' (seq=%lu)\n", msg[i], (unsigned long)(w.next_seq - 1));
        else
            printf("  Window full, cannot send '%c'\n", msg[i]);
        print_window(&w);
    }

    /* ACK first 2 bytes */
    printf("\nReceived cumulative ACK for seq %u:\n", isn + 2);
    int acked = nfs_send_window_ack(&w, isn + 2);
    printf("  Newly acked: %d bytes\n", acked);
    print_window(&w);

    /* Now we can send more */
    printf("\nSending remaining byte 'O':\n");
    int rc = nfs_send_window_send(&w, 'O');
    printf("  Send result: %d\n", rc);
    print_window(&w);

    /* Retransmission demo */
    printf("\nRetransmit data from seq %u:\n", w.base);
    size_t rlen;
    const uint8_t *rdata = nfs_send_window_get_data(&w, w.base, &rlen);
    if (rdata) {
        printf("  Got %zu bytes for retransmit: ", rlen);
        for (size_t i = 0; i < rlen && i < 10; i++)
            printf("'%c' ", rdata[i]);
        printf("\n");
    }

    /* ACK everything */
    printf("\nACK all remaining:\n");
    acked = nfs_send_window_ack(&w, w.next_seq);
    printf("  Newly acked: %d bytes\n", acked);
    print_window(&w);

    nfs_send_window_free(&w);
    printf("\nDone.\n");
    return 0;
}
