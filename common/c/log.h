#ifndef NFS_LOG_H
#define NFS_LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

/* Minimal logging used across lessons. */

#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3

#ifndef NFS_LOG_LEVEL
#define NFS_LOG_LEVEL LOG_INFO
#endif

static inline void nfs_log(int level, const char *prefix, const char *file, int line,
                           const char *fmt, ...) {
    if (level < NFS_LOG_LEVEL)
        return;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    fprintf(stderr, "[%ld.%06ld] [%s] %s:%d ", ts.tv_sec, ts.tv_nsec / 1000, prefix, file, line);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
}

#define LOG_D(...) nfs_log(LOG_DEBUG, "DEBUG", __FILE__, __LINE__, __VA_ARGS__)
#define LOG_I(...) nfs_log(LOG_INFO, "INFO", __FILE__, __LINE__, __VA_ARGS__)
#define LOG_W(...) nfs_log(LOG_WARN, "WARN", __FILE__, __LINE__, __VA_ARGS__)
#define LOG_E(...) nfs_log(LOG_ERROR, "ERROR", __FILE__, __LINE__, __VA_ARGS__)

#endif /* NFS_LOG_H */
