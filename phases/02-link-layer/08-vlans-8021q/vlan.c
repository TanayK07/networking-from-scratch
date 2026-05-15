#include "vlan.h"

#include <arpa/inet.h>  /* htons, ntohs */
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Field extraction                                                   */
/* ------------------------------------------------------------------ */

uint8_t nfs_vlan_pcp(const struct nfs_vlan_tag *t)
{
    uint16_t tci = ntohs(t->tci);
    return (uint8_t)((tci >> 13) & 0x07);
}

uint8_t nfs_vlan_dei(const struct nfs_vlan_tag *t)
{
    uint16_t tci = ntohs(t->tci);
    return (uint8_t)((tci >> 12) & 0x01);
}

uint16_t nfs_vlan_vid(const struct nfs_vlan_tag *t)
{
    uint16_t tci = ntohs(t->tci);
    return tci & 0x0FFF;
}

/* ------------------------------------------------------------------ */
/*  Set fields                                                         */
/* ------------------------------------------------------------------ */

void nfs_vlan_set(struct nfs_vlan_tag *t, uint8_t pcp, uint8_t dei,
                  uint16_t vid)
{
    t->tpid = htons(0x8100);
    uint16_t tci = (uint16_t)(((pcp & 0x07) << 13) |
                              ((dei & 0x01) << 12) |
                              (vid & 0x0FFF));
    t->tci = htons(tci);
}

/* ------------------------------------------------------------------ */
/*  Detection                                                          */
/* ------------------------------------------------------------------ */

int nfs_vlan_is_tagged(const uint8_t *frame, size_t len)
{
    if (!frame || len < 14)
        return 0;
    /* Bytes 12-13 hold the ethertype position; if 0x8100, it's tagged. */
    return (frame[12] == 0x81 && frame[13] == 0x00) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/*  Tag insertion                                                      */
/* ------------------------------------------------------------------ */

int nfs_vlan_tag_insert(const uint8_t *frame, size_t frame_len,
                        uint16_t vid, uint8_t pcp,
                        uint8_t *out, size_t out_sz)
{
    if (!frame || !out)
        return -1;
    if (frame_len < NFS_ETH_HDR_LEN)
        return -1;

    size_t new_len = frame_len + 4;
    if (out_sz < new_len)
        return -1;

    /* Copy dst + src (12 bytes). */
    memcpy(out, frame, 12);

    /* Insert 4-byte VLAN tag at offset 12. */
    struct nfs_vlan_tag tag;
    nfs_vlan_set(&tag, pcp, 0, vid);
    memcpy(out + 12, &tag, 4);

    /* Copy original ethertype + payload (everything from offset 12). */
    memcpy(out + 16, frame + 12, frame_len - 12);

    return (int)new_len;
}

/* ------------------------------------------------------------------ */
/*  Tag stripping                                                      */
/* ------------------------------------------------------------------ */

int nfs_vlan_tag_strip(const uint8_t *frame, size_t frame_len,
                       uint8_t *out, size_t out_sz,
                       uint16_t *vid_out)
{
    if (!frame || !out)
        return -1;
    /* Minimum: 12 (MACs) + 4 (VLAN tag) + 2 (ethertype) = 18 */
    if (frame_len < 18)
        return -1;
    if (!nfs_vlan_is_tagged(frame, frame_len))
        return -1;

    /* Extract VID before stripping. */
    if (vid_out) {
        struct nfs_vlan_tag tag;
        memcpy(&tag, frame + 12, 4);
        *vid_out = nfs_vlan_vid(&tag);
    }

    size_t new_len = frame_len - 4;
    if (out_sz < new_len)
        return -1;

    /* Copy dst + src (12 bytes). */
    memcpy(out, frame, 12);
    /* Copy everything after the VLAN tag (offset 16 onward). */
    memcpy(out + 12, frame + 16, frame_len - 16);

    return (int)new_len;
}

/* ------------------------------------------------------------------ */
/*  Format                                                             */
/* ------------------------------------------------------------------ */

void nfs_vlan_format(const struct nfs_vlan_tag *t, char *buf, size_t sz)
{
    if (!t || !buf || sz == 0) return;
    snprintf(buf, sz, "VID=%u PCP=%u DEI=%u",
             nfs_vlan_vid(t), nfs_vlan_pcp(t), nfs_vlan_dei(t));
}
