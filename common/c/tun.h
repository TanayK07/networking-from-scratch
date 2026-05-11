#ifndef NFS_TUN_H
#define NFS_TUN_H

#include <stddef.h>
#include <stdint.h>

/* Open a TUN device.
 *
 *   ifname: the requested name (e.g. "tun0"). On return, the actual
 *           assigned name is written back into this buffer.
 *           Must be at least IFNAMSIZ (16) bytes.
 *
 * Returns: file descriptor for the TUN device, or -1 on error.
 *
 * After this call, the caller must configure the interface with `ip`:
 *   ip link set tun0 up
 *   ip addr add 10.0.0.1/24 dev tun0
 */
int tun_open(char *ifname);

/* Open a TAP device (L2 instead of L3). Same semantics. */
int tap_open(char *ifname);

#endif /* NFS_TUN_H */
