#ifndef NFS_HPACK_H
#define NFS_HPACK_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * HPACK Header Compression (RFC 7541)
 *
 * Integer encoding: N-bit prefix with multi-byte continuation.
 * Static table: 61 predefined header name/value pairs.
 * Dynamic table: FIFO with size-bounded eviction.
 * String encoding: literal (Huffman skipped for simplicity).
 * --------------------------------------------------------------- */

/* Maximum name/value lengths */
#define NFS_HPACK_MAX_NAME   128
#define NFS_HPACK_MAX_VALUE  4096

/* A header name-value pair */
struct nfs_hpack_header {
    char name[NFS_HPACK_MAX_NAME];
    char value[NFS_HPACK_MAX_VALUE];
};

/* ---- Integer encoding (RFC 7541 Section 5.1) ---- */

/* Encode an integer with the given prefix bit-width (1-8).
 * buf/out_sz: output buffer.
 * Returns bytes written, or -1 on error. */
int nfs_hpack_encode_int(uint8_t *buf, size_t out_sz,
                         uint64_t value, uint8_t prefix_bits);

/* Decode an integer with the given prefix bit-width (1-8).
 * buf/len: input buffer.
 * *value: decoded integer.
 * Returns bytes consumed, or -1 on error. */
int nfs_hpack_decode_int(const uint8_t *buf, size_t len,
                         uint64_t *value, uint8_t prefix_bits);

/* ---- Static table (RFC 7541 Appendix A) ---- */

/* Lookup entry in the static table by 1-based index.
 * Returns 0 on success, -1 if index out of range. */
int nfs_hpack_static_lookup(int index, const char **name, const char **value);

/* Find a static table index matching the given name (and optionally value).
 * If value is NULL, matches name only.
 * Returns 1-based index, or 0 if not found. */
int nfs_hpack_static_find(const char *name, const char *value);

/* ---- Dynamic table ---- */

#define NFS_HPACK_DYN_MAX_ENTRIES  128

struct nfs_hpack_dynamic_table {
    struct nfs_hpack_header entries[NFS_HPACK_DYN_MAX_ENTRIES];
    int    count;         /* number of entries currently stored */
    int    head;          /* ring buffer head (newest) */
    size_t current_size;  /* sum of entry sizes (name_len + value_len + 32) */
    size_t max_size;      /* maximum table size in bytes */
};

/* Initialize a dynamic table with the given max size. */
void nfs_hpack_dyn_init(struct nfs_hpack_dynamic_table *dt, size_t max_size);

/* Add an entry to the dynamic table (evicts oldest if needed).
 * Returns 0 on success, -1 on error. */
int nfs_hpack_dyn_add(struct nfs_hpack_dynamic_table *dt,
                      const char *name, const char *value);

/* Lookup in dynamic table by 0-based index (0 = newest).
 * Returns 0 on success, -1 if out of range. */
int nfs_hpack_dyn_lookup(const struct nfs_hpack_dynamic_table *dt,
                         int index, const char **name, const char **value);

/* Return the number of entries in the dynamic table. */
int nfs_hpack_dyn_count(const struct nfs_hpack_dynamic_table *dt);

#endif /* NFS_HPACK_H */
