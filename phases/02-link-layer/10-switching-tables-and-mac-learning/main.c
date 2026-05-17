#include "mac_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FDB   256
#define NUM_PORTS 8

static int parse_mac(const char *str, uint8_t mac[6]) {
    unsigned int m[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6)
        return -1;
    for (int i = 0; i < 6; i++)
        mac[i] = (uint8_t)m[i];
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s learn  <mac> <port>   -- learn MAC on port\n", prog);
    fprintf(stderr, "  %s lookup <mac>          -- look up MAC\n", prog);
    fprintf(stderr, "  %s demo                  -- run a demo scenario\n", prog);
}

static void demo(void) {
    struct nfs_fdb fdb;
    nfs_fdb_init(&fdb, MAX_FDB);

    printf("=== MAC Learning Demo ===\n\n");

    /* Simulate some frames arriving on different ports */
    uint8_t mac_a[] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01};
    uint8_t mac_b[] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x02};
    uint8_t mac_c[] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x03};
    uint8_t mac_unknown[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};

    nfs_fdb_learn(&fdb, mac_a, 1);
    nfs_fdb_learn(&fdb, mac_b, 2);
    nfs_fdb_learn(&fdb, mac_c, 3);

    printf("Learned 3 MACs:\n");
    char buf[4096];
    nfs_fdb_format(&fdb, buf, sizeof(buf));
    printf("%s\n", buf);

    /* Forward to known destination */
    int out[NUM_PORTS];
    int n = nfs_fdb_forward(&fdb, mac_b, 1, out, NUM_PORTS);
    printf("Forward to aa:bb:cc:00:00:02 from port 1 -> %d port(s):", n);
    for (int i = 0; i < n; i++)
        printf(" %d", out[i]);
    printf("\n");

    /* Forward to unknown destination */
    n = nfs_fdb_forward(&fdb, mac_unknown, 1, out, NUM_PORTS);
    printf("Forward to unknown de:ad:be:ef:00:01 from port 1 -> %d port(s):", n);
    for (int i = 0; i < n; i++)
        printf(" %d", out[i]);
    printf("\n");

    nfs_fdb_free(&fdb);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "demo") == 0) {
        demo();
        return 0;
    }

    if (strcmp(argv[1], "learn") == 0 && argc == 4) {
        uint8_t mac[6];
        if (parse_mac(argv[2], mac) < 0) {
            fprintf(stderr, "Invalid MAC: %s\n", argv[2]);
            return 1;
        }
        int port = atoi(argv[3]);
        struct nfs_fdb fdb;
        nfs_fdb_init(&fdb, MAX_FDB);
        int rc = nfs_fdb_learn(&fdb, mac, port);
        if (rc == 0)
            printf("Learned %s on port %d\n", argv[2], port);
        else
            printf("Table full\n");
        nfs_fdb_free(&fdb);
        return 0;
    }

    if (strcmp(argv[1], "lookup") == 0 && argc == 3) {
        uint8_t mac[6];
        if (parse_mac(argv[2], mac) < 0) {
            fprintf(stderr, "Invalid MAC: %s\n", argv[2]);
            return 1;
        }
        struct nfs_fdb fdb;
        nfs_fdb_init(&fdb, MAX_FDB);
        int port = nfs_fdb_lookup(&fdb, mac);
        if (port >= 0)
            printf("Port %d\n", port);
        else
            printf("Unknown\n");
        nfs_fdb_free(&fdb);
        return 0;
    }

    usage(argv[0]);
    return 1;
}
