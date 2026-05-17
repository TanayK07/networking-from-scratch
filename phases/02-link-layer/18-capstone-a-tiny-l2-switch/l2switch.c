/*
 * l2switch.c — Complete L2 switch simulator with MAC learning,
 *              per-VLAN forwarding, and port mirroring support.
 */

#include "l2switch.h"

#include <arpa/inet.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static int mac_equal(const uint8_t a[6], const uint8_t b[6]) {
    return memcmp(a, b, 6) == 0;
}

static int is_broadcast(const uint8_t mac[6]) {
    static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return mac_equal(mac, bcast);
}

static int is_multicast(const uint8_t mac[6]) {
    return (mac[0] & 0x01) != 0;
}

static int vlan_in_bitmask(uint64_t mask, uint16_t vlan_id) {
    if (vlan_id >= 64)
        return 0;
    return (mask & ((uint64_t)1 << vlan_id)) != 0;
}

/* ------------------------------------------------------------------ */
/*  Switch operations                                                  */
/* ------------------------------------------------------------------ */

void nfs_sw_init(struct nfs_l2switch *sw) {
    memset(sw, 0, sizeof(*sw));
    sw->mac_aging = 300;
    sw->num_ports = 0;
    sw->tick = 0;

    /* Initialize all ports as inactive with mirror_dst = -1 */
    for (int i = 0; i < NFS_SW_MAX_PORTS; i++) {
        sw->ports[i].mirror_dst = -1;
    }

    /* VLAN 1 is active by default */
    sw->vlans[0].vlan_id = 1;
    sw->vlans[0].active = 1;
    strncpy(sw->vlans[0].name, "default", sizeof(sw->vlans[0].name) - 1);
}

int nfs_sw_add_port(struct nfs_l2switch *sw, int mode, uint16_t pvid) {
    if (!sw || sw->num_ports >= NFS_SW_MAX_PORTS)
        return -1;

    int id = sw->num_ports;
    struct nfs_sw_port *p = &sw->ports[id];
    p->id = id;
    p->active = 1;
    p->mode = mode;
    p->pvid = pvid;
    p->rx_frames = 0;
    p->tx_frames = 0;
    p->mirror_dst = -1;

    /* Set allowed VLANs: for access, just the PVID; for trunk, PVID by default */
    p->allowed_vlans = 0;
    if (pvid < 64) {
        p->allowed_vlans = (uint64_t)1 << pvid;
    }

    sw->num_ports++;
    return id;
}

