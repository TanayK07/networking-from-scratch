#include "stp.h"

#include <arpa/inet.h> /* htons, ntohs, htonl, ntohl */
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Bridge ID                                                          */
/* ------------------------------------------------------------------ */

int nfs_bridge_id_cmp(const struct nfs_bridge_id *a, const struct nfs_bridge_id *b) {
    if (a->priority != b->priority)
        return (a->priority < b->priority) ? -1 : 1;
    return memcmp(a->mac, b->mac, 6);
}

void nfs_bridge_id_pack(const struct nfs_bridge_id *id, uint8_t out[8]) {
    uint16_t p = htons(id->priority);
    memcpy(out, &p, 2);
    memcpy(out + 2, id->mac, 6);
}

void nfs_bridge_id_unpack(const uint8_t data[8], struct nfs_bridge_id *id) {
    uint16_t p;
    memcpy(&p, data, 2);
    id->priority = ntohs(p);
    memcpy(id->mac, data + 2, 6);
}

/* ------------------------------------------------------------------ */
/*  BPDU build / parse                                                 */
/* ------------------------------------------------------------------ */

int nfs_bpdu_build(const struct nfs_bpdu *bpdu, uint8_t *out, size_t out_sz) {
    if (!bpdu || !out)
        return -1;

    /* TCN BPDU: only 4 bytes */
    if (bpdu->type == 0x80) {
        if (out_sz < NFS_BPDU_TCN_SIZE)
            return -1;
        uint16_t proto = htons(bpdu->proto_id);
        memcpy(out, &proto, 2);
        out[2] = bpdu->version;
        out[3] = bpdu->type;
        return NFS_BPDU_TCN_SIZE;
    }

    /* Configuration BPDU: 35 bytes */
    if (bpdu->type != 0x00)
        return -1;
    if (out_sz < NFS_BPDU_CONFIG_SIZE)
        return -1;

    size_t off = 0;

    /* proto_id (2 bytes) */
    uint16_t proto = htons(bpdu->proto_id);
    memcpy(out + off, &proto, 2);
    off += 2;

    /* version (1 byte) */
    out[off++] = bpdu->version;

    /* type (1 byte) */
    out[off++] = bpdu->type;

    /* flags (1 byte) */
    out[off++] = bpdu->flags;

    /* root_id (8 bytes) */
    nfs_bridge_id_pack(&bpdu->root_id, out + off);
    off += 8;

    /* root_path_cost (4 bytes) */
    uint32_t cost = htonl(bpdu->root_path_cost);
    memcpy(out + off, &cost, 4);
    off += 4;

    /* sender_id (8 bytes) */
    nfs_bridge_id_pack(&bpdu->sender_id, out + off);
    off += 8;

    /* port_id (2 bytes) */
    uint16_t pid = htons(bpdu->port_id);
    memcpy(out + off, &pid, 2);
    off += 2;

    /* message_age (2 bytes) */
    uint16_t msg_age = htons(bpdu->message_age);
    memcpy(out + off, &msg_age, 2);
    off += 2;

    /* max_age (2 bytes) */
    uint16_t max_a = htons(bpdu->max_age);
    memcpy(out + off, &max_a, 2);
    off += 2;

    /* hello_time (2 bytes) */
    uint16_t hello = htons(bpdu->hello_time);
    memcpy(out + off, &hello, 2);
    off += 2;

    /* forward_delay (2 bytes) */
    uint16_t fwd = htons(bpdu->forward_delay);
    memcpy(out + off, &fwd, 2);
    off += 2;

    return (int)off; /* should be 35 */
}

