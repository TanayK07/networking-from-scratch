/* Unit tests for Blocking vs Nonblocking I/O mock. */

#include "../nonblock.h"
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

/* ---- Init and open tests ---- */

static void test_init(void) {
    struct nfs_nb_ctx ctx;
    ASSERT_EQ(nfs_nb_init(&ctx), NFS_NB_OK);
    ASSERT_EQ(nfs_nb_init(NULL), NFS_NB_EINVAL);
}

static void test_open_close(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);

    int fd = nfs_nb_open(&ctx);
    ASSERT_TRUE(fd >= 0);
    ASSERT_EQ(ctx.fds[fd].active, 1);

    ASSERT_EQ(nfs_nb_close(&ctx, fd), NFS_NB_OK);
    ASSERT_EQ(ctx.fds[fd].active, 0);
}

static void test_open_multiple(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);

    int fd0 = nfs_nb_open(&ctx);
    int fd1 = nfs_nb_open(&ctx);
    int fd2 = nfs_nb_open(&ctx);
    ASSERT_TRUE(fd0 != fd1);
    ASSERT_TRUE(fd1 != fd2);
    ASSERT_TRUE(fd0 >= 0 && fd1 >= 0 && fd2 >= 0);
}

static void test_close_bad_fd(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    ASSERT_EQ(nfs_nb_close(&ctx, -1), NFS_NB_EBADF);
    ASSERT_EQ(nfs_nb_close(&ctx, 999), NFS_NB_EBADF);
}

/* ---- Nonblocking flag tests ---- */

static void test_default_blocking(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);
    ASSERT_EQ(nfs_nb_is_nonblock(&ctx, fd), 0);
}

static void test_set_nonblock(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);

    ASSERT_EQ(nfs_nb_set_nonblock(&ctx, fd, 1), NFS_NB_OK);
    ASSERT_EQ(nfs_nb_is_nonblock(&ctx, fd), 1);

    ASSERT_EQ(nfs_nb_set_nonblock(&ctx, fd, 0), NFS_NB_OK);
    ASSERT_EQ(nfs_nb_is_nonblock(&ctx, fd), 0);
}

static void test_set_nonblock_bad_fd(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    ASSERT_EQ(nfs_nb_set_nonblock(&ctx, -1, 1), NFS_NB_EBADF);
    ASSERT_EQ(nfs_nb_is_nonblock(&ctx, -1), -1);
}

/* ---- Read tests ---- */

static void test_read_no_data(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);

    uint8_t buf[64];
    ASSERT_EQ(nfs_nb_read(&ctx, fd, buf, sizeof(buf)), NFS_NB_EAGAIN);
}

static void test_read_with_data(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);

    nfs_nb_inject_readable(&ctx, fd, (const uint8_t *)"Hello", 5);

    uint8_t buf[64];
    int rc = nfs_nb_read(&ctx, fd, buf, sizeof(buf));
    ASSERT_EQ(rc, 5);
    ASSERT_TRUE(memcmp(buf, "Hello", 5) == 0);
}

static void test_read_partial(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);

    nfs_nb_inject_readable(&ctx, fd, (const uint8_t *)"ABCDEF", 6);

    /* Read only 3 bytes */
    uint8_t buf[3];
    int rc = nfs_nb_read(&ctx, fd, buf, 3);
    ASSERT_EQ(rc, 3);
    ASSERT_TRUE(memcmp(buf, "ABC", 3) == 0);

    /* Read remaining 3 bytes */
    rc = nfs_nb_read(&ctx, fd, buf, 3);
    ASSERT_EQ(rc, 3);
    ASSERT_TRUE(memcmp(buf, "DEF", 3) == 0);

    /* No more data */
    ASSERT_EQ(nfs_nb_read(&ctx, fd, buf, 3), NFS_NB_EAGAIN);
}

static void test_read_closed_fd(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);
    nfs_nb_close(&ctx, fd);

    uint8_t buf[64];
    ASSERT_EQ(nfs_nb_read(&ctx, fd, buf, sizeof(buf)), NFS_NB_CLOSED);
}

static void test_read_null_buf(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);
    ASSERT_EQ(nfs_nb_read(&ctx, fd, NULL, 10), NFS_NB_EINVAL);
}

