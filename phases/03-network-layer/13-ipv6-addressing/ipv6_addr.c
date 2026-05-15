#include "ipv6_addr.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ---------------------------------------------------------------
 * Address classification
 * --------------------------------------------------------------- */

static int is_all_zero(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (data[i] != 0) return 0;
    }
    return 1;
}

int nfs_ipv6_addr_is_loopback(const uint8_t addr[16])
{
    /* ::1 = 15 zero bytes followed by 0x01 */
    return is_all_zero(addr, 15) && addr[15] == 0x01;
}

int nfs_ipv6_addr_is_unspecified(const uint8_t addr[16])
{
    /* :: = all 16 bytes zero */
    return is_all_zero(addr, 16);
}

int nfs_ipv6_addr_is_link_local(const uint8_t addr[16])
{
    /* fe80::/10 -> first 10 bits are 1111 1110 10 */
    return addr[0] == 0xFE && (addr[1] & 0xC0) == 0x80;
}

int nfs_ipv6_addr_is_multicast(const uint8_t addr[16])
{
    /* ff00::/8 -> first byte is 0xFF */
    return addr[0] == 0xFF;
}

int nfs_ipv6_addr_is_global_unicast(const uint8_t addr[16])
{
    /* 2000::/3 -> first 3 bits are 001 -> first byte & 0xE0 == 0x20 */
    return (addr[0] & 0xE0) == 0x20;
}

int nfs_ipv6_addr_is_ula(const uint8_t addr[16])
{
    /* fc00::/7 -> first 7 bits are 1111 110 -> first byte & 0xFE == 0xFC */
    return (addr[0] & 0xFE) == 0xFC;
}

const char *nfs_ipv6_addr_type_str(const uint8_t addr[16])
{
    if (nfs_ipv6_addr_is_loopback(addr))        return "loopback";
    if (nfs_ipv6_addr_is_unspecified(addr))      return "unspecified";
    if (nfs_ipv6_addr_is_multicast(addr))        return "multicast";
    if (nfs_ipv6_addr_is_link_local(addr))       return "link-local";
    if (nfs_ipv6_addr_is_ula(addr))              return "unique local";
    if (nfs_ipv6_addr_is_global_unicast(addr))   return "global unicast";
    return "other";
}

/* ---------------------------------------------------------------
 * EUI-64 construction
 * --------------------------------------------------------------- */

void nfs_ipv6_addr_from_eui64(const uint8_t mac[6], const uint8_t prefix[8],
                               uint8_t addr[16])
{
    /* Copy 64-bit prefix */
    memcpy(addr, prefix, 8);

    /* Build Modified EUI-64 interface ID:
     *   mac[0..2] + FF:FE + mac[3..5]
     *   then flip bit 6 (universal/local) of the first byte */
    addr[8]  = mac[0] ^ 0x02;  /* Flip the U/L bit */
    addr[9]  = mac[1];
    addr[10] = mac[2];
    addr[11] = 0xFF;
    addr[12] = 0xFE;
    addr[13] = mac[3];
    addr[14] = mac[4];
    addr[15] = mac[5];
}

/* ---------------------------------------------------------------
 * Solicited-node multicast
 * --------------------------------------------------------------- */

int nfs_ipv6_addr_solicited_node(const uint8_t addr[16], uint8_t out[16])
{
    if (!addr || !out)
        return -1;

    /* ff02::1:ffXX:XXXX where XX:XXXX = last 24 bits of addr */
    memset(out, 0, 16);
    out[0]  = 0xFF;
    out[1]  = 0x02;
    /* bytes 2..10 are zero */
    out[11] = 0x01;
    out[12] = 0xFF;
    out[13] = addr[13];
    out[14] = addr[14];
    out[15] = addr[15];

    return 0;
}

/* ---------------------------------------------------------------
 * Formatting and parsing
 * --------------------------------------------------------------- */

int nfs_ipv6_addr_format(const uint8_t addr[16], char *buf, size_t sz)
{
    if (!addr || !buf || sz < 40)
        return -1;

    snprintf(buf, sz,
             "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
             "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
             addr[0],  addr[1],  addr[2],  addr[3],
             addr[4],  addr[5],  addr[6],  addr[7],
             addr[8],  addr[9],  addr[10], addr[11],
             addr[12], addr[13], addr[14], addr[15]);
    return 0;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int nfs_ipv6_addr_parse(const char *str, uint8_t addr[16])
{
    if (!str || !addr)
        return -1;

    /* Parse exactly 8 groups of 1-4 hex digits separated by ':' */
    const char *p = str;
    int group = 0;

    while (group < 8) {
        /* Parse hex digits for this group (1-4 digits) */
        unsigned int val = 0;
        int digits = 0;

        while (*p && *p != ':') {
            int d = hex_digit(*p);
            if (d < 0)
                return -1;
            val = (val << 4) | (unsigned int)d;
            digits++;
            if (digits > 4)
                return -1;
            p++;
        }

        if (digits == 0)
            return -1;
        if (val > 0xFFFF)
            return -1;

        addr[group * 2]     = (uint8_t)(val >> 8);
        addr[group * 2 + 1] = (uint8_t)(val & 0xFF);
        group++;

        if (group < 8) {
            if (*p != ':')
                return -1;
            p++;
        }
    }

    /* Should be at end of string */
    if (*p != '\0')
        return -1;

    return 0;
}
