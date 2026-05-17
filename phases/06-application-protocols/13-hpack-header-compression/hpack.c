#include "hpack.h"
#include <string.h>

/* ---------------------------------------------------------------
 * HPACK integer encoding (RFC 7541 Section 5.1)
 *
 * If value < 2^N - 1, encode in the prefix bits of the first byte.
 * Otherwise, set prefix bits to all 1s, then encode the remainder
 * as a sequence of 7-bit chunks with continuation bit.
 * --------------------------------------------------------------- */

int nfs_hpack_encode_int(uint8_t *buf, size_t out_sz, uint64_t value, uint8_t prefix_bits) {
    if (!buf || prefix_bits < 1 || prefix_bits > 8 || out_sz == 0)
        return -1;

    uint8_t max_prefix = (uint8_t)((1U << prefix_bits) - 1);

    if (value < max_prefix) {
        buf[0] = (uint8_t)(value & max_prefix);
        return 1;
    }

    buf[0] = max_prefix;
    value -= max_prefix;
    size_t pos = 1;

    while (value >= 128) {
        if (pos >= out_sz)
            return -1;
        buf[pos++] = (uint8_t)((value & 0x7F) | 0x80);
        value >>= 7;
    }

    if (pos >= out_sz)
        return -1;
    buf[pos++] = (uint8_t)(value & 0x7F);

    return (int)pos;
}

int nfs_hpack_decode_int(const uint8_t *buf, size_t len, uint64_t *value, uint8_t prefix_bits) {
    if (!buf || !value || len == 0 || prefix_bits < 1 || prefix_bits > 8)
        return -1;

    uint8_t max_prefix = (uint8_t)((1U << prefix_bits) - 1);
    *value = buf[0] & max_prefix;

    if (*value < max_prefix)
        return 1;

    /* Multi-byte encoding */
    uint64_t m = 0;
    size_t pos = 1;

    for (;;) {
        if (pos >= len)
            return -1;

        uint8_t b = buf[pos];
        *value += (uint64_t)(b & 0x7F) << m;
        m += 7;
        pos++;

        if ((b & 0x80) == 0)
            break;

        /* Prevent overflow: 64-bit max */
        if (m > 63)
            return -1;
    }

    return (int)pos;
}

/* ---------------------------------------------------------------
 * Static table (RFC 7541 Appendix A) -- 61 entries, 1-indexed.
 * --------------------------------------------------------------- */

static const struct {
    const char *name;
    const char *value;
} hpack_static_table[] = {
    {NULL, NULL},                         /* 0: unused */
    {":authority", ""},                   /* 1 */
    {":method", "GET"},                   /* 2 */
    {":method", "POST"},                  /* 3 */
    {":path", "/"},                       /* 4 */
    {":path", "/index.html"},             /* 5 */
    {":scheme", "http"},                  /* 6 */
    {":scheme", "https"},                 /* 7 */
    {":status", "200"},                   /* 8 */
    {":status", "204"},                   /* 9 */
    {":status", "206"},                   /* 10 */
    {":status", "304"},                   /* 11 */
    {":status", "400"},                   /* 12 */
    {":status", "404"},                   /* 13 */
    {":status", "500"},                   /* 14 */
    {"accept-charset", ""},               /* 15 */
    {"accept-encoding", "gzip, deflate"}, /* 16 */
    {"accept-language", ""},              /* 17 */
    {"accept-ranges", ""},                /* 18 */
    {"accept", ""},                       /* 19 */
    {"access-control-allow-origin", ""},  /* 20 */
    {"age", ""},                          /* 21 */
    {"allow", ""},                        /* 22 */
    {"authorization", ""},                /* 23 */
    {"cache-control", ""},                /* 24 */
    {"content-disposition", ""},          /* 25 */
    {"content-encoding", ""},             /* 26 */
    {"content-language", ""},             /* 27 */
    {"content-length", ""},               /* 28 */
    {"content-location", ""},             /* 29 */
    {"content-range", ""},                /* 30 */
    {"content-type", ""},                 /* 31 */
    {"cookie", ""},                       /* 32 */
    {"date", ""},                         /* 33 */
    {"etag", ""},                         /* 34 */
    {"expect", ""},                       /* 35 */
    {"expires", ""},                      /* 36 */
    {"from", ""},                         /* 37 */
    {"host", ""},                         /* 38 */
    {"if-match", ""},                     /* 39 */
    {"if-modified-since", ""},            /* 40 */
    {"if-none-match", ""},                /* 41 */
    {"if-range", ""},                     /* 42 */
    {"if-unmodified-since", ""},          /* 43 */
    {"last-modified", ""},                /* 44 */
    {"link", ""},                         /* 45 */
    {"location", ""},                     /* 46 */
    {"max-forwards", ""},                 /* 47 */
    {"proxy-authenticate", ""},           /* 48 */
    {"proxy-authorization", ""},          /* 49 */
    {"range", ""},                        /* 50 */
    {"referer", ""},                      /* 51 */
    {"refresh", ""},                      /* 52 */
    {"retry-after", ""},                  /* 53 */
    {"server", ""},                       /* 54 */
    {"set-cookie", ""},                   /* 55 */
    {"strict-transport-security", ""},    /* 56 */
    {"transfer-encoding", ""},            /* 57 */
    {"user-agent", ""},                   /* 58 */
    {"vary", ""},                         /* 59 */
    {"via", ""},                          /* 60 */
    {"www-authenticate", ""},             /* 61 */
};

