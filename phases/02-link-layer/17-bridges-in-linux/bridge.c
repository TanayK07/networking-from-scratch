#include "bridge.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

static int mac_eq(const uint8_t a[6], const uint8_t b[6]) {
    return memcmp(a, b, 6) == 0;
}

static int is_broadcast(const uint8_t mac[6]) {
    static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return mac_eq(mac, bcast);
}

/* ------------------------------------------------------------------ */
/*  Bridge operations                                                  */
/* ------------------------------------------------------------------ */

void nfs_bridge_init(struct nfs_bridge *br, const char *name) {
    if (!br)
        return;
    memset(br, 0, sizeof(*br));
    if (name) {
        strncpy(br->name, name, sizeof(br->name) - 1);
        br->name[sizeof(br->name) - 1] = '\0';
    }
    br->aging_time = NFS_FDB_DEFAULT_AGING;
}

int nfs_bridge_add_port(struct nfs_bridge *br, const char *port_name, const uint8_t mac[6]) {
    if (!br || !port_name || !mac)
        return -1;
    if (br->num_ports >= NFS_BRIDGE_MAX_PORTS)
        return -1;

    int id = br->num_ports;
    struct nfs_bridge_port *p = &br->ports[id];
    memset(p, 0, sizeof(*p));
    p->port_id = id;
    p->active = 1;
    strncpy(p->name, port_name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';
    memcpy(p->mac, mac, 6);

    br->num_ports++;
    return id;
}

int nfs_bridge_remove_port(struct nfs_bridge *br, int port_id) {
    if (!br || port_id < 0 || port_id >= br->num_ports)
        return -1;
    if (!br->ports[port_id].active)
        return -1;

    br->ports[port_id].active = 0;

    /* Remove all FDB entries pointing to this port. */
    for (int i = 0; i < NFS_FDB_MAX_ENTRIES; i++) {
        if (br->fdb[i].valid && br->fdb[i].port_id == port_id)
            br->fdb[i].valid = 0;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  FDB operations                                                     */
/* ------------------------------------------------------------------ */

int nfs_fdb_learn(struct nfs_bridge *br, const uint8_t mac[6], int port_id) {
    if (!br || !mac)
        return -1;

    /* Check if MAC already known — update port and timestamp. */
    for (int i = 0; i < NFS_FDB_MAX_ENTRIES; i++) {
        if (br->fdb[i].valid && mac_eq(br->fdb[i].mac, mac)) {
            br->fdb[i].port_id = port_id;
            br->fdb[i].timestamp = br->clock;
            return 0;
        }
    }

    /* New entry — find a free slot. */
    for (int i = 0; i < NFS_FDB_MAX_ENTRIES; i++) {
        if (!br->fdb[i].valid) {
            memcpy(br->fdb[i].mac, mac, 6);
            br->fdb[i].port_id = port_id;
            br->fdb[i].timestamp = br->clock;
            br->fdb[i].is_static = 0;
            br->fdb[i].valid = 1;
            br->learned++;
            return 0;
        }
    }

    return -1; /* FDB full */
}

int nfs_fdb_lookup(const struct nfs_bridge *br, const uint8_t mac[6]) {
    if (!br || !mac)
        return -1;

    for (int i = 0; i < NFS_FDB_MAX_ENTRIES; i++) {
        if (br->fdb[i].valid && mac_eq(br->fdb[i].mac, mac))
            return br->fdb[i].port_id;
    }

    return -1;
}

void nfs_fdb_flush(struct nfs_bridge *br) {
    if (!br)
        return;

    for (int i = 0; i < NFS_FDB_MAX_ENTRIES; i++) {
        if (br->fdb[i].valid && !br->fdb[i].is_static)
            br->fdb[i].valid = 0;
    }
}

void nfs_fdb_age(struct nfs_bridge *br, uint32_t now) {
    if (!br)
        return;

    for (int i = 0; i < NFS_FDB_MAX_ENTRIES; i++) {
        if (br->fdb[i].valid && !br->fdb[i].is_static) {
            if (now - br->fdb[i].timestamp > br->aging_time)
                br->fdb[i].valid = 0;
        }
    }
}

int nfs_fdb_add_static(struct nfs_bridge *br, const uint8_t mac[6], int port_id) {
    if (!br || !mac)
        return -1;

    /* Check if already exists — update it. */
    for (int i = 0; i < NFS_FDB_MAX_ENTRIES; i++) {
        if (br->fdb[i].valid && mac_eq(br->fdb[i].mac, mac)) {
            br->fdb[i].port_id = port_id;
            br->fdb[i].is_static = 1;
            return 0;
        }
    }

    /* Find free slot. */
    for (int i = 0; i < NFS_FDB_MAX_ENTRIES; i++) {
        if (!br->fdb[i].valid) {
            memcpy(br->fdb[i].mac, mac, 6);
            br->fdb[i].port_id = port_id;
            br->fdb[i].timestamp = br->clock;
            br->fdb[i].is_static = 1;
            br->fdb[i].valid = 1;
            return 0;
        }
    }

    return -1; /* FDB full */
}

int nfs_fdb_count(const struct nfs_bridge *br) {
    if (!br)
        return 0;

    int count = 0;
    for (int i = 0; i < NFS_FDB_MAX_ENTRIES; i++) {
        if (br->fdb[i].valid)
            count++;
    }
    return count;
}

/* ------------------------------------------------------------------ */
/*  Frame forwarding                                                   */
/* ------------------------------------------------------------------ */

int nfs_bridge_forward(struct nfs_bridge *br, int ingress_port, const uint8_t *frame, size_t len) {
    if (!br || !frame || len < 14) {
        if (br)
            br->dropped++;
        return NFS_BRIDGE_DROP;
    }

    const uint8_t *dst_mac = frame;
    const uint8_t *src_mac = frame + 6;

    /* Source learning */
    nfs_fdb_learn(br, src_mac, ingress_port);

    /* Update ingress port stats */
    if (ingress_port >= 0 && ingress_port < br->num_ports) {
        br->ports[ingress_port].rx_frames++;
        br->ports[ingress_port].rx_bytes += len;
    }

    /* Broadcast → flood */
    if (is_broadcast(dst_mac)) {
        br->flooded++;
        return NFS_BRIDGE_FLOOD;
    }

    /* Lookup destination */
    int dst_port = nfs_fdb_lookup(br, dst_mac);

    if (dst_port < 0) {
        /* Unknown unicast → flood */
        br->flooded++;
        return NFS_BRIDGE_FLOOD;
    }

    if (dst_port == ingress_port) {
        /* Hairpin — drop */
        br->dropped++;
        return NFS_BRIDGE_DROP;
    }

    /* Known unicast → forward */
    if (dst_port >= 0 && dst_port < br->num_ports) {
        br->ports[dst_port].tx_frames++;
        br->ports[dst_port].tx_bytes += len;
    }
    br->forwarded++;
    return NFS_BRIDGE_FORWARD;
}

int nfs_bridge_forward_port(struct nfs_bridge *br, int ingress_port, const uint8_t *frame,
                            size_t len) {
    if (!br || !frame || len < 14)
        return -2;

    const uint8_t *dst_mac = frame;
    const uint8_t *src_mac = frame + 6;

    /* Source learning */
    nfs_fdb_learn(br, src_mac, ingress_port);

    /* Broadcast → flood */
    if (is_broadcast(dst_mac))
        return -1;

    /* Lookup destination */
    int dst_port = nfs_fdb_lookup(br, dst_mac);

    if (dst_port < 0)
        return -1; /* unknown unicast → flood */

    if (dst_port == ingress_port)
        return -2; /* hairpin → drop */

    return dst_port;
}
