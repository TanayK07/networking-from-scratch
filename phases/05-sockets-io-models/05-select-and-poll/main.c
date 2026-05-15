/*
 * main.c -- Demo driver for select() and poll() mock
 */

#include "selectpoll.h"
#include <stdio.h>

int main(void)
{
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);

    /* Open three fds */
    int fd0 = nfs_sp_open(&ctx);
    int fd1 = nfs_sp_open(&ctx);
    int fd2 = nfs_sp_open(&ctx);

    /* Make fd0 readable, fd1 writable, fd2 both */
    nfs_sp_set_ready(&ctx, fd0, NFS_POLLIN);
    nfs_sp_set_ready(&ctx, fd1, NFS_POLLOUT);
    nfs_sp_set_ready(&ctx, fd2, NFS_POLLIN | NFS_POLLOUT);

    /* -- Demo select() -- */
    printf("=== select() demo ===\n");

    struct nfs_fdset readfds, writefds;
    nfs_fdset_zero(&readfds);
    nfs_fdset_zero(&writefds);
    nfs_fdset_set(&readfds, fd0);
    nfs_fdset_set(&readfds, fd1);
    nfs_fdset_set(&readfds, fd2);
    nfs_fdset_set(&writefds, fd0);
    nfs_fdset_set(&writefds, fd1);
    nfs_fdset_set(&writefds, fd2);

    int n = nfs_select(&ctx, fd2 + 1, &readfds, &writefds);
    printf("select returned %d ready fds\n", n);
    printf("  fd%d readable: %d\n", fd0, nfs_fdset_isset(&readfds, fd0));
    printf("  fd%d readable: %d\n", fd1, nfs_fdset_isset(&readfds, fd1));
    printf("  fd%d readable: %d\n", fd2, nfs_fdset_isset(&readfds, fd2));
    printf("  fd%d writable: %d\n", fd1, nfs_fdset_isset(&writefds, fd1));

    /* -- Demo poll() -- */
    printf("\n=== poll() demo ===\n");

    struct nfs_pollfd pfds[3];
    nfs_pollfd_init(&pfds[0], fd0, NFS_POLLIN | NFS_POLLOUT);
    nfs_pollfd_init(&pfds[1], fd1, NFS_POLLIN | NFS_POLLOUT);
    nfs_pollfd_init(&pfds[2], fd2, NFS_POLLIN | NFS_POLLOUT);

    n = nfs_poll(&ctx, pfds, 3);
    printf("poll returned %d ready fds\n", n);
    for (int i = 0; i < 3; i++) {
        printf("  fd%d: events=0x%04x revents=0x%04x\n",
               pfds[i].fd, pfds[i].events, pfds[i].revents);
    }

    return 0;
}