int nfs_sw_set_port_mode(struct nfs_l2switch *sw, int port_id, int mode, uint16_t pvid) {
    if (!sw || port_id < 0 || port_id >= sw->num_ports)
        return -1;

    struct nfs_sw_port *p = &sw->ports[port_id];
    p->mode = mode;
    p->pvid = pvid;

    /* Reset allowed VLANs to just the PVID */
    p->allowed_vlans = 0;
    if (pvid < 64) {
        p->allowed_vlans = (uint64_t)1 << pvid;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  VLAN operations                                                    */
/* ------------------------------------------------------------------ */

int nfs_sw_vlan_create(struct nfs_l2switch *sw, uint16_t vlan_id, const char *name) {
    if (!sw || vlan_id == 0)
        return -1;

    /* Check for duplicate */
    for (int i = 0; i < NFS_SW_MAX_VLANS; i++) {
        if (sw->vlans[i].active && sw->vlans[i].vlan_id == vlan_id)
            return -1;
    }

    /* Find free slot */
    for (int i = 0; i < NFS_SW_MAX_VLANS; i++) {
        if (!sw->vlans[i].active) {
            sw->vlans[i].vlan_id = vlan_id;
            sw->vlans[i].active = 1;
            memset(sw->vlans[i].name, 0, sizeof(sw->vlans[i].name));
            if (name) {
                strncpy(sw->vlans[i].name, name, sizeof(sw->vlans[i].name) - 1);
            }
            return 0;
        }
    }
    return -1; /* table full */
}

int nfs_sw_vlan_delete(struct nfs_l2switch *sw, uint16_t vlan_id) {
    if (!sw)
        return -1;

    for (int i = 0; i < NFS_SW_MAX_VLANS; i++) {
        if (sw->vlans[i].active && sw->vlans[i].vlan_id == vlan_id) {
            sw->vlans[i].active = 0;
            sw->vlans[i].vlan_id = 0;
            memset(sw->vlans[i].name, 0, sizeof(sw->vlans[i].name));
            return 0;
        }
    }
    return -1; /* not found */
}

int nfs_sw_port_allow_vlan(struct nfs_l2switch *sw, int port_id, uint16_t vlan_id) {
    if (!sw || port_id < 0 || port_id >= sw->num_ports)
        return -1;
    if (vlan_id >= 64)
        return -1;

    sw->ports[port_id].allowed_vlans |= (uint64_t)1 << vlan_id;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  MAC table operations                                               */
/* ------------------------------------------------------------------ */

int nfs_sw_mac_learn(struct nfs_l2switch *sw, const uint8_t mac[6], int port_id, uint16_t vlan_id) {
    if (!sw || !mac)
        return -1;

    /* Check if entry already exists (update it: station move) */
    for (int i = 0; i < NFS_SW_MAC_TABLE; i++) {
        if (sw->mac_table[i].valid && mac_equal(sw->mac_table[i].mac, mac) &&
            sw->mac_table[i].vlan_id == vlan_id) {
            sw->mac_table[i].port_id = port_id;
            sw->mac_table[i].last_seen = sw->tick;
            return 0;
        }
    }

    /* Find free slot */
    for (int i = 0; i < NFS_SW_MAC_TABLE; i++) {
        if (!sw->mac_table[i].valid) {
            memcpy(sw->mac_table[i].mac, mac, 6);
            sw->mac_table[i].port_id = port_id;
            sw->mac_table[i].vlan_id = vlan_id;
            sw->mac_table[i].last_seen = sw->tick;
            sw->mac_table[i].valid = 1;
            return 0;
        }
    }
    return -1; /* table full */
}

int nfs_sw_mac_lookup(const struct nfs_l2switch *sw, const uint8_t mac[6], uint16_t vlan_id) {
    if (!sw || !mac)
        return -1;

    for (int i = 0; i < NFS_SW_MAC_TABLE; i++) {
        if (sw->mac_table[i].valid && mac_equal(sw->mac_table[i].mac, mac) &&
            sw->mac_table[i].vlan_id == vlan_id) {
            return sw->mac_table[i].port_id;
        }
    }
    return -1;
}

void nfs_sw_mac_flush(struct nfs_l2switch *sw) {
    if (!sw)
        return;
    for (int i = 0; i < NFS_SW_MAC_TABLE; i++) {
        sw->mac_table[i].valid = 0;
    }
}

void nfs_sw_mac_age(struct nfs_l2switch *sw) {
    if (!sw)
        return;
    for (int i = 0; i < NFS_SW_MAC_TABLE; i++) {
        if (sw->mac_table[i].valid) {
            if (sw->tick - sw->mac_table[i].last_seen >= sw->mac_aging) {
                sw->mac_table[i].valid = 0;
            }
        }
    }
}

int nfs_sw_mac_count(const struct nfs_l2switch *sw) {
    if (!sw)
        return 0;
    int count = 0;
    for (int i = 0; i < NFS_SW_MAC_TABLE; i++) {
        if (sw->mac_table[i].valid)
            count++;
    }
    return count;
}

/* ------------------------------------------------------------------ */
/*  Frame switching                                                    */
/* ------------------------------------------------------------------ */

int nfs_sw_process_frame(struct nfs_l2switch *sw, int ingress_port, const uint8_t *frame,
                         size_t len, struct nfs_sw_result *result) {
    if (!sw || !frame || !result)
        return -1;

    /* Minimum Ethernet frame: 14 bytes header */
    if (len < 14)
        return -1;

    /* Validate ingress port */
    if (ingress_port < 0 || ingress_port >= sw->num_ports)
        return -1;
    if (!sw->ports[ingress_port].active)
        return -1;

    struct nfs_sw_port *in_port = &sw->ports[ingress_port];

    /* Extract src and dst MAC from frame */
    const uint8_t *dst_mac = frame;
    const uint8_t *src_mac = frame + 6;

    /* Step 1: Determine VLAN */
    uint16_t vlan_id;
    uint16_t ethertype = ((uint16_t)frame[12] << 8) | frame[13];

    if (in_port->mode == NFS_SW_PORT_TRUNK && ethertype == 0x8100) {
        /* 802.1Q tagged frame — extract VLAN from tag */
        if (len < 18)
            return -1;
        vlan_id = (((uint16_t)frame[14] << 8) | frame[15]) & 0x0FFF;
    } else {
        /* Access port or untagged frame on trunk: use port PVID */
        vlan_id = in_port->pvid;
    }

    /* Step 2: Check VLAN membership on ingress port */
    if (!vlan_in_bitmask(in_port->allowed_vlans, vlan_id)) {
        result->action = NFS_SW_DROP;
        result->dst_port = -1;
        result->vlan_id = vlan_id;
        sw->total_rx++;
        sw->total_dropped++;
        in_port->rx_frames++;
        return 0;
    }

    /* Update stats */
    sw->total_rx++;
    in_port->rx_frames++;

    /* Step 3: Learn src MAC (per-VLAN) — skip multicast/broadcast src */
    if (!is_multicast(src_mac)) {
        nfs_sw_mac_learn(sw, src_mac, ingress_port, vlan_id);
    }

    /* Step 4: Lookup dst MAC */
    if (is_broadcast(dst_mac) || is_multicast(dst_mac)) {
        /* Broadcast/multicast: flood */
        result->action = NFS_SW_FLOOD;
        result->dst_port = -1;
        result->vlan_id = vlan_id;
        sw->total_flooded++;
        return 0;
    }

    int dst_port_id = nfs_sw_mac_lookup(sw, dst_mac, vlan_id);

    if (dst_port_id < 0) {
        /* Unknown unicast: flood */
        result->action = NFS_SW_FLOOD;
        result->dst_port = -1;
        result->vlan_id = vlan_id;
        sw->total_flooded++;
        return 0;
    }

    /* Step 5: Forward/Drop */
    if (dst_port_id == ingress_port) {
        /* Same port — drop (hairpin prevention) */
        result->action = NFS_SW_DROP;
        result->dst_port = -1;
        result->vlan_id = vlan_id;
        sw->total_dropped++;
        return 0;
    }

    /* Known unicast: forward */
    result->action = NFS_SW_FORWARD;
    result->dst_port = dst_port_id;
    result->vlan_id = vlan_id;
    sw->total_forwarded++;
    sw->total_tx++;
    sw->ports[dst_port_id].tx_frames++;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

int nfs_sw_build_frame(const uint8_t dst[6], const uint8_t src[6], uint16_t ethertype,
                       const uint8_t *payload, size_t plen, uint8_t *out, size_t out_sz) {
    if (!dst || !src || !out)
        return -1;

    /* Minimum payload for 60-byte frame: 46 bytes */
    size_t payload_len = plen;
    if (payload_len < 46)
        payload_len = 46;

    size_t total = 14 + payload_len;
    if (total > out_sz)
        return -1;

    /* Zero the output buffer for padding */
    memset(out, 0, total);

    /* Dst MAC */
    memcpy(out, dst, 6);
    /* Src MAC */
    memcpy(out + 6, src, 6);
    /* EtherType in network byte order */
    out[12] = (uint8_t)(ethertype >> 8);
    out[13] = (uint8_t)(ethertype & 0xFF);

    /* Payload */
    if (payload && plen > 0) {
        memcpy(out + 14, payload, plen);
    }

    return (int)total;
}
