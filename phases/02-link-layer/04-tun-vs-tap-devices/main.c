#include "tuntap.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Demo: show how TUN/TAP helpers work without requiring root.       */
/* ------------------------------------------------------------------ */

static void print_hex(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n");
}

int main(void) {
    printf("=== TUN vs TAP Devices Demo ===\n\n");

    /* 1. Prepare an ifreq for a TUN device. */
    struct ifreq ifr;
    if (nfs_tuntap_prepare_ifr(&ifr, "tun0", NFS_TUN_MODE, 0) == 0) {
        printf("Prepared ifreq for TUN device:\n");
        printf("  name:  %s\n", ifr.ifr_name);
        printf("  flags: 0x%04x (IFF_TUN=0x%04x)\n", ifr.ifr_flags, IFF_TUN);
    }

    /* 2. Prepare an ifreq for a TAP device with NO_PI. */
    if (nfs_tuntap_prepare_ifr(&ifr, "tap0", NFS_TAP_MODE, 1) == 0) {
        printf("\nPrepared ifreq for TAP device (NO_PI):\n");
        printf("  name:  %s\n", ifr.ifr_name);
        printf("  flags: 0x%04x (IFF_TAP|IFF_NO_PI=0x%04x)\n", ifr.ifr_flags, IFF_TAP | IFF_NO_PI);
    }

    /* 3. Build and parse a PI header. */
    printf("\n--- PI Header ---\n");
    uint8_t pi_buf[NFS_PI_HDR_SIZE];
    nfs_pi_build(pi_buf, sizeof(pi_buf), 0, 0x0800);
    printf("Built PI header (proto=IPv4): ");
    print_hex(pi_buf, NFS_PI_HDR_SIZE);

    uint16_t flags, proto;
    nfs_pi_parse(pi_buf, NFS_PI_HDR_SIZE, &flags, &proto);
    printf("Parsed: flags=0x%04x, proto=0x%04x\n", flags, proto);

    /* 4. TUN wrap/unwrap demo. */
    printf("\n--- TUN Wrap/Unwrap ---\n");
    uint8_t fake_ip[] = {0x45, 0x00, 0x00, 0x3c, 0x00, 0x00}; /* start of IPv4 hdr */
    uint8_t tun_buf[64];
    int tun_len = nfs_tun_wrap_ip(fake_ip, sizeof(fake_ip), tun_buf, sizeof(tun_buf));
    if (tun_len > 0) {
        printf("Wrapped TUN packet (%d bytes): ", tun_len);
        print_hex(tun_buf, (size_t)tun_len);

        const uint8_t *payload;
        size_t payload_len;
        if (nfs_tun_unwrap(tun_buf, (size_t)tun_len, &proto, &payload, &payload_len) == 0) {
            printf("Unwrapped: proto=0x%04x, payload_len=%zu\n", proto, payload_len);
        }
    }

    /* 5. Name validation. */
    printf("\n--- Name Validation ---\n");
    const char *names[] = {"tun0", "tap1", "my-dev", "too_long_name_1234567890", "bad name", NULL};
    for (int i = 0; names[i]; i++) {
        printf("  \"%s\" -> %s\n", names[i], nfs_tuntap_valid_name(names[i]) ? "valid" : "invalid");
    }

    /* 6. Constants. */
    printf("\n--- Constants ---\n");
    printf("  NFS_TUN_DEFAULT_MTU = %d\n", NFS_TUN_DEFAULT_MTU);
    printf("  NFS_TAP_DEFAULT_MTU = %d\n", NFS_TAP_DEFAULT_MTU);
    printf("  NFS_TUN_OVERHEAD    = %d\n", NFS_TUN_OVERHEAD);
    printf("  NFS_TAP_OVERHEAD    = %d (Ethernet header)\n", NFS_TAP_OVERHEAD);

    return 0;
}
