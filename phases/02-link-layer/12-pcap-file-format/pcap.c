#include "pcap.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal: byte-swap helpers                                        */
/* ------------------------------------------------------------------ */

static uint16_t swap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

static uint32_t swap32(uint32_t v) {
    return ((v >> 24) & 0x000000FFu) | ((v >> 8) & 0x0000FF00u) | ((v << 8) & 0x00FF0000u) |
           ((v << 24) & 0xFF000000u);
}

/* ------------------------------------------------------------------ */
/*  Write                                                              */
/* ------------------------------------------------------------------ */

int nfs_pcap_write_header(FILE *fp, uint32_t snaplen, uint32_t linktype) {
    if (!fp)
        return -1;

    struct nfs_pcap_global_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = NFS_PCAP_MAGIC;
    hdr.version_major = NFS_PCAP_VERSION_MAJOR;
    hdr.version_minor = NFS_PCAP_VERSION_MINOR;
    hdr.thiszone = 0;
    hdr.sigfigs = 0;
    hdr.snaplen = snaplen;
    hdr.linktype = linktype;

    if (fwrite(&hdr, sizeof(hdr), 1, fp) != 1)
        return -1;
    return 0;
}

int nfs_pcap_write_packet(FILE *fp, uint32_t ts_sec, uint32_t ts_usec, const uint8_t *data,
                          uint32_t len) {
    if (!fp || !data)
        return -1;

    struct nfs_pcap_pkt_hdr ph;
    ph.ts_sec = ts_sec;
    ph.ts_usec = ts_usec;
    ph.incl_len = len;
    ph.orig_len = len;

    if (fwrite(&ph, sizeof(ph), 1, fp) != 1)
        return -1;
    if (fwrite(data, 1, len, fp) != len)
        return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Read                                                               */
/* ------------------------------------------------------------------ */

int nfs_pcap_read_header(FILE *fp, struct nfs_pcap_global_hdr *hdr) {
    if (!fp || !hdr)
        return -1;

    if (fread(hdr, sizeof(*hdr), 1, fp) != 1)
        return -1;

    if (hdr->magic == NFS_PCAP_MAGIC) {
        /* Native byte order — nothing to do. */
        return 0;
    }

    if (hdr->magic == NFS_PCAP_MAGIC_SWAPPED) {
        /* Byte-swapped.  Convert all fields. */
        hdr->magic = swap32(hdr->magic);
        hdr->version_major = swap16(hdr->version_major);
        hdr->version_minor = swap16(hdr->version_minor);
        hdr->thiszone = (int32_t)swap32((uint32_t)hdr->thiszone);
        hdr->sigfigs = swap32(hdr->sigfigs);
        hdr->snaplen = swap32(hdr->snaplen);
        hdr->linktype = swap32(hdr->linktype);
        return 0;
    }

    /* Invalid magic. */
    return -1;
}

int nfs_pcap_read_packet(FILE *fp, struct nfs_pcap_pkt_hdr *pkt_hdr, uint8_t *data,
                         size_t data_sz) {
    if (!fp || !pkt_hdr || !data)
        return -1;

    size_t n = fread(pkt_hdr, sizeof(*pkt_hdr), 1, fp);
    if (n == 0) {
        /* Could be EOF or error. */
        if (feof(fp))
            return 1;
        return -1;
    }

    if (pkt_hdr->incl_len > data_sz)
        return -1;

    if (pkt_hdr->incl_len > 0) {
        if (fread(data, 1, pkt_hdr->incl_len, fp) != pkt_hdr->incl_len)
            return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Format                                                             */
/* ------------------------------------------------------------------ */

void nfs_pcap_format_header(const struct nfs_pcap_global_hdr *h, char *buf, size_t sz) {
    if (!h || !buf || sz == 0)
        return;
    snprintf(buf, sz, "PCAP magic=0x%08x version=%u.%u snaplen=%u linktype=%u", h->magic,
             h->version_major, h->version_minor, h->snaplen, h->linktype);
}
