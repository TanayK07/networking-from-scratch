/*
 * slaac.c -- SLAAC (Stateless Address Autoconfiguration) implementation
 */
#include "slaac.h"
#include <stdio.h>
#include <string.h>

int nfs_slaac_eui64(const uint8_t *mac, uint8_t *eui64) {
    if (!mac || !eui64)
        return -1;

    /* OUI (3 bytes) + 0xFF 0xFE + NIC (3 bytes) */
    eui64[0] = mac[0] ^ 0x02; /* flip U/L bit */
    eui64[1] = mac[1];
    eui64[2] = mac[2];
    eui64[3] = 0xFF;
    eui64[4] = 0xFE;
    eui64[5] = mac[3];
    eui64[6] = mac[4];
    eui64[7] = mac[5];

    return 0;
}

int nfs_slaac_generate_addr(const uint8_t *prefix, uint8_t prefix_len, const uint8_t *mac,
                            uint8_t *addr_out) {
    if (!prefix || !mac || !addr_out)
        return -1;
    if (prefix_len > 128)
        return -1;

    /* Generate EUI-64 interface ID */
    uint8_t eui64[8];
    if (nfs_slaac_eui64(mac, eui64) < 0)
        return -1;

    /* Start with the prefix */
    memcpy(addr_out, prefix, 16);

    /* Zero out the interface identifier portion (bits after prefix_len) */
    /* For standard SLAAC, prefix_len is 64 */
    if (prefix_len <= 64) {
        /* Zero bytes 8-15, then set EUI-64 */
        memset(addr_out + 8, 0, 8);
        memcpy(addr_out + 8, eui64, 8);

        /* Also zero out any bits in the prefix area beyond prefix_len */
        if (prefix_len < 64) {
            size_t full_bytes = prefix_len / 8;
            uint8_t remaining_bits = prefix_len % 8;

            if (remaining_bits > 0) {
                uint8_t mask = (uint8_t)(0xFF << (8 - remaining_bits));
                addr_out[full_bytes] &= mask;
                full_bytes++;
            }
            /* Zero from end of prefix bits to byte 8 */
            if (full_bytes < 8) {
                memset(addr_out + full_bytes, 0, 8 - full_bytes);
            }
        }
    } else {
        /* Prefix is longer than 64 bits -- unusual but handle it.
         * We can only set the bits after prefix_len. */
        size_t full_bytes = prefix_len / 8;
        uint8_t remaining_bits = prefix_len % 8;

        /* Copy as many EUI-64 bytes as possible into the host portion */
        size_t iid_start = 8;
        for (size_t i = iid_start; i < 16; i++) {
            if (i < full_bytes) {
                /* This byte is fully in the prefix -- keep it */
                continue;
            } else if (i == full_bytes && remaining_bits > 0) {
                /* Partial byte: keep prefix bits, set host bits from EUI-64 */
                uint8_t mask = (uint8_t)(0xFF << (8 - remaining_bits));
                addr_out[i] = (addr_out[i] & mask) | (eui64[i - 8] & ~mask);
            } else {
                addr_out[i] = eui64[i - 8];
            }
        }
    }

    return 0;
}

int nfs_slaac_validate_prefix(const uint8_t *prefix, uint8_t prefix_len, int reject_linklocal) {
    if (!prefix)
        return 0;

    /* Must be /64 for SLAAC */
    if (prefix_len != 64)
        return 0;

    /* Must not be multicast (ff00::/8) */
    if (prefix[0] == 0xFF)
        return 0;

    /* Optionally reject link-local (fe80::/10) */
    if (reject_linklocal) {
        if (prefix[0] == 0xFE && (prefix[1] & 0xC0) == 0x80)
            return 0;
    }

    /* Must not be loopback (::1) */
    uint8_t loopback[16] = {0};
    loopback[15] = 1;
    if (memcmp(prefix, loopback, 16) == 0)
        return 0;

    /* Must not be all zeros (::) */
    uint8_t zeros[16] = {0};
    if (memcmp(prefix, zeros, 16) == 0)
        return 0;

    return 1;
}

int nfs_slaac_linklocal(const uint8_t *mac, uint8_t *addr_out) {
    if (!mac || !addr_out)
        return -1;

    /* fe80::/64 prefix */
    memset(addr_out, 0, 16);
    addr_out[0] = 0xFE;
    addr_out[1] = 0x80;

    /* EUI-64 interface ID in bytes 8-15 */
    return nfs_slaac_eui64(mac, addr_out + 8);
}

int nfs_slaac_solicited_node(const uint8_t *addr, uint8_t *mcast_out) {
    if (!addr || !mcast_out)
        return -1;

    /* ff02::1:ffXX:XXXX */
    memset(mcast_out, 0, 16);
    mcast_out[0] = 0xFF;
    mcast_out[1] = 0x02;
    /* bytes 2-10 are zero */
    mcast_out[11] = 0x01;
    mcast_out[12] = 0xFF;
    mcast_out[13] = addr[13];
    mcast_out[14] = addr[14];
    mcast_out[15] = addr[15];

    return 0;
}

char *nfs_slaac_format_ipv6(const uint8_t *addr, char *out, size_t out_sz) {
    if (!addr || !out || out_sz < 40) {
        if (out && out_sz > 0)
            out[0] = '\0';
        return out;
    }

    snprintf(out, out_sz,
             "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
             "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7], addr[8],
             addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15]);
    return out;
}
