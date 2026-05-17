#ifndef NFS_ECN_H
#define NFS_ECN_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * ECN (Explicit Congestion Notification) - RFC 3168
 *
 * IP header ECN field (bits 6-7 of the TOS/DSCP byte):
 *   00 = Not-ECT (not ECN-capable)
 *   01 = ECT(1)  (ECN-capable transport)
 *   10 = ECT(0)  (ECN-capable transport)
 *   11 = CE      (Congestion Experienced)
 *
 * TCP header ECN flags (in the TCP flags field):
 *   NS  (Nonce Sum)       - bit 8 of data offset/flags word
 *   CWR (Congestion Window Reduced) - bit 7
 *   ECE (ECN-Echo)        - bit 6
 *
 * ECN negotiation:
 *   SYN:     sender sets ECE+CWR to propose ECN
 *   SYN-ACK: responder sets ECE (but not CWR) to accept
 * --------------------------------------------------------------- */

/* ECN codepoints in IP TOS byte (bits 0-1 of the 2 ECN bits) */
typedef enum {
    NFS_ECN_NOT_ECT = 0x00, /* Not ECN-Capable Transport */
    NFS_ECN_ECT1 = 0x01,    /* ECN-Capable Transport(1) */
    NFS_ECN_ECT0 = 0x02,    /* ECN-Capable Transport(0) */
    NFS_ECN_CE = 0x03,      /* Congestion Experienced */
} nfs_ecn_codepoint_t;

/* TCP ECN-related flags */
#define NFS_TCP_FLAG_NS  0x100 /* Nonce Sum (bit 8) */
#define NFS_TCP_FLAG_CWR 0x080 /* CWR (bit 7) */
#define NFS_TCP_FLAG_ECE 0x040 /* ECE (bit 6) */
#define NFS_TCP_FLAG_SYN 0x002 /* SYN */
#define NFS_TCP_FLAG_ACK 0x010 /* ACK */

/* ECN negotiation state */
typedef enum {
    NFS_ECN_STATE_UNKNOWN = 0,
    NFS_ECN_STATE_REQUESTED = 1, /* SYN sent with ECE+CWR */
    NFS_ECN_STATE_ACCEPTED = 2,  /* SYN-ACK with ECE (no CWR) */
    NFS_ECN_STATE_REJECTED = 3,  /* SYN-ACK without ECE */
    NFS_ECN_STATE_ACTIVE = 4,    /* ECN negotiation complete */
} nfs_ecn_state_t;

/* ---- Minimal IPv4 header for ECN operations ---- */

struct nfs_ecn_ip_hdr {
    uint8_t ver_ihl;
    uint8_t tos; /* DSCP (6 bits) | ECN (2 bits) */
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_addr;
    uint32_t dst_addr;
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ecn_ip_hdr) == 20, "IP header must be 20 bytes");

/* ---- Minimal TCP header for ECN operations ---- */

struct nfs_ecn_tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint16_t data_off_flags; /* data offset (4) | reserved (3) | NS | CWR | ECE | URG | ACK | PSH |
                                RST | SYN | FIN */
    uint16_t window;
    uint16_t checksum;
    uint16_t urg_ptr;
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ecn_tcp_hdr) == 20, "TCP header must be 20 bytes");

/* ---- ECN negotiation tracker ---- */

struct nfs_ecn_negotiation {
    nfs_ecn_state_t state;
    int ecn_capable; /* 1 if ECN was successfully negotiated */
};

/* ---- IP ECN field operations ---- */

/* Get the ECN codepoint from an IP TOS byte. */
nfs_ecn_codepoint_t nfs_ecn_get_ip(uint8_t tos);

/* Set the ECN codepoint in an IP TOS byte.
 * Returns the modified TOS byte (preserves DSCP bits). */
uint8_t nfs_ecn_set_ip(uint8_t tos, nfs_ecn_codepoint_t ecn);

/* Check if an IP packet is ECN-capable (ECT(0) or ECT(1)). */
int nfs_ecn_is_ect(uint8_t tos);

/* Check if an IP packet has Congestion Experienced. */
int nfs_ecn_is_ce(uint8_t tos);

/* Mark an IP packet as Congestion Experienced (set CE).
 * Only valid if packet is already ECT. Returns new TOS, or -1 if not ECT. */
int nfs_ecn_mark_ce(uint8_t tos);

/* ---- TCP ECN flag operations ---- */

/* Get the 12-bit flags from TCP data_off_flags (network byte order). */
uint16_t nfs_ecn_tcp_get_flags(uint16_t data_off_flags_net);

/* Set specific TCP flags in data_off_flags (network byte order).
 * Returns the modified data_off_flags in network byte order. */
uint16_t nfs_ecn_tcp_set_flags(uint16_t data_off_flags_net, uint16_t flags_to_set);

/* Clear specific TCP flags. */
uint16_t nfs_ecn_tcp_clear_flags(uint16_t data_off_flags_net, uint16_t flags_to_clear);

/* Check if specific TCP flags are set. */
int nfs_ecn_tcp_has_flags(uint16_t data_off_flags_net, uint16_t flags);

/* ---- ECN negotiation ---- */

/* Initialize negotiation state. */
void nfs_ecn_nego_init(struct nfs_ecn_negotiation *nego);

/* Build SYN flags for ECN negotiation (sets ECE+CWR+SYN). */
uint16_t nfs_ecn_build_syn_flags(void);

/* Build SYN-ACK flags accepting ECN (sets ECE+SYN+ACK, no CWR). */
uint16_t nfs_ecn_build_synack_accept_flags(void);

/* Build SYN-ACK flags rejecting ECN (sets SYN+ACK only). */
uint16_t nfs_ecn_build_synack_reject_flags(void);

/* Process received SYN/SYN-ACK and update negotiation state.
 * `tcp_flags` is the 12-bit TCP flags from the received segment.
 * `is_syn_ack` = 1 if this is a SYN-ACK (responder's reply).
 * Returns new state. */
nfs_ecn_state_t nfs_ecn_nego_process(struct nfs_ecn_negotiation *nego, uint16_t tcp_flags,
                                     int is_syn_ack);

/* Get human-readable name for ECN codepoint. */
const char *nfs_ecn_codepoint_name(nfs_ecn_codepoint_t ecn);

/* Get human-readable name for negotiation state. */
const char *nfs_ecn_state_name(nfs_ecn_state_t state);

#endif /* NFS_ECN_H */
