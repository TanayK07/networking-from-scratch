#ifndef NFS_BRIDGE_H
#define NFS_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define NFS_BRIDGE_MAX_PORTS 16
#define NFS_BRIDGE_MAC_LEN   6

#define NFS_FDB_MAX_ENTRIES   256
#define NFS_FDB_DEFAULT_AGING 300 /* 300 seconds (Linux default) */

/* Frame forwarding actions */
#define NFS_BRIDGE_FORWARD 0 /* unicast to known port */
#define NFS_BRIDGE_FLOOD   1 /* broadcast or unknown unicast -> all ports except ingress */
#define NFS_BRIDGE_DROP    2 /* e.g., src==dst port, or invalid frame */

/* ------------------------------------------------------------------ */
/*  Bridge port                                                        */
/* ------------------------------------------------------------------ */

struct nfs_bridge_port {
    int port_id;
    int active;
    char name[16];  /* e.g., "eth0", "veth1" */
    uint8_t mac[6]; /* port's own MAC */
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
};

/* ------------------------------------------------------------------ */
/*  Forwarding Database (FDB) entry                                    */
/* ------------------------------------------------------------------ */

struct nfs_fdb_entry {
    uint8_t mac[6];
    int port_id;
    uint32_t timestamp; /* when learned (seconds since start) */
    int is_static;      /* 0=learned, 1=static (never ages) */
    int valid;          /* 1=in use, 0=free slot */
};

/* ------------------------------------------------------------------ */
/*  Bridge structure                                                   */
/* ------------------------------------------------------------------ */

struct nfs_bridge {
    char name[16];
    int num_ports;
    struct nfs_bridge_port ports[NFS_BRIDGE_MAX_PORTS];
    struct nfs_fdb_entry fdb[NFS_FDB_MAX_ENTRIES];
    uint32_t aging_time; /* seconds */
    uint32_t clock;      /* simulated time */
    uint64_t forwarded;
    uint64_t flooded;
    uint64_t dropped;
    uint64_t learned;
};

/* ------------------------------------------------------------------ */
/*  Bridge operations                                                  */
/* ------------------------------------------------------------------ */

/* Initialize bridge with defaults. */
void nfs_bridge_init(struct nfs_bridge *br, const char *name);

/* Add a port. Returns port_id (>= 0) on success, -1 if full. */
int nfs_bridge_add_port(struct nfs_bridge *br, const char *port_name, const uint8_t mac[6]);

/* Remove a port. Returns 0 on success, -1 if not found. */
int nfs_bridge_remove_port(struct nfs_bridge *br, int port_id);

/* ------------------------------------------------------------------ */
/*  FDB operations                                                     */
/* ------------------------------------------------------------------ */

/* Learn a MAC on a port. Returns 0 on success, -1 if FDB full. */
int nfs_fdb_learn(struct nfs_bridge *br, const uint8_t mac[6], int port_id);

/* Lookup a MAC. Returns port_id, or -1 if not found. */
int nfs_fdb_lookup(const struct nfs_bridge *br, const uint8_t mac[6]);

/* Flush all dynamic entries. */
void nfs_fdb_flush(struct nfs_bridge *br);

/* Age out entries older than aging_time. */
void nfs_fdb_age(struct nfs_bridge *br, uint32_t now);

/* Add a static FDB entry. Returns 0 on success, -1 if FDB full. */
int nfs_fdb_add_static(struct nfs_bridge *br, const uint8_t mac[6], int port_id);

/* Count valid FDB entries. */
int nfs_fdb_count(const struct nfs_bridge *br);

/* ------------------------------------------------------------------ */
/*  Frame forwarding                                                   */
/* ------------------------------------------------------------------ */

/*
 * Forward a frame.  Returns action taken:
 *   NFS_BRIDGE_FORWARD — unicast to known port
 *   NFS_BRIDGE_FLOOD   — broadcast or unknown unicast
 *   NFS_BRIDGE_DROP    — invalid or hairpin
 */
int nfs_bridge_forward(struct nfs_bridge *br, int ingress_port, const uint8_t *frame, size_t len);

/*
 * Like nfs_bridge_forward but returns the destination port_id:
 *   >= 0  — specific port
 *   -1    — flood (all ports except ingress)
 *   -2    — drop
 */
int nfs_bridge_forward_port(struct nfs_bridge *br, int ingress_port, const uint8_t *frame,
                            size_t len);

#endif /* NFS_BRIDGE_H */
