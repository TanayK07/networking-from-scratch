/* IP Fragmentation and Reassembly
 *
 * Implements RFC 791 Section 2.3 fragmentation/reassembly logic.
 * Works directly on packed wire-format headers (network byte order).
 */

#include "fragment.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Helper: compute IPv4 header checksum (RFC 1071)                    */
/* ------------------------------------------------------------------ */

static uint16_t ip_checksum(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;

    for (size_t i = 0; i + 1 < len; i += 2)
        sum += (uint32_t)((uint16_t)p[i] << 8 | p[i + 1]);
    if (len & 1)
        sum += (uint32_t)p[len - 1] << 8;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return htons((uint16_t)~sum);
}

/* ------------------------------------------------------------------ */
/*  Flag / offset accessors                                            */
/* ------------------------------------------------------------------ */

uint16_t nfs_ip_get_flags(uint16_t flags_frag_host) {
    return flags_frag_host & 0xE000;
}

uint16_t nfs_ip_get_frag_offset(uint16_t flags_frag_host) {
    return flags_frag_host & NFS_IP_FRAG_OFFSET_MASK;
}

int nfs_ip_is_fragment(uint16_t flags_frag_host) {
    return (flags_frag_host & NFS_IP_FLAG_MF) || (flags_frag_host & NFS_IP_FRAG_OFFSET_MASK) != 0;
}

/* ------------------------------------------------------------------ */
/*  Fragmentation                                                      */
/* ------------------------------------------------------------------ */

int nfs_ip_fragment_packet(const uint8_t *orig_pkt, size_t pkt_len, uint16_t mtu,
                           struct nfs_ip_fragment *frags, size_t max_frags) {
    if (!orig_pkt || pkt_len < 20 || !frags || max_frags == 0)
        return -1;

    const struct nfs_ipv4_hdr *orig = (const struct nfs_ipv4_hdr *)orig_pkt;

    /* IHL — we only handle 20-byte headers (IHL=5). */
    uint8_t ihl = (orig->ver_ihl & 0x0F) * 4;
    if (ihl < 20)
        return -1;

    uint16_t total = ntohs(orig->total_len);
    if (total > pkt_len)
        return -1;

    uint16_t payload_len = total - ihl;

    /* If the packet fits within the MTU, no fragmentation needed. */
    if (total <= mtu) {
        memcpy(frags[0].header, orig_pkt, 20);
        memcpy(frags[0].payload, orig_pkt + ihl, payload_len);
        frags[0].payload_len = payload_len;
        return 1;
    }

    /* Check DF flag. */
    uint16_t flags_frag_host = ntohs(orig->flags_frag);
    if (flags_frag_host & NFS_IP_FLAG_DF)
        return -1;

    /* Maximum payload per fragment — must be multiple of 8. */
    uint16_t max_payload = (uint16_t)((mtu - 20) & ~7u);
    if (max_payload == 0)
        return -1;

    /* Original fragment offset (in 8-byte units) — could already be a frag. */
    uint16_t orig_offset = nfs_ip_get_frag_offset(flags_frag_host);
    int orig_mf = (flags_frag_host & NFS_IP_FLAG_MF) ? 1 : 0;

    /* Calculate number of fragments. */
    size_t nfrags = (payload_len + max_payload - 1) / max_payload;
    if (nfrags > max_frags)
        return -2;

    size_t offset = 0;
    for (size_t i = 0; i < nfrags; i++) {
        size_t chunk = payload_len - offset;
        if (chunk > max_payload)
            chunk = max_payload;

        /* Copy the original header. */
        memcpy(frags[i].header, orig_pkt, 20);
        struct nfs_ipv4_hdr *fhdr = (struct nfs_ipv4_hdr *)frags[i].header;

        /* Update total length. */
        fhdr->total_len = htons((uint16_t)(20 + chunk));

        /* Compute new flags_frag. */
        uint16_t frag_offset = orig_offset + (uint16_t)(offset / 8);
        uint16_t new_flags_frag = frag_offset;
        if (i < nfrags - 1) {
            /* Not the last fragment — set MF. */
            new_flags_frag |= NFS_IP_FLAG_MF;
        } else {
            /* Last fragment — preserve original MF if it was a fragment. */
            if (orig_mf)
                new_flags_frag |= NFS_IP_FLAG_MF;
        }
        fhdr->flags_frag = htons(new_flags_frag);

        /* Recompute header checksum. */
        fhdr->checksum = 0;
        fhdr->checksum = ip_checksum(fhdr, 20);

        /* Copy payload. */
        memcpy(frags[i].payload, orig_pkt + ihl + offset, chunk);
        frags[i].payload_len = chunk;

        offset += chunk;
    }

    return (int)nfrags;
}

