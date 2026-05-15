/*
 * main.c -- Demo driver for Blocking vs Nonblocking I/O
 */

#include "nonblock.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);

    /* Open an fd in blocking mode */
    int fd = nfs_nb_open(&ctx);
    printf("Opened fd=%d (nonblocking=%d)\n", fd, nfs_nb_is_nonblock(&ctx, fd));

    /* Try reading with no data -- would block */
    uint8_t buf[64];
    int rc = nfs_nb_read(&ctx, fd, buf, sizeof(buf));
    printf("Read with no data: rc=%d (EAGAIN=%d)\n", rc, NFS_NB_EAGAIN);

    /* Set nonblocking mode */
    nfs_nb_set_nonblock(&ctx, fd, 1);
    printf("Set nonblocking: %d\n", nfs_nb_is_nonblock(&ctx, fd));
    printf("Would block? %d\n", nfs_nb_would_block(&ctx, fd));

    /* Inject data and read */
    const char *msg = "Hello, nonblocking!";
    nfs_nb_inject_readable(&ctx, fd, (const uint8_t *)msg, strlen(msg));
    printf("Would block after inject? %d\n", nfs_nb_would_block(&ctx, fd));

    rc = nfs_nb_read(&ctx, fd, buf, sizeof(buf));
    printf("Read %d bytes: \"%.*s\"\n", rc, rc, buf);

    /* Write test */
    rc = nfs_nb_write(&ctx, fd, (const uint8_t *)"test", 4);
    printf("Write returned: %d\n", rc);

    /* Make not writable and try again */
    nfs_nb_set_writable(&ctx, fd, 0);
    rc = nfs_nb_write(&ctx, fd, (const uint8_t *)"test", 4);
    printf("Write when not writable: %d (EAGAIN=%d)\n", rc, NFS_NB_EAGAIN);

    nfs_nb_close(&ctx, fd);
    printf("Closed fd=%d\n", fd);

    return 0;
}
