#include "recv_win.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Demo: receive window filling up, triggering zero-window probes,
 * and the application draining the buffer to reopen the window.
 * --------------------------------------------------------------- */

int main(void) {
    printf("=== Receive Window & Zero-Window Probes Demo ===\n\n");

    struct nfs_recv_window win;
    nfs_recv_window_init(&win, 32);

    printf("Initial advertised window: %u bytes\n", nfs_recv_window_advertised(&win));

    /* Receive data in chunks */
    const char *msg1 = "Hello, TCP world! "; /* 18 bytes */
    int accepted = nfs_recv_window_receive(&win, 0, (const uint8_t *)msg1, strlen(msg1));
    printf("Received %d bytes, advertised: %u, is_zero: %d\n", accepted,
           nfs_recv_window_advertised(&win), nfs_recv_window_is_zero(&win));

    /* Fill the rest */
    const char *msg2 = "More data padding!"; /* 18 bytes, only 14 fit */
    accepted = nfs_recv_window_receive(&win, 18, (const uint8_t *)msg2, strlen(msg2));
    printf("Received %d bytes, advertised: %u, is_zero: %d\n", accepted,
           nfs_recv_window_advertised(&win), nfs_recv_window_is_zero(&win));

    /* Window is zero now -- start ZWP */
    struct nfs_zwp zwp;
    nfs_zwp_init(&zwp, 1.0);

    if (nfs_recv_window_is_zero(&win)) {
        printf("\nWindow is zero! Starting zero-window probes.\n");
        nfs_zwp_start(&zwp, 0.0);

        /* Simulate probe checks */
        for (double t = 0.5; t <= 3.5; t += 0.5) {
            if (nfs_zwp_check(&zwp, t)) {
                printf("[t=%.1f] Sent ZWP #%d\n", t, zwp.probe_count);
            }
        }
    }

    /* Application reads some data */
    uint8_t buf[16];
    size_t nr = nfs_recv_window_read(&win, buf, sizeof(buf));
    printf("\nApp read %zu bytes, advertised: %u\n", nr, nfs_recv_window_advertised(&win));

    if (!nfs_recv_window_is_zero(&win)) {
        printf("Window reopened -- stopping ZWP.\n");
        nfs_zwp_stop(&zwp);
    }

    nfs_recv_window_free(&win);
    printf("\nDone.\n");
    return 0;
}
