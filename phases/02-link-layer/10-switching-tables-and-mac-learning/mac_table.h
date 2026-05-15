#ifndef NFS_MAC_TABLE_H
#define NFS_MAC_TABLE_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* ---------------------------------------------------------------
 * MAC address forwarding database (FDB).
 *
 * A layer-2 switch learns source MAC addresses from incoming frames
 * and records which port they arrived on.  When forwarding, it looks
 * up the destination MAC:
 *   - known unicast  -> send to the single learned port
 *   - unknown / bcast -> flood to all ports except the source port
 *
 * Entries age out after a configurable timeout (typically 300 s).
 * --------------------------------------------------------------- */

struct nfs_fdb_entry {
    uint8_t mac[6];
    int     port;
    time_t  last_seen;
};

struct nfs_fdb {
    struct nfs_fdb_entry *entries;
    size_t count;
    size_t capacity;
};

/* Initialise an FDB that can hold up to `capacity` entries. */
void nfs_fdb_init(struct nfs_fdb *fdb, size_t capacity);

/* Free all resources owned by the FDB. */
void nfs_fdb_free(struct nfs_fdb *fdb);

/* Learn (or update) a MAC -> port mapping.
 * Returns 0 on success, -1 if the table is full and the MAC is new. */
int nfs_fdb_learn(struct nfs_fdb *fdb, const uint8_t mac[6], int port);

/* Look up which port a MAC was last seen on.
 * Returns the port number, or -1 if the MAC is unknown. */
int nfs_fdb_lookup(const struct nfs_fdb *fdb, const uint8_t mac[6]);

/* Remove entries whose last_seen is older than `max_age_sec` seconds
 * relative to the current time.  Returns the number of entries removed. */
int nfs_fdb_age(struct nfs_fdb *fdb, int max_age_sec);

/* Decide where to forward a frame addressed to `dst_mac` that arrived
 * on `src_port`.  Writes the list of output ports to `out_ports`
 * (at most `num_ports` entries).
 *   - Known unicast: single port (unless it equals src_port).
 *   - Unknown / broadcast: flood all ports 0..num_ports-1 except src_port.
 * Returns the number of ports written to out_ports. */
int nfs_fdb_forward(const struct nfs_fdb *fdb, const uint8_t dst_mac[6],
                    int src_port, int *out_ports, int num_ports);

/* Format the FDB contents into a human-readable string.
 * Returns the number of characters written (excluding NUL). */
int nfs_fdb_format(const struct nfs_fdb *fdb, char *buf, size_t sz);

#endif /* NFS_MAC_TABLE_H */
