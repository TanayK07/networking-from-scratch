/* Demo driver for HTTP/1.1 with virtual hosts lesson. */

#include "vhost.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* Set up virtual host table */
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);

    int idx0 = nfs_vhost_add(&table, "example.com", "/var/www/example", 1);
    int idx1 = nfs_vhost_add(&table, "blog.example.com", "/var/www/blog", 0);
    nfs_vhost_add(&table, "api.example.com", "/var/www/api", 0);

    nfs_vhost_add_alias(&table, idx0, "www.example.com");
    nfs_vhost_add_alias(&table, idx1, "www.blog.example.com");

    printf("=== Virtual Host Table ===\n");
    printf("Hosts: %d\n", nfs_vhost_count(&table));
    const struct nfs_vhost *def = nfs_vhost_get_default(&table);
    if (def) {
        printf("Default: %s -> %s\n", def->hostname, def->docroot);
    }

    /* Look up various hosts */
    printf("\n=== Lookups ===\n");
    const char *hostnames[] = {
        "example.com", "www.example.com",
        "blog.example.com", "api.example.com",
        "unknown.com"
    };
    for (int i = 0; i < 5; i++) {
        const struct nfs_vhost *vh = nfs_vhost_lookup(&table, hostnames[i]);
        if (vh) {
            printf("%s -> %s (%s)\n", hostnames[i], vh->hostname, vh->docroot);
        } else {
            printf("%s -> (no match)\n", hostnames[i]);
        }
    }

    /* Route requests */
    printf("\n=== Request Routing ===\n");
    const char *req1 =
        "GET / HTTP/1.1\r\n"
        "Host: blog.example.com\r\n"
        "\r\n";
    const struct nfs_vhost *vh = nfs_vhost_route(&table, req1, strlen(req1));
    if (vh) {
        printf("Request to blog.example.com -> %s\n", vh->docroot);
    }

    const char *req2 =
        "GET / HTTP/1.1\r\n"
        "Host: www.example.com:8080\r\n"
        "\r\n";
    vh = nfs_vhost_route(&table, req2, strlen(req2));
    if (vh) {
        printf("Request to www.example.com:8080 -> %s\n", vh->docroot);
    }

    /* Validate request */
    printf("\n=== Validation ===\n");
    printf("With Host: %s\n",
           nfs_vhost_validate_request(req1, strlen(req1)) == 0 ? "valid" : "invalid");

    const char *req_no_host =
        "GET / HTTP/1.1\r\n"
        "\r\n";
    printf("Without Host: %s\n",
           nfs_vhost_validate_request(req_no_host, strlen(req_no_host)) == 0
           ? "valid" : "invalid");

    /* HTTP/1.0 without Host is OK */
    const char *req_10 =
        "GET / HTTP/1.0\r\n"
        "\r\n";
    printf("HTTP/1.0 without Host: %s\n",
           nfs_vhost_validate_request(req_10, strlen(req_10)) == 0
           ? "valid" : "invalid");

    return 0;
}
