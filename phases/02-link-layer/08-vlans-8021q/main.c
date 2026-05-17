/*
 * vlan_parse — demonstrate 802.1Q VLAN tag insertion and stripping.
 *
 * Build:  make
 * Run:    ./vlan_parse
 */
#include "vlan.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== 802.1Q VLAN Tag Demo ===\n\n");

    /* Build an untagged frame. */
    uint8_t frame[60];
    memset(frame, 0, sizeof(frame));
    memset(frame, 0xFF, 6); /* dst broadcast */
    uint8_t src[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    memcpy(frame + 6, src, 6);
    frame[12] = 0x08;
    frame[13] = 0x00; /* IPv4 */

    printf("Original frame: %zu bytes, tagged=%d\n", sizeof(frame),
           nfs_vlan_is_tagged(frame, sizeof(frame)));

    /* Insert VLAN tag: VID=100, PCP=3. */
    uint8_t tagged[64];
    int tlen = nfs_vlan_tag_insert(frame, sizeof(frame), 100, 3, tagged, sizeof(tagged));
    if (tlen > 0) {
        printf("Tagged frame:   %d bytes, tagged=%d\n", tlen,
               nfs_vlan_is_tagged(tagged, (size_t)tlen));

        struct nfs_vlan_tag tag;
        memcpy(&tag, tagged + 12, 4);
        char fmt[64];
        nfs_vlan_format(&tag, fmt, sizeof(fmt));
        printf("VLAN tag:       %s\n", fmt);
    }

    /* Strip the VLAN tag. */
    uint8_t stripped[64];
    uint16_t vid;
    int slen = nfs_vlan_tag_strip(tagged, (size_t)tlen, stripped, sizeof(stripped), &vid);
    if (slen > 0) {
        printf("Stripped frame: %d bytes, tagged=%d, VID=%u\n", slen,
               nfs_vlan_is_tagged(stripped, (size_t)slen), vid);
    }

    return 0;
}
