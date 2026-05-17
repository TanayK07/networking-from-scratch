/* rto.c — RFC 6298 Retransmission Timeout calculator.
 *
 * Implements the SRTT/RTTVAR/RTO computation described in RFC 6298,
 * "Computing TCP's Retransmission Timer". */

#include "rto.h"

#include <math.h>

/* Clamp RTO to [RTO_MIN, RTO_MAX]. */
static double clamp_rto(double rto) {
    if (rto < NFS_RTO_MIN)
        return NFS_RTO_MIN;
    if (rto > NFS_RTO_MAX)
        return NFS_RTO_MAX;
    return rto;
}

void nfs_rto_init(struct nfs_rto_state *s) {
    s->srtt = 0.0;
    s->rttvar = 0.0;
    s->rto = NFS_RTO_MIN; /* RFC 6298 Section 2.1: initial RTO = 1s */
    s->initialized = 0;
}

void nfs_rto_update(struct nfs_rto_state *s, double rtt_sample) {
    if (!s->initialized) {
        /* RFC 6298 Section 2.2: first measurement */
        s->srtt = rtt_sample;
        s->rttvar = rtt_sample / 2.0;
        s->initialized = 1;
    } else {
        /* RFC 6298 Section 2.3: subsequent measurements.
         * IMPORTANT: RTTVAR must be updated BEFORE SRTT (uses old SRTT). */
        double delta = fabs(s->srtt - rtt_sample);
        s->rttvar = (1.0 - NFS_RTO_BETA) * s->rttvar + NFS_RTO_BETA * delta;
        s->srtt = (1.0 - NFS_RTO_ALPHA) * s->srtt + NFS_RTO_ALPHA * rtt_sample;
    }

    /* RTO = SRTT + max(G, K * RTTVAR) */
    double k_rttvar = NFS_RTO_K * s->rttvar;
    double addend = (NFS_RTO_G > k_rttvar) ? NFS_RTO_G : k_rttvar;
    s->rto = clamp_rto(s->srtt + addend);
}

void nfs_rto_backoff(struct nfs_rto_state *s) {
    /* RFC 6298 Section 5.5: double the RTO */
    s->rto = clamp_rto(s->rto * 2.0);
}

double nfs_rto_get(const struct nfs_rto_state *s) {
    return s->rto;
}

double nfs_rto_srtt(const struct nfs_rto_state *s) {
    return s->srtt;
}

double nfs_rto_rttvar(const struct nfs_rto_state *s) {
    return s->rttvar;
}
