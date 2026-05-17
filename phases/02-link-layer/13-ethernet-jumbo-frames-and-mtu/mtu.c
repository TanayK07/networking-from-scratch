#include "mtu.h"

/* ------------------------------------------------------------------ */
/*  Frame validation                                                   */
/* ------------------------------------------------------------------ */

int nfs_mtu_frame_valid(size_t frame_len, size_t mtu) {
    if (frame_len < NFS_ETH_MIN_FRAME)
        return 0;
    if (frame_len > (size_t)(NFS_ETH_HDR_LEN + mtu + NFS_ETH_FCS_LEN))
        return 0;
    return 1;
}

int nfs_mtu_payload_fits(size_t payload_len, size_t mtu) {
    return payload_len <= mtu ? 1 : 0;
}

int nfs_mtu_needs_fragmentation(size_t ip_packet_len, size_t mtu) {
    return ip_packet_len > mtu ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/*  Efficiency calculations                                            */
/* ------------------------------------------------------------------ */

double nfs_mtu_efficiency(size_t payload_len) {
    size_t padded = payload_len < NFS_ETH_MIN_PAYLOAD ? NFS_ETH_MIN_PAYLOAD : payload_len;
    size_t wire = NFS_PHY_OVERHEAD + NFS_ETH_HDR_LEN + padded + NFS_ETH_FCS_LEN;
    return (double)payload_len / (double)wire;
}

double nfs_mtu_goodput_bps(size_t payload_len, int speed_mbps) {
    size_t padded = payload_len < NFS_ETH_MIN_PAYLOAD ? NFS_ETH_MIN_PAYLOAD : payload_len;
    size_t wire = NFS_PHY_OVERHEAD + NFS_ETH_HDR_LEN + padded + NFS_ETH_FCS_LEN;
    double speed_bps = (double)speed_mbps * 1e6;
    double fps = speed_bps / ((double)wire * 8.0);
    return fps * (double)payload_len * 8.0;
}

size_t nfs_mtu_max_fps(size_t frame_len, int speed_mbps) {
    size_t wire = frame_len + NFS_PHY_OVERHEAD;
    double speed_bps = (double)speed_mbps * 1e6;
    return (size_t)(speed_bps / ((double)wire * 8.0));
}

/* ------------------------------------------------------------------ */
/*  MTU profiles                                                       */
/* ------------------------------------------------------------------ */

struct nfs_mtu_profile nfs_mtu_standard(void) {
    struct nfs_mtu_profile p;
    p.name = "standard";
    p.mtu = NFS_ETH_MAX_PAYLOAD;
    p.min_frame = NFS_ETH_MIN_FRAME;
    p.max_frame = NFS_ETH_MAX_FRAME;
    return p;
}

struct nfs_mtu_profile nfs_mtu_jumbo(void) {
    struct nfs_mtu_profile p;
    p.name = "jumbo";
    p.mtu = NFS_JUMBO_MTU;
    p.min_frame = NFS_ETH_MIN_FRAME;
    p.max_frame = NFS_JUMBO_MAX_FRAME;
    return p;
}

/* ------------------------------------------------------------------ */
/*  Path MTU helpers                                                   */
/* ------------------------------------------------------------------ */

size_t nfs_path_mtu(const size_t *link_mtus, int num_links) {
    if (!link_mtus || num_links <= 0)
        return 0;
    size_t min = link_mtus[0];
    for (int i = 1; i < num_links; i++) {
        if (link_mtus[i] < min)
            min = link_mtus[i];
    }
    return min;
}

size_t nfs_mtu_clamp(size_t desired_mtu, size_t min_mtu, size_t max_mtu) {
    if (desired_mtu < min_mtu)
        return min_mtu;
    if (desired_mtu > max_mtu)
        return max_mtu;
    return desired_mtu;
}
