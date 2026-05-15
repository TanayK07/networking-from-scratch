#include "mac.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Classification                                                     */
/* ------------------------------------------------------------------ */

int nfs_mac_is_broadcast(const uint8_t mac[6])
{
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0xFF)
            return 0;
    }
    return 1;
}

int nfs_mac_is_multicast(const uint8_t mac[6])
{
    return (mac[0] & 0x01) != 0;
}

int nfs_mac_is_unicast(const uint8_t mac[6])
{
    return (mac[0] & 0x01) == 0;
}

int nfs_mac_is_local(const uint8_t mac[6])
{
    return (mac[0] & 0x02) != 0;
}

int nfs_mac_is_global(const uint8_t mac[6])
{
    return (mac[0] & 0x02) == 0;
}

/* ------------------------------------------------------------------ */
/*  OUI                                                                */
/* ------------------------------------------------------------------ */

void nfs_mac_get_oui(const uint8_t mac[6], uint8_t oui[3])
{
    oui[0] = mac[0];
    oui[1] = mac[1];
    oui[2] = mac[2];
}

/* ------------------------------------------------------------------ */
/*  Formatting / parsing                                               */
/* ------------------------------------------------------------------ */

int nfs_mac_format(const uint8_t mac[6], char *buf, size_t sz)
{
    if (!mac || !buf || sz < 18)
        return -1;
    snprintf(buf, sz, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}

int nfs_mac_parse(const char *str, uint8_t mac[6])
{
    if (!str || !mac)
        return -1;

    unsigned int b[6];

    /* Try colon-separated first. */
    if (sscanf(str, "%x:%x:%x:%x:%x:%x",
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6)
        goto done;

    /* Try dash-separated. */
    if (sscanf(str, "%x-%x-%x-%x-%x-%x",
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6)
        goto done;

    return -1;

done:
    for (int i = 0; i < 6; i++) {
        if (b[i] > 0xFF)
            return -1;
        mac[i] = (uint8_t)b[i];
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Type string                                                        */
/* ------------------------------------------------------------------ */

const char *nfs_mac_type_str(const uint8_t mac[6])
{
    if (nfs_mac_is_broadcast(mac))
        return "broadcast";
    if (nfs_mac_is_multicast(mac))
        return "multicast";
    if (nfs_mac_is_local(mac))
        return "unicast/local";
    return "unicast/global";
}
