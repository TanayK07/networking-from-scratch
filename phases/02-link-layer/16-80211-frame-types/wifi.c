#include "wifi.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/*  Frame Control pack / unpack                                        */
/* ------------------------------------------------------------------ */

uint16_t nfs_wifi_fc_pack(const struct nfs_wifi_fc *fc) {
    if (!fc)
        return 0;

    uint16_t raw = 0;
    raw |= (uint16_t)(fc->version & 0x03);
    raw |= (uint16_t)((fc->type & 0x03) << 2);
    raw |= (uint16_t)((fc->subtype & 0x0F) << 4);
    raw |= (uint16_t)((fc->to_ds ? 1 : 0) << 8);
    raw |= (uint16_t)((fc->from_ds ? 1 : 0) << 9);
    raw |= (uint16_t)((fc->more_frag ? 1 : 0) << 10);
    raw |= (uint16_t)((fc->retry ? 1 : 0) << 11);
    raw |= (uint16_t)((fc->power_mgmt ? 1 : 0) << 12);
    raw |= (uint16_t)((fc->more_data ? 1 : 0) << 13);
    raw |= (uint16_t)((fc->protected_frame ? 1 : 0) << 14);
    raw |= (uint16_t)((fc->htc_order ? 1 : 0) << 15);

    return raw;
}

