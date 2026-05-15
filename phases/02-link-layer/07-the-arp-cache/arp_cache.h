#ifndef NFS_ARP_CACHE_H
#define NFS_ARP_CACHE_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/*
 * In-memory ARP cache with aging.
 *
 * State machine for each entry:
 *   INCOMPLETE  ->  REACHABLE  (reply received)
 *   REACHABLE   ->  STALE      (timeout expired)
 *   STALE       ->  removed    (aged out)
 *   any         ->  FAILED     (no reply after retries)
 */

/* Entry states. */
#define NFS_ARP_INCOMPLETE  0
#define NFS_ARP_REACHABLE   1
#define NFS_ARP_STALE       2
#define NFS_ARP_FAILED      3

struct nfs_arp_entry {
    uint32_t ip;            /* IPv4 address, host byte order */
    uint8_t  mac[6];
    int      state;
    time_t   timestamp;     /* time of last state change */
};

struct nfs_arp_cache {
    struct nfs_arp_entry *entries;
    size_t capacity;
    size_t count;
};

/* Initialise the cache with a maximum capacity. */
void nfs_arp_cache_init(struct nfs_arp_cache *c, size_t capacity);

/* Free all internal storage. */
void nfs_arp_cache_free(struct nfs_arp_cache *c);

/*
 * Insert or update an entry.
 * If the IP already exists, update its MAC, state, and timestamp.
 * Returns 0 on success, -1 if the cache is full (new entry only).
 */
int nfs_arp_cache_insert(struct nfs_arp_cache *c, uint32_t ip,
                         const uint8_t mac[6], int state);

/* Look up by IP.  Returns pointer to internal entry, or NULL. */
const struct nfs_arp_entry *nfs_arp_cache_lookup(
    const struct nfs_arp_cache *c, uint32_t ip);

/* Remove entry by IP.  Returns 0 on success, -1 if not found. */
int nfs_arp_cache_remove(struct nfs_arp_cache *c, uint32_t ip);

/*
 * Age entries:
 *   - REACHABLE entries older than timeout_sec -> STALE
 *   - STALE entries older than timeout_sec     -> removed
 * Returns the number of entries changed or removed.
 */
int nfs_arp_cache_age(struct nfs_arp_cache *c, int timeout_sec);

/* Format all entries into a human-readable string. */
void nfs_arp_cache_format(const struct nfs_arp_cache *c,
                          char *buf, size_t sz);

/* Helper: return a human-readable state name. */
const char *nfs_arp_state_name(int state);

#endif /* NFS_ARP_CACHE_H */
