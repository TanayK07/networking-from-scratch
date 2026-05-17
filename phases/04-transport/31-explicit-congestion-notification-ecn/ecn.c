/*
 * ecn.c -- ECN (Explicit Congestion Notification) implementation
 */
#include "ecn.h"
#include <arpa/inet.h>
#include <string.h>

/* ---- IP ECN field operations ---- */

nfs_ecn_codepoint_t nfs_ecn_get_ip(uint8_t tos) {
    return (nfs_ecn_codepoint_t)(tos & 0x03);
}

uint8_t nfs_ecn_set_ip(uint8_t tos, nfs_ecn_codepoint_t ecn) {
    return (tos & 0xFC) | ((uint8_t)ecn & 0x03);
}

int nfs_ecn_is_ect(uint8_t tos) {
    nfs_ecn_codepoint_t ecn = nfs_ecn_get_ip(tos);
    return (ecn == NFS_ECN_ECT0 || ecn == NFS_ECN_ECT1) ? 1 : 0;
}

int nfs_ecn_is_ce(uint8_t tos) {
    return nfs_ecn_get_ip(tos) == NFS_ECN_CE ? 1 : 0;
}

int nfs_ecn_mark_ce(uint8_t tos) {
    if (!nfs_ecn_is_ect(tos))
        return -1;
    return (int)nfs_ecn_set_ip(tos, NFS_ECN_CE);
}

/* ---- TCP ECN flag operations ---- */

uint16_t nfs_ecn_tcp_get_flags(uint16_t data_off_flags_net) {
    uint16_t host = ntohs(data_off_flags_net);
    return host & 0x0FFF; /* lower 12 bits are flags (including NS) */
}

uint16_t nfs_ecn_tcp_set_flags(uint16_t data_off_flags_net, uint16_t flags_to_set) {
    uint16_t host = ntohs(data_off_flags_net);
    host |= (flags_to_set & 0x0FFF);
    return htons(host);
}

uint16_t nfs_ecn_tcp_clear_flags(uint16_t data_off_flags_net, uint16_t flags_to_clear) {
    uint16_t host = ntohs(data_off_flags_net);
    host &= ~(flags_to_clear & 0x0FFF);
    return htons(host);
}

int nfs_ecn_tcp_has_flags(uint16_t data_off_flags_net, uint16_t flags) {
    uint16_t host = ntohs(data_off_flags_net);
    return ((host & (flags & 0x0FFF)) == (flags & 0x0FFF)) ? 1 : 0;
}

/* ---- ECN negotiation ---- */

void nfs_ecn_nego_init(struct nfs_ecn_negotiation *nego) {
    if (!nego)
        return;
    nego->state = NFS_ECN_STATE_UNKNOWN;
    nego->ecn_capable = 0;
}

uint16_t nfs_ecn_build_syn_flags(void) {
    return NFS_TCP_FLAG_SYN | NFS_TCP_FLAG_ECE | NFS_TCP_FLAG_CWR;
}

uint16_t nfs_ecn_build_synack_accept_flags(void) {
    return NFS_TCP_FLAG_SYN | NFS_TCP_FLAG_ACK | NFS_TCP_FLAG_ECE;
}

uint16_t nfs_ecn_build_synack_reject_flags(void) {
    return NFS_TCP_FLAG_SYN | NFS_TCP_FLAG_ACK;
}

nfs_ecn_state_t nfs_ecn_nego_process(struct nfs_ecn_negotiation *nego, uint16_t tcp_flags,
                                     int is_syn_ack) {
    if (!nego)
        return NFS_ECN_STATE_UNKNOWN;

    if (!is_syn_ack) {
        /* Processing incoming SYN (we are the responder) */
        if ((tcp_flags & NFS_TCP_FLAG_SYN) && (tcp_flags & NFS_TCP_FLAG_ECE) &&
            (tcp_flags & NFS_TCP_FLAG_CWR)) {
            nego->state = NFS_ECN_STATE_REQUESTED;
        }
    } else {
        /* Processing incoming SYN-ACK (we are the initiator) */
        if ((tcp_flags & NFS_TCP_FLAG_SYN) && (tcp_flags & NFS_TCP_FLAG_ACK)) {
            if ((tcp_flags & NFS_TCP_FLAG_ECE) && !(tcp_flags & NFS_TCP_FLAG_CWR)) {
                nego->state = NFS_ECN_STATE_ACCEPTED;
                nego->ecn_capable = 1;
            } else {
                nego->state = NFS_ECN_STATE_REJECTED;
                nego->ecn_capable = 0;
            }
        }
    }

    return nego->state;
}

const char *nfs_ecn_codepoint_name(nfs_ecn_codepoint_t ecn) {
    switch (ecn) {
    case NFS_ECN_NOT_ECT:
        return "Not-ECT";
    case NFS_ECN_ECT1:
        return "ECT(1)";
    case NFS_ECN_ECT0:
        return "ECT(0)";
    case NFS_ECN_CE:
        return "CE";
    default:
        return "Unknown";
    }
}

const char *nfs_ecn_state_name(nfs_ecn_state_t state) {
    switch (state) {
    case NFS_ECN_STATE_UNKNOWN:
        return "UNKNOWN";
    case NFS_ECN_STATE_REQUESTED:
        return "REQUESTED";
    case NFS_ECN_STATE_ACCEPTED:
        return "ACCEPTED";
    case NFS_ECN_STATE_REJECTED:
        return "REJECTED";
    case NFS_ECN_STATE_ACTIVE:
        return "ACTIVE";
    default:
        return "INVALID";
    }
}
