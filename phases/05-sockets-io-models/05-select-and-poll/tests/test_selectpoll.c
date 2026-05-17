/* Unit tests for select() and poll() mock. */

#include "../selectpoll.h"
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(expr, expected)                                                                  \
    do {                                                                                           \
        tests_run++;                                                                               \
        long long _got = (long long)(expr);                                                        \
        long long _exp = (long long)(expected);                                                    \
        if (_got != _exp) {                                                                        \
            fprintf(stderr, "  FAIL %s:%d: %s == 0x%llx, want 0x%llx\n", __FILE__, __LINE__,       \
                    #expr, _got, _exp);                                                            \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

#define ASSERT_TRUE(expr)                                                                          \
    do {                                                                                           \
        tests_run++;                                                                               \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "  FAIL %s:%d: %s is false\n", __FILE__, __LINE__, #expr);             \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

/* ---- Context tests ---- */

static void test_init(void) {
    struct nfs_sp_ctx ctx;
    ASSERT_EQ(nfs_sp_init(&ctx), 0);
    ASSERT_EQ(nfs_sp_init(NULL), -1);
}

static void test_open_close(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    int fd = nfs_sp_open(&ctx);
    ASSERT_TRUE(fd >= 0);
    ASSERT_EQ(nfs_sp_close(&ctx, fd), 0);
}

static void test_close_bad_fd(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    ASSERT_EQ(nfs_sp_close(&ctx, -1), -1);
    ASSERT_EQ(nfs_sp_close(&ctx, 999), -1);
}

/* ---- fd_set tests ---- */

static void test_fdset_zero(void) {
    struct nfs_fdset s;
    s.bits = 0xFFFFFFFF;
    nfs_fdset_zero(&s);
    ASSERT_EQ(s.bits, 0);
}

static void test_fdset_set_isset(void) {
    struct nfs_fdset s;
    nfs_fdset_zero(&s);
    nfs_fdset_set(&s, 0);
    nfs_fdset_set(&s, 5);
    nfs_fdset_set(&s, 63);

    ASSERT_EQ(nfs_fdset_isset(&s, 0), 1);
    ASSERT_EQ(nfs_fdset_isset(&s, 5), 1);
    ASSERT_EQ(nfs_fdset_isset(&s, 63), 1);
    ASSERT_EQ(nfs_fdset_isset(&s, 1), 0);
    ASSERT_EQ(nfs_fdset_isset(&s, 62), 0);
}

static void test_fdset_clr(void) {
    struct nfs_fdset s;
    nfs_fdset_zero(&s);
    nfs_fdset_set(&s, 3);
    ASSERT_EQ(nfs_fdset_isset(&s, 3), 1);
    nfs_fdset_clr(&s, 3);
    ASSERT_EQ(nfs_fdset_isset(&s, 3), 0);
}

static void test_fdset_count(void) {
    struct nfs_fdset s;
    nfs_fdset_zero(&s);
    ASSERT_EQ(nfs_fdset_count(&s), 0);
    nfs_fdset_set(&s, 0);
    nfs_fdset_set(&s, 10);
    nfs_fdset_set(&s, 20);
    ASSERT_EQ(nfs_fdset_count(&s), 3);
}

static void test_fdset_boundary(void) {
    struct nfs_fdset s;
    nfs_fdset_zero(&s);
    /* Out of range should be safe */
    nfs_fdset_set(&s, -1);
    nfs_fdset_set(&s, 100);
    ASSERT_EQ(nfs_fdset_count(&s), 0);
    ASSERT_EQ(nfs_fdset_isset(&s, -1), 0);
    ASSERT_EQ(nfs_fdset_isset(NULL, 0), 0);
}

/* ---- Readiness tests ---- */

static void test_set_get_ready(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    int fd = nfs_sp_open(&ctx);

    ASSERT_EQ(nfs_sp_get_ready(&ctx, fd), 0);
    nfs_sp_set_ready(&ctx, fd, NFS_POLLIN);
    ASSERT_EQ(nfs_sp_get_ready(&ctx, fd), NFS_POLLIN);
    nfs_sp_set_ready(&ctx, fd, NFS_POLLOUT);
    ASSERT_EQ(nfs_sp_get_ready(&ctx, fd), NFS_POLLIN | NFS_POLLOUT);
}

static void test_clear_ready(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    int fd = nfs_sp_open(&ctx);

    nfs_sp_set_ready(&ctx, fd, NFS_POLLIN | NFS_POLLOUT);
    nfs_sp_clear_ready(&ctx, fd, NFS_POLLIN);
    ASSERT_EQ(nfs_sp_get_ready(&ctx, fd), NFS_POLLOUT);
}

static void test_ready_bad_fd(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    ASSERT_EQ(nfs_sp_set_ready(&ctx, -1, NFS_POLLIN), -1);
    ASSERT_EQ(nfs_sp_get_ready(&ctx, -1), -1);
}

/* ---- select() tests ---- */

static void test_select_readable(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    int fd0 = nfs_sp_open(&ctx);
    int fd1 = nfs_sp_open(&ctx);

    nfs_sp_set_ready(&ctx, fd0, NFS_POLLIN);
    /* fd1 is not readable */

    struct nfs_fdset readfds;
    nfs_fdset_zero(&readfds);
    nfs_fdset_set(&readfds, fd0);
    nfs_fdset_set(&readfds, fd1);

    int n = nfs_select(&ctx, fd1 + 1, &readfds, NULL);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(nfs_fdset_isset(&readfds, fd0), 1);
    ASSERT_EQ(nfs_fdset_isset(&readfds, fd1), 0);
}

static void test_select_writable(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    int fd = nfs_sp_open(&ctx);

    nfs_sp_set_ready(&ctx, fd, NFS_POLLOUT);

    struct nfs_fdset writefds;
    nfs_fdset_zero(&writefds);
    nfs_fdset_set(&writefds, fd);

    int n = nfs_select(&ctx, fd + 1, NULL, &writefds);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(nfs_fdset_isset(&writefds, fd), 1);
}

static void test_select_none_ready(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    int fd = nfs_sp_open(&ctx);

    struct nfs_fdset readfds;
    nfs_fdset_zero(&readfds);
    nfs_fdset_set(&readfds, fd);

    int n = nfs_select(&ctx, fd + 1, &readfds, NULL);
    ASSERT_EQ(n, 0);
    ASSERT_EQ(nfs_fdset_isset(&readfds, fd), 0);
}

static void test_select_both(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    int fd = nfs_sp_open(&ctx);

    nfs_sp_set_ready(&ctx, fd, NFS_POLLIN | NFS_POLLOUT);

    struct nfs_fdset readfds, writefds;
    nfs_fdset_zero(&readfds);
    nfs_fdset_zero(&writefds);
    nfs_fdset_set(&readfds, fd);
    nfs_fdset_set(&writefds, fd);

    int n = nfs_select(&ctx, fd + 1, &readfds, &writefds);
    ASSERT_EQ(n, 1); /* 1 fd that is ready */
    ASSERT_EQ(nfs_fdset_isset(&readfds, fd), 1);
    ASSERT_EQ(nfs_fdset_isset(&writefds, fd), 1);
}

static void test_select_null_ctx(void) {
    ASSERT_EQ(nfs_select(NULL, 1, NULL, NULL), -1);
}

/* ---- poll() tests ---- */

static void test_poll_basic(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    int fd = nfs_sp_open(&ctx);

    nfs_sp_set_ready(&ctx, fd, NFS_POLLIN);

    struct nfs_pollfd pfd;
    nfs_pollfd_init(&pfd, fd, NFS_POLLIN | NFS_POLLOUT);

    int n = nfs_poll(&ctx, &pfd, 1);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(nfs_poll_check_ready(&pfd, NFS_POLLIN), 1);
    ASSERT_EQ(nfs_poll_check_ready(&pfd, NFS_POLLOUT), 0);
}

static void test_poll_multiple(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    int fd0 = nfs_sp_open(&ctx);
    int fd1 = nfs_sp_open(&ctx);
    int fd2 = nfs_sp_open(&ctx);

    nfs_sp_set_ready(&ctx, fd0, NFS_POLLIN);
    nfs_sp_set_ready(&ctx, fd2, NFS_POLLOUT);

    struct nfs_pollfd pfds[3];
    nfs_pollfd_init(&pfds[0], fd0, NFS_POLLIN);
    nfs_pollfd_init(&pfds[1], fd1, NFS_POLLIN);
    nfs_pollfd_init(&pfds[2], fd2, NFS_POLLOUT);

    int n = nfs_poll(&ctx, pfds, 3);
    ASSERT_EQ(n, 2);
    ASSERT_EQ(pfds[0].revents, NFS_POLLIN);
    ASSERT_EQ(pfds[1].revents, 0);
    ASSERT_EQ(pfds[2].revents, NFS_POLLOUT);
}

static void test_poll_err_always_reported(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    int fd = nfs_sp_open(&ctx);

    /* Set POLLERR but only request POLLIN */
    nfs_sp_set_ready(&ctx, fd, NFS_POLLERR);

    struct nfs_pollfd pfd;
    nfs_pollfd_init(&pfd, fd, NFS_POLLIN);

    nfs_poll(&ctx, &pfd, 1);
    ASSERT_EQ(nfs_poll_check_ready(&pfd, NFS_POLLERR), 1);
}

static void test_poll_invalid_fd(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);

    struct nfs_pollfd pfd;
    nfs_pollfd_init(&pfd, 999, NFS_POLLIN);

    int n = nfs_poll(&ctx, &pfd, 1);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(pfd.revents, NFS_POLLNVAL);
}

static void test_poll_negative_fd(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);

    struct nfs_pollfd pfd;
    nfs_pollfd_init(&pfd, -1, NFS_POLLIN);

    int n = nfs_poll(&ctx, &pfd, 1);
    ASSERT_EQ(n, 0);
    ASSERT_EQ(pfd.revents, 0);
}

static void test_poll_null(void) {
    struct nfs_sp_ctx ctx;
    nfs_sp_init(&ctx);
    ASSERT_EQ(nfs_poll(&ctx, NULL, 1), -1);
    ASSERT_EQ(nfs_poll(NULL, NULL, 0), -1);
}

/* ---- pollfd_init test ---- */

static void test_pollfd_init(void) {
    struct nfs_pollfd pfd;
    nfs_pollfd_init(&pfd, 7, NFS_POLLIN | NFS_POLLOUT);
    ASSERT_EQ(pfd.fd, 7);
    ASSERT_EQ(pfd.events, NFS_POLLIN | NFS_POLLOUT);
    ASSERT_EQ(pfd.revents, 0);
}

/* ---- Event name test ---- */

static void test_event_names(void) {
    ASSERT_TRUE(strcmp(nfs_poll_event_name(NFS_POLLIN), "POLLIN") == 0);
    ASSERT_TRUE(strcmp(nfs_poll_event_name(NFS_POLLOUT), "POLLOUT") == 0);
    ASSERT_TRUE(strcmp(nfs_poll_event_name(NFS_POLLERR), "POLLERR") == 0);
    ASSERT_TRUE(strcmp(nfs_poll_event_name(NFS_POLLHUP), "POLLHUP") == 0);
    ASSERT_TRUE(strcmp(nfs_poll_event_name(NFS_POLLNVAL), "POLLNVAL") == 0);
    ASSERT_TRUE(strcmp(nfs_poll_event_name(0xFFFF), "UNKNOWN") == 0);
}

/* ---- poll_check_ready null ---- */

static void test_poll_check_null(void) {
    ASSERT_EQ(nfs_poll_check_ready(NULL, NFS_POLLIN), 0);
}

int main(void) {
    printf("select() and poll() Tests\n");

    test_init();
    test_open_close();
    test_close_bad_fd();
    test_fdset_zero();
    test_fdset_set_isset();
    test_fdset_clr();
    test_fdset_count();
    test_fdset_boundary();
    test_set_get_ready();
    test_clear_ready();
    test_ready_bad_fd();
    test_select_readable();
    test_select_writable();
    test_select_none_ready();
    test_select_both();
    test_select_null_ctx();
    test_poll_basic();
    test_poll_multiple();
    test_poll_err_always_reported();
    test_poll_invalid_fd();
    test_poll_negative_fd();
    test_poll_null();
    test_pollfd_init();
    test_event_names();
    test_poll_check_null();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
