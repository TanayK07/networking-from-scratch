/* TUN/TAP device wrapper.
 *
 * The kernel exposes virtual L2 (TAP) and L3 (TUN) network devices via
 * /dev/net/tun. Open the device, ioctl with the desired flags, and you
 * get a file descriptor you can read packets from and write packets to
 * — as if you were the network card.
 *
 * See: kernel docs/networking/tuntap.txt
 */

#include "tun.h"

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int open_dev(char *ifname, int flags) {
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) return -1;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = flags | IFF_NO_PI;
    if (ifname[0]) {
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    }

    if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
        close(fd);
        return -1;
    }

    /* Write back the actual name (kernel may pick if we passed empty). */
    strncpy(ifname, ifr.ifr_name, IFNAMSIZ - 1);
    ifname[IFNAMSIZ - 1] = '\0';
    return fd;
}

int tun_open(char *ifname) { return open_dev(ifname, IFF_TUN); }
int tap_open(char *ifname) { return open_dev(ifname, IFF_TAP); }
