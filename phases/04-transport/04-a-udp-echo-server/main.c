/*
 * main.c -- Demo driver for UDP Echo Server
 */

#include "echo.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct nfs_echo_server srv;
    if (nfs_echo_init(&srv, 0) != 0) {
        fprintf(stderr, "Failed to initialize echo server\n");
        return 1;
    }

    /* Simulate echoing several datagrams */
    const char *messages[] = {
        "Hello, World!",
        "UDP Echo (RFC 862)",
        "Networking from scratch",
        "",  /* empty datagram */
    };

    uint8_t response[1500];
    size_t  resp_len;

    for (int i = 0; i < 4; i++) {
        const uint8_t *in = (const uint8_t *)messages[i];
        size_t in_len = strlen(messages[i]);

        int rc = nfs_echo_process(&srv, in, in_len,
                                  response, sizeof(response), &resp_len);
        if (rc == 0) {
            printf("Echo %d: in=%zu bytes, out=%zu bytes", i + 1, in_len, resp_len);
            if (resp_len > 0)
                printf(" \"%.*s\"", (int)resp_len, response);
            printf("\n");
        } else {
            printf("Echo %d: ERROR\n", i + 1);
        }
    }

    /* Print stats */
    struct nfs_echo_stats stats;
    nfs_echo_stats(&srv, &stats);
    printf("\nStats: packets=%lu bytes=%lu errors=%lu\n",
           (unsigned long)stats.packets_processed,
           (unsigned long)stats.bytes_echoed,
           (unsigned long)stats.errors);

    nfs_echo_shutdown(&srv);
    return 0;
}