/* ------------------------------------------------------------------ */
/*  Reassembly                                                         */
/* ------------------------------------------------------------------ */

/* Comparison function for sorting fragments by offset. */
static int frag_cmp(const void *a, const void *b) {
    const struct nfs_ip_fragment *fa = (const struct nfs_ip_fragment *)a;
    const struct nfs_ip_fragment *fb = (const struct nfs_ip_fragment *)b;

    uint16_t off_a =
        nfs_ip_get_frag_offset(ntohs(((const struct nfs_ipv4_hdr *)fa->header)->flags_frag));
    uint16_t off_b =
        nfs_ip_get_frag_offset(ntohs(((const struct nfs_ipv4_hdr *)fb->header)->flags_frag));

    return (off_a > off_b) - (off_a < off_b);
}

int nfs_ip_reassemble(const struct nfs_ip_fragment *frags, size_t nfrags, uint8_t *out,
                      size_t out_sz) {
    if (!frags || nfrags == 0 || !out || out_sz < 20)
        return -1;

    /* Make a sorted copy. */
    struct nfs_ip_fragment *sorted = malloc(nfrags * sizeof(*sorted));
    if (!sorted)
        return -1;
    memcpy(sorted, frags, nfrags * sizeof(*sorted));
    qsort(sorted, nfrags, sizeof(*sorted), frag_cmp);

    /* First fragment must have offset 0. */
    uint16_t first_ff = ntohs(((const struct nfs_ipv4_hdr *)sorted[0].header)->flags_frag);
    if (nfs_ip_get_frag_offset(first_ff) != 0) {
        free(sorted);
        return -1;
    }

    /* Walk fragments, verify contiguity, accumulate payload. */
    size_t total_payload = 0;
    uint16_t expected_offset = 0; /* in 8-byte units */

    for (size_t i = 0; i < nfrags; i++) {
        const struct nfs_ipv4_hdr *fhdr = (const struct nfs_ipv4_hdr *)sorted[i].header;
        uint16_t ff = ntohs(fhdr->flags_frag);
        uint16_t off = nfs_ip_get_frag_offset(ff);

        if (off != expected_offset) {
            free(sorted);
            return -1;
        }

        size_t plen = sorted[i].payload_len;
        if (20 + total_payload + plen > out_sz) {
            free(sorted);
            return -1;
        }

        memcpy(out + 20 + total_payload, sorted[i].payload, plen);
        total_payload += plen;
        expected_offset = (uint16_t)(off + (plen / 8));

        /* If this is the last fragment (MF not set), we're done. */
        if (!(ff & NFS_IP_FLAG_MF)) {
            if (i != nfrags - 1) {
                /* Extra fragments after the last — error. */
                free(sorted);
                return -1;
            }
        }
    }

    /* Reconstruct the header from the first fragment. */
    memcpy(out, sorted[0].header, 20);
    struct nfs_ipv4_hdr *outhdr = (struct nfs_ipv4_hdr *)out;

    /* Clear MF flag and fragment offset. */
    outhdr->flags_frag = 0;

    /* Update total length. */
    outhdr->total_len = htons((uint16_t)(20 + total_payload));

    /* Recompute checksum. */
    outhdr->checksum = 0;
    outhdr->checksum = ip_checksum(outhdr, 20);

    free(sorted);
    return (int)(20 + total_payload);
}
