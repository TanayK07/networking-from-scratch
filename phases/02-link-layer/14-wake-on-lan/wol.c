#include "wol.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

int nfs_wol_build(const uint8_t mac[6], uint8_t *out, size_t out_sz)
{
    if (!mac || !out || out_sz < NFS_WOL_MAGIC_SIZE)
        return -1;

    /* 6 bytes of 0xFF */
    memset(out, 0xFF, NFS_WOL_SYNC_LEN);

    /* Target MAC repeated 16 times */
    for (int i = 0; i < NFS_WOL_MAC_REPEATS; i++)
        memcpy(out + NFS_WOL_SYNC_LEN + i * 6, mac, 6);

    return NFS_WOL_MAGIC_SIZE;
}

int nfs_wol_build_with_password(const uint8_t mac[6],
                                const uint8_t *password, size_t pw_len,
                                uint8_t *out, size_t out_sz)
{
    if (pw_len != 0 && pw_len != 4 && pw_len != 6)
        return -1;

    size_t total = NFS_WOL_MAGIC_SIZE + pw_len;
    if (!out || out_sz < total)
        return -1;

    int base = nfs_wol_build(mac, out, out_sz);
    if (base < 0)
        return -1;

    if (pw_len > 0) {
        if (!password)
            return -1;
        memcpy(out + NFS_WOL_MAGIC_SIZE, password, pw_len);
    }

    return (int)total;
}

int nfs_wol_validate(const uint8_t *pkt, size_t len, uint8_t mac_out[6])
{
    if (!pkt || !mac_out || len < NFS_WOL_MAGIC_SIZE)
        return -1;

    /* Check sync bytes */
    for (int i = 0; i < NFS_WOL_SYNC_LEN; i++) {
        if (pkt[i] != 0xFF)
            return -1;
    }

    /* Extract MAC from first copy */
    const uint8_t *first_mac = pkt + NFS_WOL_SYNC_LEN;

    /* Verify all 16 copies are identical */
    for (int i = 1; i < NFS_WOL_MAC_REPEATS; i++) {
        if (memcmp(first_mac, pkt + NFS_WOL_SYNC_LEN + i * 6, 6) != 0)
            return -1;
    }

    memcpy(mac_out, first_mac, 6);
    return 0;
}

/* Parse a single hex digit, return value 0-15 or -1. */
static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int nfs_wol_mac_parse(const char *str, uint8_t mac[6])
{
    if (!str || !mac)
        return -1;

    /* Expect format: XX:XX:XX:XX:XX:XX or XX-XX-XX-XX-XX-XX */
    for (int i = 0; i < 6; i++) {
        int hi = hex_val(str[0]);
        int lo = hex_val(str[1]);
        if (hi < 0 || lo < 0)
            return -1;

        mac[i] = (uint8_t)((hi << 4) | lo);
        str += 2;

        if (i < 5) {
            if (*str != ':' && *str != '-')
                return -1;
            str++;
        }
    }

    /* Should be at end of string */
    if (*str != '\0')
        return -1;

    return 0;
}

int nfs_wol_mac_format(const uint8_t mac[6], char *buf, size_t sz)
{
    if (!mac || !buf || sz < 18)
        return -1;

    snprintf(buf, sz, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}
