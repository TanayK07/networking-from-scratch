/* HTTP/1.1 with virtual hosts implementation. */

#include "vhost.h"
#include <string.h>
#include <strings.h>

/* ---------------------------------------------------------------
 * Helper: find CRLF
 * --------------------------------------------------------------- */

static const char *find_crlf(const char *data, size_t len)
{
    for (size_t i = 0; i + 1 < len; i++) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            return data + i;
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------
 * Virtual host table management
 * --------------------------------------------------------------- */

void nfs_vhost_table_init(struct nfs_vhost_table *table)
{
    if (!table) return;
    memset(table, 0, sizeof(*table));
    table->default_idx = -1;
}

int nfs_vhost_add(struct nfs_vhost_table *table,
                  const char *hostname, const char *docroot,
                  int is_default)
{
    if (!table || !hostname || !docroot) return -1;
    if (table->count >= NFS_VHOST_MAX_HOSTS) return -1;

    size_t hlen = strlen(hostname);
    size_t dlen = strlen(docroot);
    if (hlen >= NFS_VHOST_MAX_HOSTNAME || dlen >= NFS_VHOST_MAX_DOCROOT)
        return -1;

    int idx = table->count;
    struct nfs_vhost *vh = &table->hosts[idx];
    memset(vh, 0, sizeof(*vh));
    memcpy(vh->hostname, hostname, hlen + 1);
    memcpy(vh->docroot, docroot, dlen + 1);
    vh->active = 1;

    if (is_default || table->count == 0) {
        table->default_idx = idx;
    }

    table->count++;
    return idx;
}

int nfs_vhost_add_alias(struct nfs_vhost_table *table,
                        int host_idx, const char *alias)
{
    if (!table || !alias) return -1;
    if (host_idx < 0 || host_idx >= table->count) return -1;

    struct nfs_vhost *vh = &table->hosts[host_idx];
    if (vh->alias_count >= NFS_VHOST_MAX_ALIASES) return -1;

    size_t alen = strlen(alias);
    if (alen >= NFS_VHOST_MAX_HOSTNAME) return -1;

    memcpy(vh->aliases[vh->alias_count], alias, alen + 1);
    vh->alias_count++;

    return 0;
}

const struct nfs_vhost *nfs_vhost_lookup(
    const struct nfs_vhost_table *table,
    const char *hostname)
{
    if (!table || !hostname) return NULL;

    /* Search primary hostnames and aliases */
    for (int i = 0; i < table->count; i++) {
        const struct nfs_vhost *vh = &table->hosts[i];
        if (!vh->active) continue;

        if (strcasecmp(vh->hostname, hostname) == 0) {
            return vh;
        }

        for (int j = 0; j < vh->alias_count; j++) {
            if (strcasecmp(vh->aliases[j], hostname) == 0) {
                return vh;
            }
        }
    }

    /* Fall back to default */
    if (table->default_idx >= 0 && table->default_idx < table->count) {
        return &table->hosts[table->default_idx];
    }

    return NULL;
}

const struct nfs_vhost *nfs_vhost_get_default(
    const struct nfs_vhost_table *table)
{
    if (!table || table->default_idx < 0) return NULL;
    if (table->default_idx >= table->count) return NULL;
    return &table->hosts[table->default_idx];
}

int nfs_vhost_count(const struct nfs_vhost_table *table)
{
    if (!table) return 0;
    return table->count;
}

/* ---------------------------------------------------------------
 * Host header extraction
 * --------------------------------------------------------------- */

int nfs_vhost_parse_host_value(const char *value,
                               char *hostname, size_t hostname_sz,
                               uint16_t *port)
{
    if (!value || !hostname || hostname_sz == 0) return -1;

    size_t vlen = strlen(value);

    /* Check for IPv6 address in brackets */
    if (value[0] == '[') {
        const char *bracket = strchr(value, ']');
        if (!bracket) return -1;
        size_t hlen = (size_t)(bracket - value - 1);
        if (hlen >= hostname_sz) return -1;
        memcpy(hostname, value + 1, hlen);
        hostname[hlen] = '\0';

        if (port) {
            if (bracket[1] == ':') {
                *port = 0;
                const char *p = bracket + 2;
                while (*p >= '0' && *p <= '9') {
                    *port = (uint16_t)(*port * 10 + (*p - '0'));
                    p++;
                }
            } else {
                *port = 80;
            }
        }
        return 0;
    }

    /* Look for port separator */
    const char *colon = strchr(value, ':');
    if (colon) {
        size_t hlen = (size_t)(colon - value);
        if (hlen >= hostname_sz) return -1;
        memcpy(hostname, value, hlen);
        hostname[hlen] = '\0';

        if (port) {
            *port = 0;
            const char *p = colon + 1;
            while (*p >= '0' && *p <= '9') {
                *port = (uint16_t)(*port * 10 + (*p - '0'));
                p++;
            }
        }
    } else {
        if (vlen >= hostname_sz) return -1;
        memcpy(hostname, value, vlen + 1);
        if (port) *port = 80;
    }

    return 0;
}

int nfs_vhost_extract_host(const char *request, size_t request_len,
                           char *host_out, size_t host_out_sz)
{
    if (!request || !host_out || host_out_sz == 0) return -1;

    const char *pos = request;
    const char *end = request + request_len;

    /* Skip request line */
    const char *crlf = find_crlf(pos, (size_t)(end - pos));
    if (!crlf) return -1;
    pos = crlf + 2;

    /* Search headers for Host */
    while (pos < end) {
        if (pos + 1 < end && pos[0] == '\r' && pos[1] == '\n') {
            break; /* end of headers */
        }

        crlf = find_crlf(pos, (size_t)(end - pos));
        if (!crlf) return -1;

        /* Check if this is the Host header */
        size_t line_len = (size_t)(crlf - pos);
        if (line_len > 5 && (pos[0] == 'H' || pos[0] == 'h') &&
            (pos[1] == 'o' || pos[1] == 'O') &&
            (pos[2] == 's' || pos[2] == 'S') &&
            (pos[3] == 't' || pos[3] == 'T') &&
            pos[4] == ':') {
            const char *val = pos + 5;
            while (val < crlf && *val == ' ') val++;
            size_t vlen = (size_t)(crlf - val);

            /* Parse the value, stripping port */
            char raw_val[NFS_VHOST_MAX_HOSTNAME];
            if (vlen >= sizeof(raw_val)) return -1;
            memcpy(raw_val, val, vlen);
            raw_val[vlen] = '\0';

            return nfs_vhost_parse_host_value(raw_val, host_out,
                                               host_out_sz, NULL);
        }

        pos = crlf + 2;
    }

    return -1; /* Host header not found */
}

/* ---------------------------------------------------------------
 * Request routing
 * --------------------------------------------------------------- */

const struct nfs_vhost *nfs_vhost_route(
    const struct nfs_vhost_table *table,
    const char *request, size_t request_len)
{
    if (!table || !request) return NULL;

    char hostname[NFS_VHOST_MAX_HOSTNAME];
    if (nfs_vhost_extract_host(request, request_len,
                                hostname, sizeof(hostname)) != 0) {
        /* No Host header: return default if available */
        return nfs_vhost_get_default(table);
    }

    return nfs_vhost_lookup(table, hostname);
}

int nfs_vhost_validate_request(const char *request, size_t request_len)
{
    if (!request) return -1;

    /* Check if this is HTTP/1.1 */
    const char *crlf = find_crlf(request, request_len);
    if (!crlf) return -1;

    /* Check version */
    const char *http11 = "HTTP/1.1";
    size_t line_len = (size_t)(crlf - request);
    /* Version is at the end of the request line */
    if (line_len < 8) return -1;
    if (memcmp(crlf - 8, http11, 8) != 0) {
        /* Not HTTP/1.1, Host is not required */
        return 0;
    }

    /* HTTP/1.1: Host header is required */
    char hostname[NFS_VHOST_MAX_HOSTNAME];
    if (nfs_vhost_extract_host(request, request_len,
                                hostname, sizeof(hostname)) != 0) {
        return -1; /* Missing Host header */
    }

    return 0;
}
