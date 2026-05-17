#ifndef NFS_STP_H
#define NFS_STP_H

#include <stddef.h>
#include <stdint.h>

/*
 * IEEE 802.1D Spanning Tree Protocol (STP) — BPDU handling and bridge election.
 *
 * STP prevents loops in bridged Ethernet networks by electing a root bridge
 * and placing redundant ports into a blocking state.
 */

/* ------------------------------------------------------------------ */
/*  Timer constants (IEEE 802.1D defaults)                             */
/* ------------------------------------------------------------------ */

#define NFS_STP_HELLO_TIME 2  /* seconds */
#define NFS_STP_MAX_AGE    20 /* seconds */
#define NFS_STP_FWD_DELAY  15 /* seconds */

/* ------------------------------------------------------------------ */
/*  Bridge ID                                                          */
/* ------------------------------------------------------------------ */

#define NFS_STP_DEFAULT_PRIORITY 32768

struct nfs_bridge_id {
    uint16_t priority; /* Default 32768 (0x8000), in increments of 4096 */
    uint8_t mac[6];    /* Bridge MAC address */
};

/*
 * Compare two bridge IDs.
 * Returns <0 if a < b (a is better/root), 0 if equal, >0 if a > b.
 * Priority is compared first, then MAC address.
 */
int nfs_bridge_id_cmp(const struct nfs_bridge_id *a, const struct nfs_bridge_id *b);

/*
 * Pack a bridge ID into 8 bytes: 2 bytes priority (network order) + 6 bytes MAC.
 */
void nfs_bridge_id_pack(const struct nfs_bridge_id *id, uint8_t out[8]);

/*
 * Unpack 8 bytes into a bridge ID.
 */
void nfs_bridge_id_unpack(const uint8_t data[8], struct nfs_bridge_id *id);

/* ------------------------------------------------------------------ */
/*  BPDU (Bridge Protocol Data Unit)                                   */
/* ------------------------------------------------------------------ */

#define NFS_BPDU_CONFIG_SIZE 35 /* Configuration BPDU is 35 bytes */
#define NFS_BPDU_TCN_SIZE    4  /* TCN BPDU is 4 bytes */

struct nfs_bpdu {
    uint16_t proto_id; /* Always 0x0000 for STP */
    uint8_t version;   /* 0 for STP, 2 for RSTP */
    uint8_t type;      /* 0x00 = Configuration BPDU, 0x80 = TCN BPDU */
    uint8_t flags;     /* bit 0: TC, bit 7: TCA */
    struct nfs_bridge_id root_id;
    uint32_t root_path_cost;
    struct nfs_bridge_id sender_id;
    uint16_t port_id;
    uint16_t message_age;   /* in 1/256ths of a second */
    uint16_t max_age;       /* default 20 sec = 20*256 = 5120 */
    uint16_t hello_time;    /* default 2 sec = 2*256 = 512 */
    uint16_t forward_delay; /* default 15 sec = 15*256 = 3840 */
};

/*
 * Build a BPDU into a byte buffer.
 * Returns bytes written (35 for config, 4 for TCN), or -1 on error.
 * All multi-byte fields are in network byte order.
 */
int nfs_bpdu_build(const struct nfs_bpdu *bpdu, uint8_t *out, size_t out_sz);

/*
 * Parse a BPDU from a byte buffer.
 * Returns 0 on success, -1 on error (bad proto_id, version, type, or too short).
 */
int nfs_bpdu_parse(const uint8_t *data, size_t len, struct nfs_bpdu *bpdu);

/*
 * Compare BPDU superiority. Returns 1 if a is superior (should replace b).
 * Compare in order: root_id, root_path_cost, sender_id, port_id.
 */
int nfs_bpdu_superior(const struct nfs_bpdu *a, const struct nfs_bpdu *b);

/* ------------------------------------------------------------------ */
/*  Port state machine                                                 */
/* ------------------------------------------------------------------ */

#define NFS_STP_DISABLED   0
#define NFS_STP_BLOCKING   1
#define NFS_STP_LISTENING  2
#define NFS_STP_LEARNING   3
#define NFS_STP_FORWARDING 4

#define NFS_STP_ROOT_PORT       0
#define NFS_STP_DESIGNATED_PORT 1
#define NFS_STP_ALTERNATE_PORT  2

struct nfs_stp_port {
    uint16_t port_id;
    int state;
    int role; /* NFS_STP_ROOT_PORT, NFS_STP_DESIGNATED_PORT, NFS_STP_ALTERNATE_PORT */
    uint32_t path_cost;
    struct nfs_bpdu best_bpdu; /* Best BPDU received on this port */
};

/*
 * Initialize a port. Starts in BLOCKING state with DESIGNATED role.
 */
void nfs_stp_port_init(struct nfs_stp_port *port, uint16_t port_id, uint32_t path_cost);

/*
 * Return recommended path cost per IEEE 802.1D-2004:
 *   10 Mbps   -> 100
 *   100 Mbps  -> 19
 *   1000 Mbps -> 4
 *   10000 Mbps -> 2
 * Returns 0 for unknown speeds.
 */
uint32_t nfs_stp_path_cost(int speed_mbps);

/* ------------------------------------------------------------------ */
/*  Root bridge election (simplified)                                  */
/* ------------------------------------------------------------------ */

#define NFS_STP_MAX_PORTS 16

struct nfs_stp_bridge {
    struct nfs_bridge_id id;
    struct nfs_bridge_id root_id;
    uint32_t root_path_cost;
    int root_port; /* -1 if this bridge is root */
    int num_ports;
    struct nfs_stp_port ports[NFS_STP_MAX_PORTS];
};

/*
 * Initialize a bridge. Assumes self is root.
 */
void nfs_stp_bridge_init(struct nfs_stp_bridge *br, uint16_t priority, const uint8_t mac[6]);

/*
 * Process a received BPDU on a port. Updates root/port states.
 */
void nfs_stp_process_bpdu(struct nfs_stp_bridge *br, int port_idx, const struct nfs_bpdu *bpdu);

#endif /* NFS_STP_H */