/* ---- Write tests ---- */

static void test_write_writable(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);

    int rc = nfs_nb_write(&ctx, fd, (const uint8_t *)"data", 4);
    ASSERT_EQ(rc, 4);
    ASSERT_EQ(ctx.fds[fd].write_total, 4);
}

static void test_write_not_writable(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);

    nfs_nb_set_writable(&ctx, fd, 0);
    ASSERT_EQ(nfs_nb_write(&ctx, fd, (const uint8_t *)"data", 4), NFS_NB_EAGAIN);
}

static void test_write_closed(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);
    nfs_nb_close(&ctx, fd);
    ASSERT_EQ(nfs_nb_write(&ctx, fd, (const uint8_t *)"x", 1), NFS_NB_CLOSED);
}

static void test_write_zero(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);
    ASSERT_EQ(nfs_nb_write(&ctx, fd, (const uint8_t *)"", 0), 0);
}

/* ---- Would-block tests ---- */

static void test_would_block_blocking_mode(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);
    /* In blocking mode, would_block returns 0 */
    ASSERT_EQ(nfs_nb_would_block(&ctx, fd), 0);
}

static void test_would_block_nonblock_no_data(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);
    nfs_nb_set_nonblock(&ctx, fd, 1);
    ASSERT_EQ(nfs_nb_would_block(&ctx, fd), 1);
}

static void test_would_block_nonblock_with_data(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);
    nfs_nb_set_nonblock(&ctx, fd, 1);
    nfs_nb_inject_readable(&ctx, fd, (const uint8_t *)"x", 1);
    ASSERT_EQ(nfs_nb_would_block(&ctx, fd), 0);
}

/* ---- Retry loop tests ---- */

static void test_retry_no_data(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);
    nfs_nb_set_nonblock(&ctx, fd, 1);

    uint8_t buf[64];
    ASSERT_EQ(nfs_nb_read_retry(&ctx, fd, buf, sizeof(buf), 3), NFS_NB_EAGAIN);
}

static void test_retry_with_data(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);

    nfs_nb_inject_readable(&ctx, fd, (const uint8_t *)"OK", 2);

    uint8_t buf[64];
    int rc = nfs_nb_read_retry(&ctx, fd, buf, sizeof(buf), 3);
    ASSERT_EQ(rc, 2);
}

/* ---- Inject tests ---- */

static void test_inject_bad_fd(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    ASSERT_EQ(nfs_nb_inject_readable(&ctx, -1, (const uint8_t *)"x", 1), NFS_NB_EBADF);
}

static void test_inject_null_data(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);
    ASSERT_EQ(nfs_nb_inject_readable(&ctx, fd, NULL, 5), NFS_NB_EINVAL);
}

/* ---- Total tracking ---- */

static void test_read_write_totals(void) {
    struct nfs_nb_ctx ctx;
    nfs_nb_init(&ctx);
    int fd = nfs_nb_open(&ctx);

    nfs_nb_inject_readable(&ctx, fd, (const uint8_t *)"ABC", 3);
    uint8_t buf[64];
    nfs_nb_read(&ctx, fd, buf, sizeof(buf));
    ASSERT_EQ(ctx.fds[fd].read_total, 3);

    nfs_nb_write(&ctx, fd, (const uint8_t *)"12345", 5);
    nfs_nb_write(&ctx, fd, (const uint8_t *)"67", 2);
    ASSERT_EQ(ctx.fds[fd].write_total, 7);
}

int main(void) {
    printf("Blocking vs Nonblocking I/O Tests\n");

    test_init();
    test_open_close();
    test_open_multiple();
    test_close_bad_fd();
    test_default_blocking();
    test_set_nonblock();
    test_set_nonblock_bad_fd();
    test_read_no_data();
    test_read_with_data();
    test_read_partial();
    test_read_closed_fd();
    test_read_null_buf();
    test_write_writable();
    test_write_not_writable();
    test_write_closed();
    test_write_zero();
    test_would_block_blocking_mode();
    test_would_block_nonblock_no_data();
    test_would_block_nonblock_with_data();
    test_retry_no_data();
    test_retry_with_data();
    test_inject_bad_fd();
    test_inject_null_data();
    test_read_write_totals();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
