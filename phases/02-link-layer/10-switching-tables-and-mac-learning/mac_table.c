#include "mac_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------- */

static int mac_eq(const uint8_t a[6], const uint8_t b[6])
{
    return memcmp(a, b, 6) == 0;
}

static int mac_is_broadcast(const uint8_t mac[6])
{
    static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return mac_eq(mac, bcast);
}

static int mac_is_multicast(const uint8_t mac[6])
{
    return (mac[0] & 0x01) != 0;
}

/* ---------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

void nfs_fdb_init(struct nfs_fdb *fdb, size_t capacity)
{
    fdb->entries  = calloc(capacity, sizeof(struct nfs_fdb_entry));
    fdb->count    = 0;
    fdb->capacity = capacity;
}

void nfs_fdb_free(struct nfs_fdb *fdb)
{
    free(fdb->entries);
    fdb->entries  = NULL;
    fdb->count    = 0;
    fdb->capacity = 0;
}

int nfs_fdb_learn(struct nfs_fdb *fdb, const uint8_t mac[6], int port)
{
    /* Never learn broadcast or multicast MACs */
    if (mac_is_broadcast(mac) || mac_is_multicast(mac))
        return 0;

    /* Check if this MAC already exists -- update in place */
    for (size_t i = 0; i < fdb->count; i++) {
        if (mac_eq(fdb->entries[i].mac, mac)) {
            fdb->entries[i].port      = port;
            fdb->entries[i].last_seen = time(NULL);
            return 0;
        }
    }

    /* New entry */
    if (fdb->count >= fdb->capacity)
        return -1;

    struct nfs_fdb_entry *e = &fdb->entries[fdb->count++];
    memcpy(e->mac, mac, 6);
    e->port      = port;
    e->last_seen = time(NULL);
    return 0;
}

int nfs_fdb_lookup(const struct nfs_fdb *fdb, const uint8_t mac[6])
{
    for (size_t i = 0; i < fdb->count; i++) {
        if (mac_eq(fdb->entries[i].mac, mac))
            return fdb->entries[i].port;
    }
    return -1;
}

int nfs_fdb_age(struct nfs_fdb *fdb, int max_age_sec)
{
    time_t now = time(NULL);
    int removed = 0;
    size_t i = 0;

    while (i < fdb->count) {
        if ((now - fdb->entries[i].last_seen) > max_age_sec) {
            /* Swap with last entry and shrink */
            fdb->entries[i] = fdb->entries[fdb->count - 1];
            fdb->count--;
            removed++;
        } else {
            i++;
        }
    }

    return removed;
}

int nfs_fdb_forward(const struct nfs_fdb *fdb, const uint8_t dst_mac[6],
                    int src_port, int *out_ports, int num_ports)
{
    /* Broadcast / multicast -> flood */
    if (mac_is_broadcast(dst_mac) || mac_is_multicast(dst_mac)) {
        int n = 0;
        for (int p = 0; p < num_ports; p++) {
            if (p != src_port)
                out_ports[n++] = p;
        }
        return n;
    }

    /* Known unicast */
    int dst_port = nfs_fdb_lookup(fdb, dst_mac);
    if (dst_port >= 0) {
        /* Don't send back to the port it came from */
        if (dst_port == src_port)
            return 0;
        out_ports[0] = dst_port;
        return 1;
    }

    /* Unknown unicast -> flood */
    int n = 0;
    for (int p = 0; p < num_ports; p++) {
        if (p != src_port)
            out_ports[n++] = p;
    }
    return n;
}

int nfs_fdb_format(const struct nfs_fdb *fdb, char *buf, size_t sz)
{
    int total = 0;
    int n;

    n = snprintf(buf + total, sz - (size_t)total,
                 "%-20s  %-6s  %s\n", "MAC Address", "Port", "Age (s)");
    if (n < 0) return -1;
    total += n;

    time_t now = time(NULL);

    for (size_t i = 0; i < fdb->count; i++) {
        const struct nfs_fdb_entry *e = &fdb->entries[i];
        long age = (long)(now - e->last_seen);
        n = snprintf(buf + total, sz - (size_t)total,
                     "%02x:%02x:%02x:%02x:%02x:%02x    %-6d  %ld\n",
                     e->mac[0], e->mac[1], e->mac[2],
                     e->mac[3], e->mac[4], e->mac[5],
                     e->port, age);
        if (n < 0) return -1;
        total += n;
    }

    return total;
}
