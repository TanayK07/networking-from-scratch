/* Demo driver for toy HTTP server lesson. */

#include "http.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    /* Parse a sample HTTP request */
    const char *raw_req = "GET /index.html HTTP/1.1\r\n"
                          "Host: www.example.com\r\n"
                          "User-Agent: nfs-demo/1.0\r\n"
                          "Accept: text/html\r\n"
                          "Connection: keep-alive\r\n"
                          "\r\n";

    printf("=== Request Parsing ===\n");
    struct nfs_http_request req;
    if (nfs_http_request_parse(raw_req, strlen(raw_req), &req) == 0) {
        printf("Method:  %s\n", req.method);
        printf("URI:     %s\n", req.uri);
        printf("Version: %s\n", req.version);
        printf("Headers: %u\n", req.header_count);
        for (uint16_t i = 0; i < req.header_count; i++) {
            printf("  %s: %s\n", req.headers[i].name, req.headers[i].value);
        }
        printf("Body:    %zu bytes\n", req.body_len);

        const char *host = nfs_http_request_get_header(&req, "Host");
        printf("Host header: %s\n", host ? host : "(none)");
    }

    /* Build a sample HTTP response */
    printf("\n=== Response Building ===\n");
    struct nfs_http_response resp;
    nfs_http_response_init(&resp, 200);
    nfs_http_response_add_header(&resp, "Content-Type", "text/html");
    nfs_http_response_add_header(&resp, "Content-Length", "45");
    nfs_http_response_add_header(&resp, "Connection", "close");

    const char *body = "<html><body>Hello, World!</body></html>\r\n";
    nfs_http_response_set_body(&resp, (const uint8_t *)body, strlen(body));

    char buf[4096];
    int blen = nfs_http_response_build(&resp, buf, sizeof(buf));
    if (blen > 0) {
        printf("Response (%d bytes):\n%.*s\n", blen, blen, buf);
    }

    /* Error responses */
    printf("\n=== Status Codes ===\n");
    uint16_t codes[] = {200, 301, 400, 404, 500};
    for (int i = 0; i < 5; i++) {
        printf("%u: %s\n", codes[i], nfs_http_status_str(codes[i]));
    }

    return 0;
}
