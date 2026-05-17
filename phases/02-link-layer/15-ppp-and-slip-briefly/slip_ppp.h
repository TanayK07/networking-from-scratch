#ifndef NFS_SLIP_PPP_H
#define NFS_SLIP_PPP_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * SLIP (Serial Line Internet Protocol) - RFC 1055
 *
 * SLIP framing uses special byte values:
 *   END     0xC0  - marks frame boundaries
 *   ESC     0xDB  - escape prefix
 *   ESC_END 0xDC  - follows ESC when payload contains 0xC0
 *   ESC_ESC 0xDD  - follows ESC when payload contains 0xDB
 * --------------------------------------------------------------- */

#define NFS_SLIP_END     0xC0
#define NFS_SLIP_ESC     0xDB
#define NFS_SLIP_ESC_END 0xDC
#define NFS_SLIP_ESC_ESC 0xDD

/* Encode raw IP datagram into a SLIP frame.
 * Writes END + escaped payload + END into `out`.
 * Returns number of bytes written, or -1 if out_sz is too small. */
int nfs_slip_encode(const uint8_t *data, size_t len, uint8_t *out, size_t out_sz);

/* Decode a SLIP frame back to raw IP datagram.
 * `frame` should start with END and end with END.
 * Returns number of payload bytes written to `out`, or -1 on error. */
int nfs_slip_decode(const uint8_t *frame, size_t frame_len, uint8_t *out, size_t out_sz);

/* ---------------------------------------------------------------
 * PPP (Point-to-Point Protocol) - RFC 1662
 *
 * PPP frame format (HDLC-like):
 *   Flag (0x7E) | Address (0xFF) | Control (0x03) | Protocol (2B)
 *   | Payload | FCS (2B, CRC-16/HDLC) | Flag (0x7E)
 *
 * Protocol field: 0x0021 = IPv4, 0x0057 = IPv6, 0xC021 = LCP, etc.
 * FCS is CRC-16/CCITT (x^16 + x^12 + x^5 + 1) over address+control+protocol+payload.
 * --------------------------------------------------------------- */

#define NFS_PPP_FLAG 0x7E
#define NFS_PPP_ADDR 0xFF
#define NFS_PPP_CTRL 0x03

#define NFS_PPP_PROTO_IPV4 0x0021
#define NFS_PPP_PROTO_IPV6 0x0057
#define NFS_PPP_PROTO_LCP  0xC021
#define NFS_PPP_PROTO_IPCP 0x8021

#define NFS_PPP_MAX_PAYLOAD 1500
#define NFS_PPP_FCS_SIZE    2

struct nfs_ppp_frame {
    uint8_t address;
    uint8_t control;
    uint16_t protocol; /* host byte order */
    uint8_t payload[NFS_PPP_MAX_PAYLOAD];
    uint16_t payload_len;
    uint16_t fcs; /* computed/parsed FCS */
} __attribute__((packed));

_Static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");

/* Compute CRC-16/HDLC (FCS) over `data` of `len` bytes.
 * Initial value 0xFFFF, final XOR 0xFFFF. */
uint16_t nfs_ppp_fcs16(const uint8_t *data, size_t len);

/* Build a PPP frame into `out`.
 * Writes: Flag | Addr | Ctrl | Protocol | Payload | FCS | Flag
 * Returns total bytes written, or -1 on error. */
int nfs_ppp_frame_build(uint16_t protocol, const uint8_t *payload, size_t payload_len, uint8_t *out,
                        size_t out_sz);

/* Parse a PPP frame from `data` into `frame`.
 * Validates flag bytes, address, control, and FCS.
 * Returns 0 on success, -1 on error. */
int nfs_ppp_frame_parse(const uint8_t *data, size_t len, struct nfs_ppp_frame *frame);

/* Return protocol name string for known PPP protocols. */
const char *nfs_ppp_protocol_name(uint16_t protocol);

#endif /* NFS_SLIP_PPP_H */
