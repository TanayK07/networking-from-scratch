#ifndef NFS_TCP_H
#define NFS_TCP_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP segment header (RFC 9293, Section 3.1).
 *
 * The minimum TCP header is 20 bytes. Options may follow, but this
 * lesson only implements the fixed header for the three-way handshake.
 * --------------------------------------------------------------- */

struct nfs_tcp_hdr {
    uint16_t src_port;      /* source port, network order */
    uint16_t dst_port;      /* destination port, network order */
    uint32_t seq_num;       /* sequence number, network order */
    uint32_t ack_num;       /* acknowledgement number, network order */
    uint8_t  data_off_rsv;  /* data offset (4 bits) | reserved (4 bits) */
    uint8_t  flags;         /* CWR ECE URG ACK PSH RST SYN FIN */
    uint16_t window;        /* window size, network order */
    uint16_t checksum;      /* TCP checksum, network order */
    uint16_t urgent_ptr;    /* urgent pointer, network order */
} __attribute__((packed));

/* Compile-time guarantee: the struct matches the wire layout. */
_Static_assert(sizeof(struct nfs_tcp_hdr) == 20,
               "nfs_tcp_hdr must be exactly 20 bytes");

/* ---------------------------------------------------------------
 * TCP flag bits (low byte of the 13th octet in the header).
 * --------------------------------------------------------------- */

#define NFS_TCP_FIN  0x01
#define NFS_TCP_SYN  0x02
#define NFS_TCP_RST  0x04
#define NFS_TCP_PSH  0x08
#define NFS_TCP_ACK  0x10
#define NFS_TCP_URG  0x20
#define NFS_TCP_ECE  0x40
#define NFS_TCP_CWR  0x80

/* ---------------------------------------------------------------
 * Handshake state machine (simplified from RFC 9293 Figure 6).
 * --------------------------------------------------------------- */

enum nfs_tcp_state {
    NFS_TCP_CLOSED = 0,
    NFS_TCP_SYN_SENT,
    NFS_TCP_SYN_RECEIVED,
    NFS_TCP_ESTABLISHED,
};

/* Per-connection handshake context. Tracks the state and sequence
 * numbers for one side of a three-way handshake. */
struct nfs_tcp_handshake {
    enum nfs_tcp_state state;
    uint32_t local_seq;     /* our ISN (host order) */
    uint32_t remote_seq;    /* peer's ISN (host order) */
    uint16_t local_port;    /* host order */
    uint16_t remote_port;   /* host order */
};

/* ---------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

/* Initialize a handshake context. */
void nfs_tcp_handshake_init(struct nfs_tcp_handshake *ctx,
                            uint16_t local_port,
                            uint16_t remote_port);

/* Generate a randomized Initial Sequence Number (ISN).
 * Per RFC 6528 spirit: based on time + randomness. */
uint32_t nfs_tcp_generate_isn(void);

/* Parse raw bytes into a tcp_hdr struct. Fields are converted to
 * host byte order in `out`. Returns 0 on success, -1 on error. */
int nfs_tcp_parse(const uint8_t *buf, size_t len,
                  struct nfs_tcp_hdr *out);

/* Build a SYN segment. Sets the ISN in ctx->local_seq.
 * Returns bytes written, or 0 on error. */
size_t nfs_tcp_build_syn(struct nfs_tcp_handshake *ctx,
                         uint8_t *buf, size_t len);

/* Build a SYN-ACK in response to a parsed SYN header.
 * The `syn_hdr` must have been produced by nfs_tcp_parse (host order).
 * Returns bytes written, or 0 on error. */
size_t nfs_tcp_build_synack(struct nfs_tcp_handshake *ctx,
                            const struct nfs_tcp_hdr *syn_hdr,
                            uint8_t *buf, size_t len);

/* Build the final ACK in response to a parsed SYN-ACK header.
 * Returns bytes written, or 0 on error. */
size_t nfs_tcp_build_ack(struct nfs_tcp_handshake *ctx,
                         const struct nfs_tcp_hdr *synack_hdr,
                         uint8_t *buf, size_t len);

/* TCP pseudo-header checksum for IPv4.
 * src_ip and dst_ip are in network byte order (4 bytes each).
 * tcp_buf points to the raw TCP segment (header + payload).
 * Returns the checksum in network byte order. */
uint16_t nfs_tcp_checksum_pseudo(const uint8_t *src_ip,
                                 const uint8_t *dst_ip,
                                 const uint8_t *tcp_buf,
                                 size_t tcp_len);

#endif /* NFS_TCP_H */