#define STATIC_TABLE_SIZE 61

int nfs_hpack_static_lookup(int index, const char **name, const char **value) {
    if (index < 1 || index > STATIC_TABLE_SIZE)
        return -1;
    if (!name || !value)
        return -1;
    *name = hpack_static_table[index].name;
    *value = hpack_static_table[index].value;
    return 0;
}

int nfs_hpack_static_find(const char *name, const char *value) {
    if (!name)
        return 0;

    int name_match = 0;

    for (int i = 1; i <= STATIC_TABLE_SIZE; i++) {
        if (strcmp(hpack_static_table[i].name, name) == 0) {
            if (value && strcmp(hpack_static_table[i].value, value) == 0)
                return i; /* exact match */
            if (name_match == 0)
                name_match = i; /* first name-only match */
        }
    }

    return name_match; /* 0 if no match */
}

/* ---------------------------------------------------------------
 * Dynamic table: ring buffer with FIFO eviction.
 * Entry size = len(name) + len(value) + 32 (RFC 7541 Section 4.1)
 * --------------------------------------------------------------- */

static size_t entry_size(const char *name, const char *value) {
    return strlen(name) + strlen(value) + 32;
}

void nfs_hpack_dyn_init(struct nfs_hpack_dynamic_table *dt, size_t max_size) {
    if (!dt)
        return;
    dt->count = 0;
    dt->head = 0;
    dt->current_size = 0;
    dt->max_size = max_size;
}

/* Evict the oldest entry (tail of the ring buffer). */
static void evict_oldest(struct nfs_hpack_dynamic_table *dt) {
    if (dt->count == 0)
        return;

    /* Tail index: head - count + 1 (mod capacity) */
    int tail = (dt->head - dt->count + 1 + NFS_HPACK_DYN_MAX_ENTRIES) % NFS_HPACK_DYN_MAX_ENTRIES;

    size_t sz = entry_size(dt->entries[tail].name, dt->entries[tail].value);
    dt->current_size -= sz;
    dt->count--;
}

int nfs_hpack_dyn_add(struct nfs_hpack_dynamic_table *dt, const char *name, const char *value) {
    if (!dt || !name || !value)
        return -1;

    size_t sz = entry_size(name, value);

    /* If entry is larger than max_size, evict everything and add nothing */
    if (sz > dt->max_size) {
        dt->count = 0;
        dt->current_size = 0;
        return 0;
    }

    /* Evict until there is room */
    while (dt->current_size + sz > dt->max_size)
        evict_oldest(dt);

    /* Check slot limit */
    if (dt->count >= NFS_HPACK_DYN_MAX_ENTRIES)
        evict_oldest(dt);

    /* Advance head */
    if (dt->count > 0)
        dt->head = (dt->head + 1) % NFS_HPACK_DYN_MAX_ENTRIES;

    /* Store entry */
    struct nfs_hpack_header *e = &dt->entries[dt->head];
    strncpy(e->name, name, NFS_HPACK_MAX_NAME - 1);
    e->name[NFS_HPACK_MAX_NAME - 1] = '\0';
    strncpy(e->value, value, NFS_HPACK_MAX_VALUE - 1);
    e->value[NFS_HPACK_MAX_VALUE - 1] = '\0';

    dt->count++;
    dt->current_size += sz;
    return 0;
}

int nfs_hpack_dyn_lookup(const struct nfs_hpack_dynamic_table *dt, int index, const char **name,
                         const char **value) {
    if (!dt || !name || !value)
        return -1;
    if (index < 0 || index >= dt->count)
        return -1;

    /* Index 0 = newest = head, index 1 = head-1, etc. */
    int slot = (dt->head - index + NFS_HPACK_DYN_MAX_ENTRIES) % NFS_HPACK_DYN_MAX_ENTRIES;
    *name = dt->entries[slot].name;
    *value = dt->entries[slot].value;
    return 0;
}

int nfs_hpack_dyn_count(const struct nfs_hpack_dynamic_table *dt) {
    if (!dt)
        return 0;
    return dt->count;
}
