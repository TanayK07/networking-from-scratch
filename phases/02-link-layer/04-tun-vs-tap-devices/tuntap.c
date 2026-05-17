#include "tuntap.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  ifreq builder                                                     */
/* ------------------------------------------------------------------ */

int nfs_tuntap_prepare_ifr(struct ifreq *ifr, const char *dev_name, int mode, int no_pi) {
    if (!ifr || !dev_name) {
        return -1;
    }

    size_t namelen = strlen(dev_name);
    if (namelen > IFNAMSIZ - 1) {
        return -1;
    }

    memset(ifr, 0, sizeof(*ifr));

    /* Copy device name (empty string is fine -- kernel will auto-assign). */
    memcpy(ifr->ifr_name, dev_name, namelen);
    ifr->ifr_name[namelen] = '\0';

    /* Set device type flag. */
    short flags = (mode == NFS_TAP_MODE) ? IFF_TAP : IFF_TUN;
    if (no_pi) {
        flags |= IFF_NO_PI;
    }
    ifr->ifr_flags = flags;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Device name validation                                            */
/* ------------------------------------------------------------------ */

int nfs_tuntap_valid_name(const char *name) {
    if (!name) {
        return 0;
    }

    size_t len = strlen(name);
    if (len == 0 || len > IFNAMSIZ - 1) {
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!isalnum((unsigned char)c) && c != '-' && c != '_') {
            return 0;
        }
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/*  PI header build / parse                                           */
/* ------------------------------------------------------------------ */

int nfs_pi_build(uint8_t *out, size_t out_sz, uint16_t flags, uint16_t proto) {
    if (!out || out_sz < NFS_PI_HDR_SIZE) {
        return -1;
    }

    /* PI header fields are in network byte order. */
    uint16_t nflags = htons(flags);
    uint16_t nproto = htons(proto);

    memcpy(out, &nflags, 2);
    memcpy(out + 2, &nproto, 2);

    return NFS_PI_HDR_SIZE;
}

int nfs_pi_parse(const uint8_t *data, size_t len, uint16_t *flags, uint16_t *proto) {
    if (!data || len < NFS_PI_HDR_SIZE || !flags || !proto) {
        return -1;
    }

    uint16_t nflags, nproto;
    memcpy(&nflags, data, 2);
    memcpy(&nproto, data + 2, 2);

    *flags = ntohs(nflags);
    *proto = ntohs(nproto);

    return 0;
}

/* ------------------------------------------------------------------ */
/*  TUN wrap / unwrap                                                 */
/* ------------------------------------------------------------------ */

int nfs_tun_wrap_ip(const uint8_t *ip_pkt, size_t ip_len, uint8_t *out, size_t out_sz) {
    if (!ip_pkt || !out) {
        return -1;
    }

    size_t total = NFS_PI_HDR_SIZE + ip_len;
    if (out_sz < total) {
        return -1;
    }

    /* Build PI header with proto = ETH_P_IP (0x0800). */
    if (nfs_pi_build(out, out_sz, 0, 0x0800) < 0) {
        return -1;
    }

    memcpy(out + NFS_PI_HDR_SIZE, ip_pkt, ip_len);
    return (int)total;
}

int nfs_tun_unwrap(const uint8_t *data, size_t len, uint16_t *proto, const uint8_t **payload,
                   size_t *payload_len) {
    if (!data || !proto || !payload || !payload_len) {
        return -1;
    }
    if (len < NFS_PI_HDR_SIZE) {
        return -1;
    }

    uint16_t flags;
    if (nfs_pi_parse(data, len, &flags, proto) < 0) {
        return -1;
    }

    *payload = data + NFS_PI_HDR_SIZE;
    *payload_len = len - NFS_PI_HDR_SIZE;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  TAP wrap / unwrap                                                 */
/* ------------------------------------------------------------------ */

int nfs_tap_wrap_frame(const uint8_t *frame, size_t frame_len, uint8_t *out, size_t out_sz) {
    if (!frame || !out) {
        return -1;
    }

    size_t total = NFS_PI_HDR_SIZE + frame_len;
    if (out_sz < total) {
        return -1;
    }

    /* Build PI header with proto = ETH_P_IP (0x0800). */
    if (nfs_pi_build(out, out_sz, 0, 0x0800) < 0) {
        return -1;
    }

    memcpy(out + NFS_PI_HDR_SIZE, frame, frame_len);
    return (int)total;
}

int nfs_tap_unwrap(const uint8_t *data, size_t len, uint16_t *proto, const uint8_t **payload,
                   size_t *payload_len) {
    if (!data || !proto || !payload || !payload_len) {
        return -1;
    }
    if (len < NFS_PI_HDR_SIZE) {
        return -1;
    }

    uint16_t flags;
    if (nfs_pi_parse(data, len, &flags, proto) < 0) {
        return -1;
    }

    *payload = data + NFS_PI_HDR_SIZE;
    *payload_len = len - NFS_PI_HDR_SIZE;
    return 0;
}
