/*
 * main.c -- Demo driver for TLS 1.3 ClientHello
 */

#include "tls_clienthello.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* Build a minimal ClientHello */
    struct nfs_tls_client_hello ch;
    memset(&ch, 0, sizeof(ch));

    ch.legacy_version = 0x0303;  /* TLS 1.2 for compat */

    /* Fill random with test pattern */
    for (int i = 0; i < 32; i++) ch.random[i] = (uint8_t)i;

    ch.session_id_len = 0;

    /* Cipher suites */
    ch.cipher_suites_count = 3;
    ch.cipher_suites[0] = NFS_TLS_CS_AES_128_GCM_SHA256;
    ch.cipher_suites[1] = NFS_TLS_CS_AES_256_GCM_SHA384;
    ch.cipher_suites[2] = NFS_TLS_CS_CHACHA20_POLY1305_SHA256;

    /* Compression: null only */
    ch.compression_len = 1;
    ch.compression[0] = 0x00;

    /* Extensions: supported_versions with TLS 1.3 */
    uint8_t sv_data[] = {0x03, 0x03, 0x04};  /* len=3: {0x0304} */
    ch.extensions_count = 1;
    ch.extensions[0].type = NFS_TLS_EXT_SUPPORTED_VERSIONS;
    ch.extensions[0].length = sizeof(sv_data);

    const uint8_t *ext_bufs[] = {sv_data};

    uint8_t buf[512];
    int n = nfs_tls_ch_build(buf, sizeof(buf), &ch, ext_bufs);
    printf("Built ClientHello: %d bytes\n\n", n);

    /* Parse it back */
    struct nfs_tls_client_hello parsed;
    int consumed = nfs_tls_ch_parse(buf, (size_t)n, &parsed);
    printf("Parsed ClientHello: %d bytes consumed\n", consumed);
    printf("  legacy_version: 0x%04x\n", parsed.legacy_version);
    printf("  cipher_suites:  %u\n", parsed.cipher_suites_count);
    for (uint16_t i = 0; i < parsed.cipher_suites_count; i++) {
        printf("    0x%04x = %s\n", parsed.cipher_suites[i],
               nfs_tls_cipher_suite_name(parsed.cipher_suites[i]));
    }
    printf("  extensions:     %u\n", parsed.extensions_count);
    for (uint16_t i = 0; i < parsed.extensions_count; i++) {
        printf("    type=%u (%s) len=%u\n",
               parsed.extensions[i].type,
               nfs_tls_extension_name(parsed.extensions[i].type),
               parsed.extensions[i].length);
    }

    return 0;
}
