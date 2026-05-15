#ifndef NFS_NTP_H
#define NFS_NTP_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * NTP / SNTP Client (RFC 5905 / RFC 4330)
 *
 * NTP packet: 48 bytes minimum
 *   Byte 0: LI(2) | VN(3) | Mode(3)
 *   Byte 1: Stratum
 *   Byte 2: Poll interval
 *   Byte 3: Precision
 *   Bytes 4-7:   Root Delay (32-bit fixed-point)
 *   Bytes 8-11:  Root Dispersion (32-bit fixed-point)
 *   Bytes 12-15: Reference ID
 *   Bytes 16-23: Reference Timestamp (64-bit NTP)
 *   Bytes 24-31: Origin Timestamp (64-bit NTP)
 *   Bytes 32-39: Receive Timestamp (64-bit NTP)
 *   Bytes 40-47: Transmit Timestamp (64-bit NTP)
 *
 * NTP epoch: 1900-01-01.  Unix epoch: 1970-01-01.
 * Offset = 2208988800 seconds.
 * --------------------------------------------------------------- */

#define NFS_NTP_PACKET_SIZE  48

/* NTP epoch offset from Unix epoch */
#define NFS_NTP_UNIX_OFFSET  2208988800ULL

/* LI (Leap Indicator) values */
#define NFS_NTP_LI_NONE      0
#define NFS_NTP_LI_LAST61    1
#define NFS_NTP_LI_LAST59    2
#define NFS_NTP_LI_ALARM     3

/* Mode values */
#define NFS_NTP_MODE_CLIENT   3
#define NFS_NTP_MODE_SERVER   4

/* NTP 64-bit timestamp: seconds + fraction */
struct nfs_ntp_ts {
    uint32_t seconds;
    uint32_t fraction;
};

/* Parsed NTP packet */
struct nfs_ntp_packet {
    uint8_t  li;           /* Leap Indicator (2 bits) */
    uint8_t  vn;           /* Version Number (3 bits) */
    uint8_t  mode;         /* Mode (3 bits) */
    uint8_t  stratum;
    int8_t   poll;
    int8_t   precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t ref_id;
    struct nfs_ntp_ts ref_ts;
    struct nfs_ntp_ts origin_ts;
    struct nfs_ntp_ts recv_ts;
    struct nfs_ntp_ts xmit_ts;
};

/* Parse a 48-byte NTP packet.
 * Returns 0 on success, -1 on error. */
int nfs_ntp_parse(const uint8_t *buf, size_t len,
                  struct nfs_ntp_packet *out);

/* Build a 48-byte NTP packet into buf (must be >= 48 bytes).
 * Returns bytes written (48), or -1 on error. */
int nfs_ntp_build(uint8_t *buf, size_t out_sz,
                  const struct nfs_ntp_packet *pkt);

/* Convert NTP timestamp to Unix seconds (double for sub-second precision). */
double nfs_ntp_to_unix(const struct nfs_ntp_ts *ts);

/* Convert Unix timestamp (seconds since 1970) to NTP timestamp. */
struct nfs_ntp_ts nfs_unix_to_ntp(double unix_time);

/* Calculate clock offset from four timestamps:
 *   t1 = client send (origin), t2 = server receive,
 *   t3 = server transmit,      t4 = client receive
 * offset = ((t2-t1) + (t3-t4)) / 2 */
double nfs_ntp_offset(const struct nfs_ntp_ts *t1,
                      const struct nfs_ntp_ts *t2,
                      const struct nfs_ntp_ts *t3,
                      const struct nfs_ntp_ts *t4);

/* Calculate round-trip time:
 *   rtt = (t4-t1) - (t3-t2) */
double nfs_ntp_rtt(const struct nfs_ntp_ts *t1,
                   const struct nfs_ntp_ts *t2,
                   const struct nfs_ntp_ts *t3,
                   const struct nfs_ntp_ts *t4);

#endif /* NFS_NTP_H */
