#ifndef NFS_L2SWITCH_H
#define NFS_L2SWITCH_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define NFS_SW_MAX_PORTS 32
#define NFS_SW_MAX_VLANS 64
#define NFS_SW_MAC_TABLE 1024
#define NFS_SW_MAX_FRAME 1522 /* VLAN tagged max */
#define NFS_SW_MAC_LEN   6

/* Port modes */
#define NFS_SW_PORT_ACCESS 0 /* Untagged, single VLAN */
#define NFS_SW_PORT_TRUNK  1 /* Tagged, multiple VLANs */

/* Frame switching actions */
#define NFS_SW_FORWARD 0
#define NFS_SW_FLOOD   1
#define NFS_SW_DROP    2

/* ------------------------------------------------------------------ */
/*  Data structures                                                    */
/* ------------------------------------------------------------------ */

struct nfs_sw_port {
    int id;
    int active;
    int mode;               /* ACCESS or TRUNK */
    uint16_t pvid;          /* Port VLAN ID (native VLAN for access ports) */
    uint64_t allowed_vlans; /* Bitmask of allowed VLANs (for trunk, up to 64) */
    uint64_t rx_frames;
    uint64_t tx_frames;
    int mirror_dst; /* -1 = no mirror, else port to mirror TO */
};

struct nfs_sw_mac_entry {
    uint8_t mac[6];
    int port_id;
    uint16_t vlan_id;
    uint32_t last_seen; /* tick count */
    int valid;
};

struct nfs_sw_vlan {
    uint16_t vlan_id;
    int active;
    char name[32];
};

struct nfs_sw_result {
    int action;       /* FORWARD, FLOOD, DROP */
    int dst_port;     /* destination port (-1 for flood) */
    uint16_t vlan_id; /* effective VLAN for this frame */
};

struct nfs_l2switch {
    int num_ports;
    struct nfs_sw_port ports[NFS_SW_MAX_PORTS];
    struct nfs_sw_mac_entry mac_table[NFS_SW_MAC_TABLE];
    struct nfs_sw_vlan vlans[NFS_SW_MAX_VLANS];
    uint32_t mac_aging; /* default 300 seconds */
    uint32_t tick;      /* simulated clock */
    /* Stats */
    uint64_t total_rx;
    uint64_t total_tx;
    uint64_t total_flooded;
    uint64_t total_dropped;
    uint64_t total_forwarded;
};

/* ------------------------------------------------------------------ */
/*  Switch operations                                                  */
/* ------------------------------------------------------------------ */

/* Initialize switch with defaults (VLAN 1 active, aging=300). */
void nfs_sw_init(struct nfs_l2switch *sw);

/* Add a port. Returns port_id or -1 on failure. */
int nfs_sw_add_port(struct nfs_l2switch *sw, int mode, uint16_t pvid);

/* Set port mode and PVID. Returns 0 or -1. */
int nfs_sw_set_port_mode(struct nfs_l2switch *sw, int port_id, int mode, uint16_t pvid);

/* ------------------------------------------------------------------ */
/*  VLAN operations                                                    */
/* ------------------------------------------------------------------ */

/* Create a VLAN. Returns 0 or -1 if duplicate/full. */
int nfs_sw_vlan_create(struct nfs_l2switch *sw, uint16_t vlan_id, const char *name);

/* Delete a VLAN. Returns 0 or -1. */
int nfs_sw_vlan_delete(struct nfs_l2switch *sw, uint16_t vlan_id);

/* Allow a VLAN on a port (sets bit in allowed_vlans). Returns 0 or -1. */
int nfs_sw_port_allow_vlan(struct nfs_l2switch *sw, int port_id, uint16_t vlan_id);

/* ------------------------------------------------------------------ */
/*  MAC table operations                                               */
/* ------------------------------------------------------------------ */

/* Learn a MAC on a port+VLAN. Returns 0 or -1. */
int nfs_sw_mac_learn(struct nfs_l2switch *sw, const uint8_t mac[6], int port_id, uint16_t vlan_id);

/* Lookup a MAC in a given VLAN. Returns port_id or -1. */
int nfs_sw_mac_lookup(const struct nfs_l2switch *sw, const uint8_t mac[6], uint16_t vlan_id);

/* Flush all dynamic MAC entries. */
void nfs_sw_mac_flush(struct nfs_l2switch *sw);

/* Age out expired entries (last_seen + mac_aging <= tick). */
void nfs_sw_mac_age(struct nfs_l2switch *sw);

/* Count valid MAC entries. */
int nfs_sw_mac_count(const struct nfs_l2switch *sw);

/* ------------------------------------------------------------------ */
/*  Frame switching                                                    */
/* ------------------------------------------------------------------ */

/*
 * Process an incoming Ethernet frame on ingress_port.
 * Fills in result with action, dst_port, and VLAN.
 * Returns 0 on success, -1 on error (bad frame, bad port, etc.).
 */
int nfs_sw_process_frame(struct nfs_l2switch *sw, int ingress_port, const uint8_t *frame,
                         size_t len, struct nfs_sw_result *result);

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

/*
 * Build a test Ethernet frame.
 * Pads to 60-byte minimum. Returns frame length or -1.
 */
int nfs_sw_build_frame(const uint8_t dst[6], const uint8_t src[6], uint16_t ethertype,
                       const uint8_t *payload, size_t plen, uint8_t *out, size_t out_sz);

#endif /* NFS_L2SWITCH_H */
