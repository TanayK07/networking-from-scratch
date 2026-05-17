#include "wire.h"
#include <string.h>

/* ---------------------------------------------------------------
 * Preamble & SFD (IEEE 802.3)
 * --------------------------------------------------------------- */

int nfs_preamble_generate(uint8_t *out, size_t out_sz)
{
    if (!out || out_sz < NFS_PREAMBLE_TOTAL)
        return 0;

    memset(out, NFS_PREAMBLE_BYTE, NFS_PREAMBLE_LEN);
    out[NFS_PREAMBLE_LEN] = NFS_SFD_BYTE;
    return NFS_PREAMBLE_TOTAL;
}

int nfs_preamble_detect(const uint8_t *data, size_t len)
{
    if (!data || len < NFS_PREAMBLE_TOTAL)
        return -1;

    /* Search for 7 consecutive 0xAA followed by 0xAB */
    for (size_t i = 0; i <= len - NFS_PREAMBLE_TOTAL; i++) {
        int found = 1;
        for (int j = 0; j < NFS_PREAMBLE_LEN; j++) {
            if (data[i + j] != NFS_PREAMBLE_BYTE) {
                found = 0;
                break;
            }
        }
        if (found && data[i + NFS_PREAMBLE_LEN] == NFS_SFD_BYTE) {
            return (int)(i + NFS_PREAMBLE_LEN); /* offset of SFD byte */
        }
    }
    return -1;
}

/* ---------------------------------------------------------------
 * Interframe Gap (IFG)
 * --------------------------------------------------------------- */

int nfs_ifg_bytes(int speed_mbps)
{
    (void)speed_mbps;
    return NFS_IFG_BYTES;
}

int nfs_ifg_ns(int speed_mbps)
{
    if (speed_mbps <= 0)
        return 0;

    /* IFG = 12 bytes = 96 bits.
     * Bit time = 1000 / speed_mbps ns.
     * IFG time = 96 * 1000 / speed_mbps ns.
     *
     * 10 Mbps:    96 * 100  = 9600 ns
     * 100 Mbps:   96 * 10   = 960 ns
     * 1000 Mbps:  96 * 1    = 96 ns
     * 10000 Mbps: 96 * 0.1  = 9.6 → 10 ns (integer rounding)
     */
    long long bits = (long long)NFS_IFG_BYTES * 8;
    long long numerator = bits * 1000;
    /* Round to nearest integer: (num + denom/2) / denom */
    return (int)((numerator + speed_mbps / 2) / speed_mbps);
}

/* ---------------------------------------------------------------
 * 8b/10b encoding concepts (IEEE 802.3z)
 * --------------------------------------------------------------- */

/* Count number of 1-bits in the lower 10 bits of a value. */
static int count_ones_10(uint16_t val)
{
    int count = 0;
    for (int i = 0; i < 10; i++) {
        if (val & (1 << i))
            count++;
    }
    return count;
}

int nfs_rd_update(int current_rd, uint16_t symbol_10bit)
{
    int ones = count_ones_10(symbol_10bit);
    if (ones > 5)
        return 1;
    if (ones < 5)
        return -1;
    return current_rd;
}

int nfs_8b10b_dc_balanced(uint16_t symbol)
{
    return count_ones_10(symbol) == 5 ? 1 : 0;
}

int nfs_is_comma(uint16_t symbol)
{
    uint16_t masked = symbol & 0x3FF; /* keep only lower 10 bits */
    return (masked == NFS_K28_5_RDN || masked == NFS_K28_5_RDP) ? 1 : 0;
}

/* ---------------------------------------------------------------
 * Frame overhead and efficiency
 * --------------------------------------------------------------- */

int nfs_wire_frame_overhead(void)
{
    /* preamble(7) + SFD(1) + header(14) + FCS(4) + IFG(12) = 38 */
    return NFS_PREAMBLE_LEN + NFS_SFD_LEN + NFS_ETH_HEADER
         + NFS_FCS_SIZE + NFS_IFG_BYTES;
}

double nfs_wire_efficiency(size_t payload_size)
{
    double total = (double)payload_size + (double)nfs_wire_frame_overhead();
    if (total <= 0.0)
        return 0.0;
    return (double)payload_size / total;
}

int nfs_min_frame_size(void)
{
    /* Minimum Ethernet frame (L2, without preamble/SFD/IFG):
     * header(14) + min_payload(46) + FCS(4) = 64 bytes */
    return NFS_ETH_HEADER + NFS_MIN_PAYLOAD + NFS_FCS_SIZE;
}

int nfs_max_frame_size(void)
{
    /* Maximum standard Ethernet frame (L2):
     * header(14) + max_payload(1500) + FCS(4) = 1518 bytes */
    return NFS_ETH_HEADER + NFS_MAX_PAYLOAD + NFS_FCS_SIZE;
}

/* ---------------------------------------------------------------
 * Bit time and frame timing
 * --------------------------------------------------------------- */

int nfs_bit_time_ns(int speed_mbps)
{
    if (speed_mbps <= 0)
        return 0;
    /* 1 bit at N Mbps = 1000/N ns.
     * 10 Mbps: 100, 100 Mbps: 10, 1000 Mbps: 1, 10000 Mbps: 0 */
    return 1000 / speed_mbps;
}

long long nfs_frame_time_ns(size_t frame_bytes, int speed_mbps)
{
    int bt = nfs_bit_time_ns(speed_mbps);
    /* total bytes on wire = frame + preamble(8) + IFG(12) */
    long long total_bits = (long long)(frame_bytes + NFS_PREAMBLE_TOTAL
                                       + NFS_IFG_BYTES) * 8;
    return total_bits * bt;
}