int nfs_wifi_fc_unpack(uint16_t raw, struct nfs_wifi_fc *fc) {
    if (!fc)
        return -1;

    fc->version = (uint8_t)(raw & 0x03);
    fc->type = (uint8_t)((raw >> 2) & 0x03);
    fc->subtype = (uint8_t)((raw >> 4) & 0x0F);
    fc->to_ds = (raw >> 8) & 1;
    fc->from_ds = (raw >> 9) & 1;
    fc->more_frag = (raw >> 10) & 1;
    fc->retry = (raw >> 11) & 1;
    fc->power_mgmt = (raw >> 12) & 1;
    fc->more_data = (raw >> 13) & 1;
    fc->protected_frame = (raw >> 14) & 1;
    fc->htc_order = (raw >> 15) & 1;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Header size                                                        */
/* ------------------------------------------------------------------ */

int nfs_wifi_hdr_size(const struct nfs_wifi_fc *fc) {
    if (!fc)
        return -1;

    if (fc->type == NFS_WIFI_TYPE_CTRL) {
        /* ACK and CTS: FC(2) + Duration(2) + addr1(6) = 10 */
        if (fc->subtype == NFS_WIFI_CTRL_ACK || fc->subtype == NFS_WIFI_CTRL_CTS)
            return 10;
        /* RTS: FC(2) + Duration(2) + addr1(6) + addr2(6) = 16 */
        if (fc->subtype == NFS_WIFI_CTRL_RTS)
            return 16;
        /* PS-Poll, CF-End: FC(2) + Duration/AID(2) + addr1(6) + addr2(6) = 16 */
        return 16;
    }

    /* Management and Data: FC(2) + Duration(2) + 3*addr(18) + SeqCtrl(2) = 24 */
    if (fc->type == NFS_WIFI_TYPE_DATA && fc->to_ds && fc->from_ds) {
        /* WDS: add addr4(6) = 30 */
        return 30;
    }

    return 24;
}

/* ------------------------------------------------------------------ */
/*  Header build                                                       */
/* ------------------------------------------------------------------ */

int nfs_wifi_hdr_build(const struct nfs_wifi_hdr *hdr, uint8_t *out, size_t out_sz) {
    if (!hdr || !out)
        return -1;

    int size = nfs_wifi_hdr_size(&hdr->fc);
    if (size < 0 || out_sz < (size_t)size)
        return -1;

    memset(out, 0, (size_t)size);

    /* Frame control — 2 bytes little-endian */
    uint16_t fc_raw = nfs_wifi_fc_pack(&hdr->fc);
    out[0] = (uint8_t)(fc_raw & 0xFF);
    out[1] = (uint8_t)((fc_raw >> 8) & 0xFF);

    /* Duration — 2 bytes little-endian */
    out[2] = (uint8_t)(hdr->duration & 0xFF);
    out[3] = (uint8_t)((hdr->duration >> 8) & 0xFF);

    /* addr1 always present */
    memcpy(out + 4, hdr->addr1, 6);

    if (size <= 10)
        return size;

    /* addr2 */
    memcpy(out + 10, hdr->addr2, 6);

    if (size <= 16)
        return size;

    /* addr3 + seq_ctrl */
    memcpy(out + 16, hdr->addr3, 6);
    out[22] = (uint8_t)(hdr->seq_ctrl & 0xFF);
    out[23] = (uint8_t)((hdr->seq_ctrl >> 8) & 0xFF);

    if (size <= 24)
        return size;

    /* addr4 (WDS) */
    memcpy(out + 24, hdr->addr4, 6);

    return size;
}

/* ------------------------------------------------------------------ */
/*  Header parse                                                       */
/* ------------------------------------------------------------------ */

int nfs_wifi_hdr_parse(const uint8_t *data, size_t len, struct nfs_wifi_hdr *hdr) {
    if (!data || !hdr)
        return -1;

    /* Need at least 10 bytes for the smallest control frame */
    if (len < 10)
        return -1;

    memset(hdr, 0, sizeof(*hdr));

    /* Frame control — 2 bytes little-endian */
    uint16_t fc_raw = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    if (nfs_wifi_fc_unpack(fc_raw, &hdr->fc) != 0)
        return -1;

    int expected = nfs_wifi_hdr_size(&hdr->fc);
    if (expected < 0 || len < (size_t)expected)
        return -1;

    /* Duration */
    hdr->duration = (uint16_t)data[2] | ((uint16_t)data[3] << 8);

    /* addr1 */
    memcpy(hdr->addr1, data + 4, 6);

    if (expected <= 10)
        return 0;

    /* addr2 */
    memcpy(hdr->addr2, data + 10, 6);

    if (expected <= 16)
        return 0;

    /* addr3 + seq_ctrl */
    memcpy(hdr->addr3, data + 16, 6);
    hdr->seq_ctrl = (uint16_t)data[22] | ((uint16_t)data[23] << 8);

    if (expected <= 24) {
        hdr->has_addr4 = 0;
        return 0;
    }

    /* addr4 (WDS) */
    memcpy(hdr->addr4, data + 24, 6);
    hdr->has_addr4 = 1;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Address interpretation                                             */
/* ------------------------------------------------------------------ */

int nfs_wifi_get_da(const struct nfs_wifi_hdr *hdr, const uint8_t **da) {
    if (!hdr || !da)
        return -1;

    if (!hdr->fc.to_ds && !hdr->fc.from_ds) {
        /* IBSS: DA = addr1 */
        *da = hdr->addr1;
    } else if (hdr->fc.to_ds && !hdr->fc.from_ds) {
        /* To AP: DA = addr3 */
        *da = hdr->addr3;
    } else if (!hdr->fc.to_ds && hdr->fc.from_ds) {
        /* From AP: DA = addr1 */
        *da = hdr->addr1;
    } else {
        /* WDS: DA = addr3 */
        *da = hdr->addr3;
    }
    return 0;
}

int nfs_wifi_get_sa(const struct nfs_wifi_hdr *hdr, const uint8_t **sa) {
    if (!hdr || !sa)
        return -1;

    if (!hdr->fc.to_ds && !hdr->fc.from_ds) {
        /* IBSS: SA = addr2 */
        *sa = hdr->addr2;
    } else if (hdr->fc.to_ds && !hdr->fc.from_ds) {
        /* To AP: SA = addr2 */
        *sa = hdr->addr2;
    } else if (!hdr->fc.to_ds && hdr->fc.from_ds) {
        /* From AP: SA = addr3 */
        *sa = hdr->addr3;
    } else {
        /* WDS: SA = addr4 */
        *sa = hdr->addr4;
    }
    return 0;
}

int nfs_wifi_get_bssid(const struct nfs_wifi_hdr *hdr, const uint8_t **bssid) {
    if (!hdr || !bssid)
        return -1;

    if (!hdr->fc.to_ds && !hdr->fc.from_ds) {
        /* IBSS: BSSID = addr3 */
        *bssid = hdr->addr3;
    } else if (hdr->fc.to_ds && !hdr->fc.from_ds) {
        /* To AP: BSSID = addr1 */
        *bssid = hdr->addr1;
    } else if (!hdr->fc.to_ds && hdr->fc.from_ds) {
        /* From AP: BSSID = addr2 */
        *bssid = hdr->addr2;
    } else {
        /* WDS: no single BSSID concept; return -1 */
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Sequence control                                                   */
/* ------------------------------------------------------------------ */

uint16_t nfs_wifi_seq_num(uint16_t seq_ctrl) {
    return (seq_ctrl >> 4) & 0x0FFF;
}

uint8_t nfs_wifi_frag_num(uint16_t seq_ctrl) {
    return (uint8_t)(seq_ctrl & 0x0F);
}

uint16_t nfs_wifi_seq_ctrl(uint16_t seq_num, uint8_t frag_num) {
    return (uint16_t)((seq_num & 0x0FFF) << 4) | (uint16_t)(frag_num & 0x0F);
}

/* ------------------------------------------------------------------ */
/*  Type / subtype names                                               */
/* ------------------------------------------------------------------ */

const char *nfs_wifi_type_name(uint8_t type) {
    switch (type) {
    case NFS_WIFI_TYPE_MGMT:
        return "Management";
    case NFS_WIFI_TYPE_CTRL:
        return "Control";
    case NFS_WIFI_TYPE_DATA:
        return "Data";
    default:
        return "Unknown";
    }
}

const char *nfs_wifi_subtype_name(uint8_t type, uint8_t subtype) {
    if (type == NFS_WIFI_TYPE_MGMT) {
        switch (subtype) {
        case NFS_WIFI_MGMT_ASSOC_REQ:
            return "Association Request";
        case NFS_WIFI_MGMT_ASSOC_RESP:
            return "Association Response";
        case NFS_WIFI_MGMT_REASSOC_REQ:
            return "Reassociation Request";
        case NFS_WIFI_MGMT_REASSOC_RESP:
            return "Reassociation Response";
        case NFS_WIFI_MGMT_PROBE_REQ:
            return "Probe Request";
        case NFS_WIFI_MGMT_PROBE_RESP:
            return "Probe Response";
        case NFS_WIFI_MGMT_BEACON:
            return "Beacon";
        case NFS_WIFI_MGMT_ATIM:
            return "ATIM";
        case NFS_WIFI_MGMT_DISASSOC:
            return "Disassociation";
        case NFS_WIFI_MGMT_AUTH:
            return "Authentication";
        case NFS_WIFI_MGMT_DEAUTH:
            return "Deauthentication";
        case NFS_WIFI_MGMT_ACTION:
            return "Action";
        default:
            return "Unknown Management";
        }
    }

    if (type == NFS_WIFI_TYPE_CTRL) {
        switch (subtype) {
        case NFS_WIFI_CTRL_PS_POLL:
            return "PS-Poll";
        case NFS_WIFI_CTRL_RTS:
            return "RTS";
        case NFS_WIFI_CTRL_CTS:
            return "CTS";
        case NFS_WIFI_CTRL_ACK:
            return "ACK";
        case NFS_WIFI_CTRL_CF_END:
            return "CF-End";
        default:
            return "Unknown Control";
        }
    }

    if (type == NFS_WIFI_TYPE_DATA) {
        switch (subtype) {
        case NFS_WIFI_DATA_DATA:
            return "Data";
        case NFS_WIFI_DATA_NULL:
            return "Null";
        case NFS_WIFI_DATA_QOS_DATA:
            return "QoS Data";
        case NFS_WIFI_DATA_QOS_NULL:
            return "QoS Null";
        default:
            return "Unknown Data";
        }
    }

    return "Unknown";
}
