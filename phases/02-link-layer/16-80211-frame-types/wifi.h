#ifndef NFS_WIFI_H
#define NFS_WIFI_H

#include <stddef.h>
#include <stdint.h>

/*
 * IEEE 802.11 (Wi-Fi) frame format — frame control field and header parsing.
 *
 * Frame Control field (2 bytes, little-endian on the wire):
 *
 *   Bits  [1:0]   Protocol Version (always 0)
 *         [3:2]   Type: 00=Management, 01=Control, 10=Data
 *         [7:4]   Subtype (4 bits)
 *         [8]     To DS
 *         [9]     From DS
 *         [10]    More Fragments
 *         [11]    Retry
 *         [12]    Power Management
 *         [13]    More Data
 *         [14]    Protected Frame (WEP/WPA)
 *         [15]    +HTC/Order
 */

/* Frame types (2-bit field) */
#define NFS_WIFI_TYPE_MGMT 0 /* Management */
#define NFS_WIFI_TYPE_CTRL 1 /* Control */
#define NFS_WIFI_TYPE_DATA 2 /* Data */

/* Management subtypes (4-bit field) */
#define NFS_WIFI_MGMT_ASSOC_REQ    0
#define NFS_WIFI_MGMT_ASSOC_RESP   1
#define NFS_WIFI_MGMT_REASSOC_REQ  2
#define NFS_WIFI_MGMT_REASSOC_RESP 3
#define NFS_WIFI_MGMT_PROBE_REQ    4
#define NFS_WIFI_MGMT_PROBE_RESP   5
#define NFS_WIFI_MGMT_BEACON       8
#define NFS_WIFI_MGMT_ATIM         9
#define NFS_WIFI_MGMT_DISASSOC     10
#define NFS_WIFI_MGMT_AUTH         11
#define NFS_WIFI_MGMT_DEAUTH       12
#define NFS_WIFI_MGMT_ACTION       13

/* Control subtypes */
#define NFS_WIFI_CTRL_PS_POLL 10
#define NFS_WIFI_CTRL_RTS     11
#define NFS_WIFI_CTRL_CTS     12
#define NFS_WIFI_CTRL_ACK     13
#define NFS_WIFI_CTRL_CF_END  14

/* Data subtypes */
#define NFS_WIFI_DATA_DATA     0
#define NFS_WIFI_DATA_NULL     4
#define NFS_WIFI_DATA_QOS_DATA 8
#define NFS_WIFI_DATA_QOS_NULL 12

/* MAC address length */
#define NFS_WIFI_ADDR_LEN 6

/*
 * Parsed frame control field.
 */
struct nfs_wifi_fc {
    uint8_t version; /* 0-3, always 0 */
    uint8_t type;    /* 0=mgmt, 1=ctrl, 2=data */
    uint8_t subtype; /* 0-15 */
    int to_ds;
    int from_ds;
    int more_frag;
    int retry;
    int power_mgmt;
    int more_data;
    int protected_frame;
    int htc_order;
};

/*
 * 802.11 header (variable size depending on frame type).
 */
struct nfs_wifi_hdr {
    struct nfs_wifi_fc fc;
    uint16_t duration;
    uint8_t addr1[6];  /* Receiver / Destination */
    uint8_t addr2[6];  /* Transmitter / Source */
    uint8_t addr3[6];  /* BSSID or other (depends on To/From DS) */
    uint16_t seq_ctrl; /* Sequence control (frag# + seq#) */
    uint8_t addr4[6];  /* Only present in WDS (ToDS=1, FromDS=1) */
    int has_addr4;
};

/*
 * Pack a parsed frame control struct into a 16-bit value (little-endian wire
 * format).
 */
uint16_t nfs_wifi_fc_pack(const struct nfs_wifi_fc *fc);

/*
 * Unpack a 16-bit raw frame control value into a struct.
 * Returns 0 on success.
 */
int nfs_wifi_fc_unpack(uint16_t raw, struct nfs_wifi_fc *fc);

/*
 * Return the expected header size in bytes for a given frame control.
 */
int nfs_wifi_hdr_size(const struct nfs_wifi_fc *fc);

/*
 * Build a raw 802.11 header from a struct into `out`.
 * Returns bytes written or -1 on error.
 */
int nfs_wifi_hdr_build(const struct nfs_wifi_hdr *hdr, uint8_t *out, size_t out_sz);

/*
 * Parse raw bytes into an 802.11 header struct.
 * Returns 0 on success or -1 on error.
 */
int nfs_wifi_hdr_parse(const uint8_t *data, size_t len, struct nfs_wifi_hdr *hdr);

/*
 * Address interpretation helpers — set *da / *sa / *bssid to point into hdr.
 * Returns 0 on success, -1 if the address is not available for that frame type.
 */
int nfs_wifi_get_da(const struct nfs_wifi_hdr *hdr, const uint8_t **da);
int nfs_wifi_get_sa(const struct nfs_wifi_hdr *hdr, const uint8_t **sa);
int nfs_wifi_get_bssid(const struct nfs_wifi_hdr *hdr, const uint8_t **bssid);

/*
 * Sequence control helpers.
 */
uint16_t nfs_wifi_seq_num(uint16_t seq_ctrl);
uint8_t nfs_wifi_frag_num(uint16_t seq_ctrl);
uint16_t nfs_wifi_seq_ctrl(uint16_t seq_num, uint8_t frag_num);

/*
 * Human-readable frame type / subtype names.
 */
const char *nfs_wifi_type_name(uint8_t type);
const char *nfs_wifi_subtype_name(uint8_t type, uint8_t subtype);

#endif /* NFS_WIFI_H */