int nfs_bpdu_parse(const uint8_t *data, size_t len, struct nfs_bpdu *bpdu) {
    if (!data || !bpdu)
        return -1;
    if (len < 4)
        return -1;

    /* proto_id */
    uint16_t proto;
    memcpy(&proto, data, 2);
    bpdu->proto_id = ntohs(proto);
    if (bpdu->proto_id != 0x0000)
        return -1;

    /* version */
    bpdu->version = data[2];
    if (bpdu->version != 0 && bpdu->version != 2)
        return -1;

    /* type */
    bpdu->type = data[3];
    if (bpdu->type != 0x00 && bpdu->type != 0x80)
        return -1;

    /* TCN BPDU: done after 4 bytes */
    if (bpdu->type == 0x80) {
        /* Zero out remaining fields */
        bpdu->flags = 0;
        memset(&bpdu->root_id, 0, sizeof(bpdu->root_id));
        bpdu->root_path_cost = 0;
        memset(&bpdu->sender_id, 0, sizeof(bpdu->sender_id));
        bpdu->port_id = 0;
        bpdu->message_age = 0;
        bpdu->max_age = 0;
        bpdu->hello_time = 0;
        bpdu->forward_delay = 0;
        return 0;
    }

    /* Configuration BPDU needs 35 bytes */
    if (len < NFS_BPDU_CONFIG_SIZE)
        return -1;

    size_t off = 4;

    /* flags */
    bpdu->flags = data[off++];

    /* root_id */
    nfs_bridge_id_unpack(data + off, &bpdu->root_id);
    off += 8;

    /* root_path_cost */
    uint32_t cost;
    memcpy(&cost, data + off, 4);
    bpdu->root_path_cost = ntohl(cost);
    off += 4;

    /* sender_id */
    nfs_bridge_id_unpack(data + off, &bpdu->sender_id);
    off += 8;

    /* port_id */
    uint16_t pid;
    memcpy(&pid, data + off, 2);
    bpdu->port_id = ntohs(pid);
    off += 2;

    /* message_age */
    uint16_t msg_age;
    memcpy(&msg_age, data + off, 2);
    bpdu->message_age = ntohs(msg_age);
    off += 2;

    /* max_age */
    uint16_t max_a;
    memcpy(&max_a, data + off, 2);
    bpdu->max_age = ntohs(max_a);
    off += 2;

    /* hello_time */
    uint16_t hello;
    memcpy(&hello, data + off, 2);
    bpdu->hello_time = ntohs(hello);
    off += 2;

    /* forward_delay */
    uint16_t fwd;
    memcpy(&fwd, data + off, 2);
    bpdu->forward_delay = ntohs(fwd);
    off += 2;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  BPDU superiority                                                   */
/* ------------------------------------------------------------------ */

int nfs_bpdu_superior(const struct nfs_bpdu *a, const struct nfs_bpdu *b) {
    /* Compare root_id: lower wins */
    int cmp = nfs_bridge_id_cmp(&a->root_id, &b->root_id);
    if (cmp < 0)
        return 1;
    if (cmp > 0)
        return 0;

    /* Same root: compare root_path_cost */
    if (a->root_path_cost < b->root_path_cost)
        return 1;
    if (a->root_path_cost > b->root_path_cost)
        return 0;

    /* Same cost: compare sender_id */
    cmp = nfs_bridge_id_cmp(&a->sender_id, &b->sender_id);
    if (cmp < 0)
        return 1;
    if (cmp > 0)
        return 0;

    /* Same sender: compare port_id */
    if (a->port_id < b->port_id)
        return 1;
    if (a->port_id > b->port_id)
        return 0;

    /* Completely equal: a is not strictly superior */
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Port state machine                                                 */
/* ------------------------------------------------------------------ */

void nfs_stp_port_init(struct nfs_stp_port *port, uint16_t port_id, uint32_t path_cost) {
    if (!port)
        return;
    memset(port, 0, sizeof(*port));
    port->port_id = port_id;
    port->state = NFS_STP_BLOCKING;
    port->role = NFS_STP_DESIGNATED_PORT;
    port->path_cost = path_cost;
}

uint32_t nfs_stp_path_cost(int speed_mbps) {
    switch (speed_mbps) {
    case 10:
        return 100;
    case 100:
        return 19;
    case 1000:
        return 4;
    case 10000:
        return 2;
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/*  Bridge init / BPDU processing                                      */
/* ------------------------------------------------------------------ */

void nfs_stp_bridge_init(struct nfs_stp_bridge *br, uint16_t priority, const uint8_t mac[6]) {
    if (!br)
        return;
    memset(br, 0, sizeof(*br));
    br->id.priority = priority;
    memcpy(br->id.mac, mac, 6);

    /* Assume self is root */
    br->root_id = br->id;
    br->root_path_cost = 0;
    br->root_port = -1;
    br->num_ports = 0;
}

void nfs_stp_process_bpdu(struct nfs_stp_bridge *br, int port_idx, const struct nfs_bpdu *bpdu) {
    if (!br || !bpdu)
        return;
    if (port_idx < 0 || port_idx >= br->num_ports)
        return;

    /*
     * Check if the received BPDU advertises a better root than we know.
     * Build a "received info" combining the BPDU root + cost through this port.
     */
    int cmp = nfs_bridge_id_cmp(&bpdu->root_id, &br->root_id);

    if (cmp < 0) {
        /* Received a superior root: update our root info */
        br->root_id = bpdu->root_id;
        br->root_path_cost = bpdu->root_path_cost + br->ports[port_idx].path_cost;
        br->root_port = port_idx;

        /* The port that leads to root is the root port */
        br->ports[port_idx].role = NFS_STP_ROOT_PORT;
        br->ports[port_idx].best_bpdu = *bpdu;

        /* Other ports become designated (they forward for our segment) */
        for (int i = 0; i < br->num_ports; i++) {
            if (i != port_idx) {
                br->ports[i].role = NFS_STP_DESIGNATED_PORT;
            }
        }
    } else if (cmp == 0 && br->root_port == port_idx) {
        /* Same root, update cost if BPDU came on our root port */
        uint32_t new_cost = bpdu->root_path_cost + br->ports[port_idx].path_cost;
        br->root_path_cost = new_cost;
        br->ports[port_idx].best_bpdu = *bpdu;
    } else if (cmp == 0 && br->root_port != -1) {
        /* Same root but on a non-root port: check if this path is better */
        uint32_t new_cost = bpdu->root_path_cost + br->ports[port_idx].path_cost;
        if (new_cost < br->root_path_cost) {
            br->root_path_cost = new_cost;
            br->ports[br->root_port].role = NFS_STP_DESIGNATED_PORT;
            br->root_port = port_idx;
            br->ports[port_idx].role = NFS_STP_ROOT_PORT;
            br->ports[port_idx].best_bpdu = *bpdu;
        } else {
            /* This port has a worse path; it becomes alternate (blocking) */
            br->ports[port_idx].role = NFS_STP_ALTERNATE_PORT;
            br->ports[port_idx].best_bpdu = *bpdu;
        }
    }
    /* If cmp > 0, the received root is worse than ours: ignore (we are root or know a better one)
     */
}
