#include "ntp.h"
#include <arpa/inet.h>
#include <math.h>
#include <string.h>

/* ---------------------------------------------------------------
 * NTP timestamp helpers
 * --------------------------------------------------------------- */

/* Convert NTP 64-bit timestamp to a double (seconds since NTP epoch). */
static double ntp_ts_to_double(const struct nfs_ntp_ts *ts) {
    return (double)ts->seconds + (double)ts->fraction / 4294967296.0;
}

/* ---------------------------------------------------------------
 * Parse a 48-byte NTP packet from wire format.
 * --------------------------------------------------------------- */

int nfs_ntp_parse(const uint8_t *buf, size_t len, struct nfs_ntp_packet *out) {
    if (!buf || !out || len < NFS_NTP_PACKET_SIZE)
        return -1;

    memset(out, 0, sizeof(*out));

    out->li = (buf[0] >> 6) & 0x03;
    out->vn = (buf[0] >> 3) & 0x07;
    out->mode = buf[0] & 0x07;

    out->stratum = buf[1];
    out->poll = (int8_t)buf[2];
    out->precision = (int8_t)buf[3];

    memcpy(&out->root_delay, &buf[4], 4);
    memcpy(&out->root_dispersion, &buf[8], 4);
    memcpy(&out->ref_id, &buf[12], 4);

    out->root_delay = ntohl(out->root_delay);
    out->root_dispersion = ntohl(out->root_dispersion);
    out->ref_id = ntohl(out->ref_id);

    /* Timestamps: each is 8 bytes (4 seconds + 4 fraction) */
    uint32_t tmp;
    memcpy(&tmp, &buf[16], 4);
    out->ref_ts.seconds = ntohl(tmp);
    memcpy(&tmp, &buf[20], 4);
    out->ref_ts.fraction = ntohl(tmp);
    memcpy(&tmp, &buf[24], 4);
    out->origin_ts.seconds = ntohl(tmp);
    memcpy(&tmp, &buf[28], 4);
    out->origin_ts.fraction = ntohl(tmp);
    memcpy(&tmp, &buf[32], 4);
    out->recv_ts.seconds = ntohl(tmp);
    memcpy(&tmp, &buf[36], 4);
    out->recv_ts.fraction = ntohl(tmp);
    memcpy(&tmp, &buf[40], 4);
    out->xmit_ts.seconds = ntohl(tmp);
    memcpy(&tmp, &buf[44], 4);
    out->xmit_ts.fraction = ntohl(tmp);

    return 0;
}

/* ---------------------------------------------------------------
 * Build a 48-byte NTP packet in wire format.
 * --------------------------------------------------------------- */

int nfs_ntp_build(uint8_t *buf, size_t out_sz, const struct nfs_ntp_packet *pkt) {
    if (!buf || !pkt || out_sz < NFS_NTP_PACKET_SIZE)
        return -1;

    memset(buf, 0, NFS_NTP_PACKET_SIZE);

    buf[0] = (uint8_t)(((pkt->li & 0x03) << 6) | ((pkt->vn & 0x07) << 3) | (pkt->mode & 0x07));
    buf[1] = pkt->stratum;
    buf[2] = (uint8_t)pkt->poll;
    buf[3] = (uint8_t)pkt->precision;

    uint32_t tmp;
    tmp = htonl(pkt->root_delay);
    memcpy(&buf[4], &tmp, 4);
    tmp = htonl(pkt->root_dispersion);
    memcpy(&buf[8], &tmp, 4);
    tmp = htonl(pkt->ref_id);
    memcpy(&buf[12], &tmp, 4);

    tmp = htonl(pkt->ref_ts.seconds);
    memcpy(&buf[16], &tmp, 4);
    tmp = htonl(pkt->ref_ts.fraction);
    memcpy(&buf[20], &tmp, 4);
    tmp = htonl(pkt->origin_ts.seconds);
    memcpy(&buf[24], &tmp, 4);
    tmp = htonl(pkt->origin_ts.fraction);
    memcpy(&buf[28], &tmp, 4);
    tmp = htonl(pkt->recv_ts.seconds);
    memcpy(&buf[32], &tmp, 4);
    tmp = htonl(pkt->recv_ts.fraction);
    memcpy(&buf[36], &tmp, 4);
    tmp = htonl(pkt->xmit_ts.seconds);
    memcpy(&buf[40], &tmp, 4);
    tmp = htonl(pkt->xmit_ts.fraction);
    memcpy(&buf[44], &tmp, 4);

    return NFS_NTP_PACKET_SIZE;
}

/* ---------------------------------------------------------------
 * Timestamp conversions
 * --------------------------------------------------------------- */

double nfs_ntp_to_unix(const struct nfs_ntp_ts *ts) {
    if (!ts)
        return 0.0;
    return ntp_ts_to_double(ts) - (double)NFS_NTP_UNIX_OFFSET;
}

struct nfs_ntp_ts nfs_unix_to_ntp(double unix_time) {
    struct nfs_ntp_ts ts;
    double ntp_time = unix_time + (double)NFS_NTP_UNIX_OFFSET;
    ts.seconds = (uint32_t)ntp_time;
    ts.fraction = (uint32_t)((ntp_time - (double)ts.seconds) * 4294967296.0);
    return ts;
}

/* ---------------------------------------------------------------
 * Clock offset and RTT calculations
 * --------------------------------------------------------------- */

double nfs_ntp_offset(const struct nfs_ntp_ts *t1, const struct nfs_ntp_ts *t2,
                      const struct nfs_ntp_ts *t3, const struct nfs_ntp_ts *t4) {
    if (!t1 || !t2 || !t3 || !t4)
        return 0.0;

    double d1 = ntp_ts_to_double(t1);
    double d2 = ntp_ts_to_double(t2);
    double d3 = ntp_ts_to_double(t3);
    double d4 = ntp_ts_to_double(t4);

    return ((d2 - d1) + (d3 - d4)) / 2.0;
}

double nfs_ntp_rtt(const struct nfs_ntp_ts *t1, const struct nfs_ntp_ts *t2,
                   const struct nfs_ntp_ts *t3, const struct nfs_ntp_ts *t4) {
    if (!t1 || !t2 || !t3 || !t4)
        return 0.0;

    double d1 = ntp_ts_to_double(t1);
    double d2 = ntp_ts_to_double(t2);
    double d3 = ntp_ts_to_double(t3);
    double d4 = ntp_ts_to_double(t4);

    return (d4 - d1) - (d3 - d2);
}
