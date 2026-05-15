#ifndef NFS_VHOST_H
#define NFS_VHOST_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * HTTP/1.1 with virtual hosts.
 *
 * In HTTP/1.1 the Host header is mandatory.  A server uses it
 * to route requests to different site configurations (virtual
 * hosts) sharing the same IP address.
 *
 * This module implements:
 *   - Virtual host table: map hostnames to site configs
 *   - Host header extraction from HTTP requests
 *   - Default host for unmatched requests
 *   - Routing logic
 * --------------------------------------------------------------- */

/* Limits */
#define NFS_VHOST_MAX_HOSTS     32
#define NFS_VHOST_MAX_HOSTNAME  256
#define NFS_VHOST_MAX_DOCROOT   512
#define NFS_VHOST_MAX_ALIASES   8
#define NFS_VHOST_MAX_HDR_NAME  128
#define NFS_VHOST_MAX_HDR_VALUE 4096
#define NFS_VHOST_MAX_HEADERS   64
#define NFS_VHOST_MAX_METHOD    16
#define NFS_VHOST_MAX_URI       2048

/* A single virtual host configuration */
struct nfs_vhost {
    char hostname[NFS_VHOST_MAX_HOSTNAME];
    char docroot[NFS_VHOST_MAX_DOCROOT];
    char aliases[NFS_VHOST_MAX_ALIASES][NFS_VHOST_MAX_HOSTNAME];
    int  alias_count;
    int  active;
};

/* Virtual host table */
struct nfs_vhost_table {
    struct nfs_vhost hosts[NFS_VHOST_MAX_HOSTS];
    int              count;
    int              default_idx;  /* index of default host, -1 if none */
};

/* Parsed HTTP request (minimal, for Host extraction) */
struct nfs_vhost_request {
    char method[NFS_VHOST_MAX_METHOD];
    char uri[NFS_VHOST_MAX_URI];
    char version[16];
    uint16_t header_count;
    struct {
        char name[NFS_VHOST_MAX_HDR_NAME];
        char value[NFS_VHOST_MAX_HDR_VALUE];
    } headers[NFS_VHOST_MAX_HEADERS];
};

/* --- Virtual host table management ------------------------------ */

/* Initialize an empty virtual host table. */
void nfs_vhost_table_init(struct nfs_vhost_table *table);

/* Add a virtual host with the given hostname and document root.
 * If `is_default` is non-zero, this becomes the default host.
 * Returns the host index, or -1 on error. */
int nfs_vhost_add(struct nfs_vhost_table *table,
                  const char *hostname, const char *docroot,
                  int is_default);

/* Add an alias hostname for an existing virtual host.
 * Returns 0 on success, -1 on error. */
int nfs_vhost_add_alias(struct nfs_vhost_table *table,
                        int host_idx, const char *alias);

/* Look up a virtual host by hostname (case-insensitive).
 * Checks primary hostname and aliases.
 * Falls back to default host if no match.
 * Returns pointer to the vhost, or NULL if no default. */
const struct nfs_vhost *nfs_vhost_lookup(
    const struct nfs_vhost_table *table,
    const char *hostname);

/* Get the default virtual host.
 * Returns pointer or NULL if no default set. */
const struct nfs_vhost *nfs_vhost_get_default(
    const struct nfs_vhost_table *table);

/* Get the number of configured virtual hosts. */
int nfs_vhost_count(const struct nfs_vhost_table *table);

/* --- Host header extraction ------------------------------------- */

/* Extract the Host header from a raw HTTP request.
 * Writes the hostname (without port) to `host_out`.
 * Returns 0 on success, -1 if Host header not found. */
int nfs_vhost_extract_host(const char *request, size_t request_len,
                           char *host_out, size_t host_out_sz);

/* Parse a Host header value, stripping the port if present.
 * "example.com:8080" -> "example.com"
 * Returns 0 on success, -1 on error. */
int nfs_vhost_parse_host_value(const char *value,
                               char *hostname, size_t hostname_sz,
                               uint16_t *port);

/* --- Request routing -------------------------------------------- */

/* Route an HTTP request to a virtual host.
 * Parses the Host header, looks up the vhost, returns it.
 * Returns NULL if Host is missing (HTTP/1.1 violation) and
 * no default host is configured. */
const struct nfs_vhost *nfs_vhost_route(
    const struct nfs_vhost_table *table,
    const char *request, size_t request_len);

/* Validate that an HTTP/1.1 request has a Host header.
 * Returns 0 if valid (Host present), -1 if missing. */
int nfs_vhost_validate_request(const char *request, size_t request_len);

#endif /* NFS_VHOST_H */
