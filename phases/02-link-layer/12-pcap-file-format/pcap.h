#ifndef NFS_PCAP_H
#define NFS_PCAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Classic libpcap file format (NOT pcapng).
 *
 * File layout:
 *   [Global Header 24B]
 *   [Packet Header 16B][Packet Data]
 *   [Packet Header 16B][Packet Data]
 *   ...
 *
 * All multi-byte fields are in the byte order indicated by the magic:
 *   0xa1b2c3d4 — native (same as writer)
 *   0xd4c3b2a1 — swapped (reader must byte-swap)
 */

#define NFS_PCAP_MAGIC             0xa1b2c3d4u
#define NFS_PCAP_MAGIC_SWAPPED     0xd4c3b2a1u
#define NFS_PCAP_VERSION_MAJOR     2
#define NFS_PCAP_VERSION_MINOR     4
#define NFS_PCAP_LINKTYPE_ETHERNET 1

struct nfs_pcap_global_hdr {
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t thiszone;  /* GMT-to-local correction (usually 0) */
    uint32_t sigfigs;  /* accuracy of timestamps (usually 0) */
    uint32_t snaplen;  /* max captured length per packet */
    uint32_t linktype; /* data link type (1 = Ethernet) */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_pcap_global_hdr) == 24,
               "nfs_pcap_global_hdr must be exactly 24 bytes");

struct nfs_pcap_pkt_hdr {
    uint32_t ts_sec;   /* timestamp seconds */
    uint32_t ts_usec;  /* timestamp microseconds */
    uint32_t incl_len; /* number of bytes captured */
    uint32_t orig_len; /* original packet length on wire */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_pcap_pkt_hdr) == 16, "nfs_pcap_pkt_hdr must be exactly 16 bytes");

/*
 * Write the global PCAP header.
 * Returns 0 on success, -1 on error.
 */
int nfs_pcap_write_header(FILE *fp, uint32_t snaplen, uint32_t linktype);

/*
 * Write a single packet (header + data).
 * incl_len = orig_len = len (no truncation).
 * Returns 0 on success, -1 on error.
 */
int nfs_pcap_write_packet(FILE *fp, uint32_t ts_sec, uint32_t ts_usec, const uint8_t *data,
                          uint32_t len);

/*
 * Read and validate the global header.
 * Handles both native and byte-swapped magic.
 * Returns 0 on success, -1 on error.
 */
int nfs_pcap_read_header(FILE *fp, struct nfs_pcap_global_hdr *hdr);

/*
 * Read the next packet.
 * Returns 0 on success, 1 on EOF, -1 on error.
 * `data` must be at least pkt_hdr->incl_len bytes; data_sz is checked.
 */
int nfs_pcap_read_packet(FILE *fp, struct nfs_pcap_pkt_hdr *pkt_hdr, uint8_t *data, size_t data_sz);

/* Format the global header as a human-readable summary. */
void nfs_pcap_format_header(const struct nfs_pcap_global_hdr *h, char *buf, size_t sz);

#endif /* NFS_PCAP_H */
