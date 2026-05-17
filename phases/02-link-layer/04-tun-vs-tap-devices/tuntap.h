#ifndef NFS_TUNTAP_H
#define NFS_TUNTAP_H

#include <linux/if.h>
#include <linux/if_tun.h>
#include <stddef.h>
#include <stdint.h>

/* ---- Device modes ---- */

#define NFS_TUN_MODE 0 /* Layer 3: IP packets   */
#define NFS_TAP_MODE 1 /* Layer 2: Ethernet frames */

/* ---- MTU constants ---- */

#define NFS_TUN_DEFAULT_MTU 1500
#define NFS_TAP_DEFAULT_MTU 1500
#define NFS_TUN_OVERHEAD    0  /* TUN has no L2 overhead          */
#define NFS_TAP_OVERHEAD    14 /* Ethernet header: 6+6+2 = 14     */

/* ---- Packet Info (PI) header ----
 *
 * When IFF_NO_PI is NOT set, the kernel prepends/expects a 4-byte header
 * on every read/write to the TUN/TAP fd.
 */

struct nfs_pi_header {
    uint16_t flags; /* usually 0                              */
    uint16_t proto; /* EtherType in network byte order:       */
                    /*   ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD */
};

#define NFS_PI_HDR_SIZE 4

/* ---- Functions ---- */

/*
 * nfs_tuntap_prepare_ifr -- build an ifreq struct suitable for TUNSETIFF.
 *
 * @ifr:      output ifreq (zeroed and populated).
 * @dev_name: interface name, e.g. "tun0". Empty string "" lets kernel assign.
 * @mode:     NFS_TUN_MODE or NFS_TAP_MODE.
 * @no_pi:    if nonzero, sets IFF_NO_PI (suppress packet info header).
 * Returns 0 on success, -1 if name is NULL or longer than IFNAMSIZ-1 (15).
 */
int nfs_tuntap_prepare_ifr(struct ifreq *ifr, const char *dev_name, int mode, int no_pi);

/*
 * nfs_tuntap_valid_name -- check whether a device name is valid.
 *
 * Valid: 1-15 characters, each alphanumeric, dash, or underscore.
 * Empty string is NOT valid here (use prepare_ifr for kernel auto-assign).
 * Returns 1 if valid, 0 otherwise.
 */
int nfs_tuntap_valid_name(const char *name);

/*
 * nfs_pi_build -- serialise a PI header into @out (network byte order).
 *
 * @out:    output buffer.
 * @out_sz: size of output buffer (must be >= NFS_PI_HDR_SIZE).
 * @flags:  PI flags (host byte order).
 * @proto:  EtherType (host byte order, e.g. 0x0800).
 * Returns NFS_PI_HDR_SIZE (4) on success, -1 if buffer too small.
 */
int nfs_pi_build(uint8_t *out, size_t out_sz, uint16_t flags, uint16_t proto);

/*
 * nfs_pi_parse -- parse a PI header from raw bytes.
 *
 * @data:  input buffer (at least NFS_PI_HDR_SIZE bytes).
 * @len:   length of input.
 * @flags: receives parsed flags (host byte order).
 * @proto: receives parsed EtherType (host byte order).
 * Returns 0 on success, -1 if len < NFS_PI_HDR_SIZE.
 */
int nfs_pi_parse(const uint8_t *data, size_t len, uint16_t *flags, uint16_t *proto);

/*
 * nfs_tun_wrap_ip -- prepend a PI header (proto=ETH_P_IP) to an IP packet.
 *
 * @ip_pkt: raw IP packet.
 * @ip_len: length of IP packet.
 * @out:    output buffer.
 * @out_sz: size of output buffer.
 * Returns total bytes written (NFS_PI_HDR_SIZE + ip_len), or -1 on error.
 */
int nfs_tun_wrap_ip(const uint8_t *ip_pkt, size_t ip_len, uint8_t *out, size_t out_sz);

/*
 * nfs_tun_unwrap -- strip a PI header from TUN device data.
 *
 * @data:        raw data from TUN fd (PI header + IP packet).
 * @len:         length of data.
 * @proto:       receives the EtherType from the PI header (host byte order).
 * @payload:     receives pointer to the IP packet after the PI header.
 * @payload_len: receives length of the IP packet.
 * Returns 0 on success, -1 if data is too short.
 */
int nfs_tun_unwrap(const uint8_t *data, size_t len, uint16_t *proto, const uint8_t **payload,
                   size_t *payload_len);

/*
 * nfs_tap_wrap_frame -- prepend a PI header (proto=ETH_P_IP) to an Ethernet frame.
 *
 * @frame:     raw Ethernet frame.
 * @frame_len: length of the frame.
 * @out:       output buffer.
 * @out_sz:    size of output buffer.
 * Returns total bytes written (NFS_PI_HDR_SIZE + frame_len), or -1 on error.
 */
int nfs_tap_wrap_frame(const uint8_t *frame, size_t frame_len, uint8_t *out, size_t out_sz);

/*
 * nfs_tap_unwrap -- strip a PI header from TAP device data.
 *
 * @data:        raw data from TAP fd (PI header + Ethernet frame).
 * @len:         length of data.
 * @proto:       receives the EtherType from the PI header (host byte order).
 * @payload:     receives pointer to the Ethernet frame after the PI header.
 * @payload_len: receives length of the Ethernet frame.
 * Returns 0 on success, -1 if data is too short.
 */
int nfs_tap_unwrap(const uint8_t *data, size_t len, uint16_t *proto, const uint8_t **payload,
                   size_t *payload_len);

#endif /* NFS_TUNTAP_H */
