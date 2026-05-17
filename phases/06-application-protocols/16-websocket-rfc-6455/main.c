/* WebSocket frame parser/builder demo (RFC 6455). */

#include "websocket.h"
#include <stdio.h>
#include <string.h>

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    if (len % 16 != 0)
        printf("\n");
}

int main(void) {
    printf("=== WebSocket Frame Parser/Builder (RFC 6455) ===\n\n");

    /* Build an unmasked text frame */
    const char *msg = "Hello, WebSocket!";
    uint8_t buf[256];
    int total = nfs_ws_build_frame(NFS_WS_TEXT, 1, 0, NULL, (const uint8_t *)msg, strlen(msg), buf,
                                   sizeof(buf));
    if (total > 0) {
        printf("Unmasked text frame (%d bytes):\n", total);
        print_hex(buf, (size_t)total);

        struct nfs_ws_frame frame;
        size_t hdr_len;
        if (nfs_ws_parse_frame(buf, (size_t)total, &frame, &hdr_len) == 0) {
            printf("  FIN:     %d\n", frame.fin);
            printf("  Opcode:  0x%x (%s)\n", frame.opcode, nfs_ws_opcode_str(frame.opcode));
            printf("  Masked:  %d\n", frame.masked);
            printf("  Length:  %lu\n", (unsigned long)frame.payload_len);
            printf("  Payload: %.*s\n", (int)frame.payload_len, buf + hdr_len);
        }
    }

    printf("\n");

    /* Build a masked text frame */
    uint8_t mask[4] = {0x37, 0xfa, 0x21, 0x3d};
    total = nfs_ws_build_frame(NFS_WS_TEXT, 1, 1, mask, (const uint8_t *)msg, strlen(msg), buf,
                               sizeof(buf));
    if (total > 0) {
        printf("Masked text frame (%d bytes):\n", total);
        print_hex(buf, (size_t)total);

        struct nfs_ws_frame frame;
        size_t hdr_len;
        if (nfs_ws_parse_frame(buf, (size_t)total, &frame, &hdr_len) == 0) {
            printf("  FIN:     %d\n", frame.fin);
            printf("  Opcode:  0x%x (%s)\n", frame.opcode, nfs_ws_opcode_str(frame.opcode));
            printf("  Masked:  %d\n", frame.masked);
            printf("  Length:  %lu\n", (unsigned long)frame.payload_len);

            /* Unmask the payload */
            uint8_t *payload = buf + hdr_len;
            nfs_ws_mask_payload(payload, (size_t)frame.payload_len, frame.mask_key);
            printf("  Payload: %.*s\n", (int)frame.payload_len, payload);
        }
    }

    printf("\n");

    /* Control frame check */
    printf("Control frames:\n");
    uint8_t ops[] = {0x0, 0x1, 0x2, 0x8, 0x9, 0xA};
    for (size_t i = 0; i < sizeof(ops); i++) {
        printf("  0x%x (%s): %s\n", ops[i], nfs_ws_opcode_str(ops[i]),
               nfs_ws_is_control(ops[i]) ? "control" : "data");
    }

    return 0;
}
