#include "syslog5424.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Build a RFC 5424 syslog message.
 * Format: <PRI>VERSION SP TIMESTAMP SP HOSTNAME SP APP-NAME SP PROCID SP MSGID SP SD SP MSG
 * --------------------------------------------------------------- */

int nfs_syslog_build(char *buf, size_t out_sz, const struct nfs_syslog_msg *msg) {
    if (!buf || !msg || out_sz == 0)
        return -1;

    int pri = nfs_syslog_pri_encode(msg->facility, msg->severity);
    if (pri < 0)
        return -1;

    /* Use "-" for nil values as per RFC 5424 */
    const char *ts = (msg->timestamp[0] != '\0') ? msg->timestamp : "-";
    const char *host = (msg->hostname[0] != '\0') ? msg->hostname : "-";
    const char *app = (msg->app_name[0] != '\0') ? msg->app_name : "-";
    const char *pid = (msg->procid[0] != '\0') ? msg->procid : "-";
    const char *mid = (msg->msgid[0] != '\0') ? msg->msgid : "-";
    const char *sd = (msg->sd[0] != '\0') ? msg->sd : "-";

    int n;
    if (msg->msg[0] != '\0') {
        n = snprintf(buf, out_sz, "<%d>%u %s %s %s %s %s %s %s", pri, msg->version, ts, host, app,
                     pid, mid, sd, msg->msg);
    } else {
        n = snprintf(buf, out_sz, "<%d>%u %s %s %s %s %s %s", pri, msg->version, ts, host, app, pid,
                     mid, sd);
    }

    if (n < 0 || (size_t)n >= out_sz)
        return -1;

    return n;
}

/* ---------------------------------------------------------------
 * Parse a RFC 5424 syslog message.
 * --------------------------------------------------------------- */

/* Helper: read a token delimited by space, advance *pos.
 * Copies at most max-1 chars into dst. */
static int read_token(const char *buf, size_t len, size_t *pos, char *dst, size_t max) {
    size_t start = *pos;
    while (*pos < len && buf[*pos] != ' ')
        (*pos)++;

    size_t tlen = *pos - start;
    if (tlen == 0)
        return -1;

    if (tlen >= max)
        tlen = max - 1;
    memcpy(dst, buf + start, tlen);
    dst[tlen] = '\0';

    /* Skip the space delimiter */
    if (*pos < len && buf[*pos] == ' ')
        (*pos)++;

    return 0;
}

int nfs_syslog_parse(const char *buf, size_t len, struct nfs_syslog_msg *out) {
    if (!buf || !out || len == 0)
        return -1;

    memset(out, 0, sizeof(*out));

    size_t pos = 0;

    /* Parse <PRI> */
    if (buf[pos] != '<')
        return -1;
    pos++;

    /* Read PRI digits */
    char pri_str[8];
    size_t pi = 0;
    while (pos < len && buf[pos] != '>' && pi < sizeof(pri_str) - 1) {
        pri_str[pi++] = buf[pos++];
    }
    pri_str[pi] = '\0';

    if (pos >= len || buf[pos] != '>')
        return -1;
    pos++; /* skip '>' */

    int pri = atoi(pri_str);
    if (nfs_syslog_pri_decode(pri, &out->facility, &out->severity) < 0)
        return -1;

    /* Parse VERSION (digits until space) */
    char ver_str[8];
    size_t vi = 0;
    while (pos < len && buf[pos] != ' ' && vi < sizeof(ver_str) - 1) {
        ver_str[vi++] = buf[pos++];
    }
    ver_str[vi] = '\0';
    out->version = (uint8_t)atoi(ver_str);

    if (pos < len && buf[pos] == ' ')
        pos++;

    /* Parse remaining tokens: TIMESTAMP HOSTNAME APP-NAME PROCID MSGID */
    if (read_token(buf, len, &pos, out->timestamp, sizeof(out->timestamp)) < 0)
        return -1;
    if (read_token(buf, len, &pos, out->hostname, sizeof(out->hostname)) < 0)
        return -1;
    if (read_token(buf, len, &pos, out->app_name, sizeof(out->app_name)) < 0)
        return -1;
    if (read_token(buf, len, &pos, out->procid, sizeof(out->procid)) < 0)
        return -1;
    if (read_token(buf, len, &pos, out->msgid, sizeof(out->msgid)) < 0)
        return -1;

    /* Parse STRUCTURED-DATA: either "-" or "[...]" (possibly multiple) */
    if (pos < len) {
        if (buf[pos] == '[') {
            /* Read all structured data elements */
            size_t sd_start = pos;
            while (pos < len && buf[pos] == '[') {
                /* Find matching ] */
                while (pos < len && buf[pos] != ']')
                    pos++;
                if (pos < len)
                    pos++; /* skip ']' */
            }
            size_t sd_len = pos - sd_start;
            if (sd_len >= sizeof(out->sd))
                sd_len = sizeof(out->sd) - 1;
            memcpy(out->sd, buf + sd_start, sd_len);
            out->sd[sd_len] = '\0';

            if (pos < len && buf[pos] == ' ')
                pos++;
        } else {
            /* Read token (likely "-") */
            if (read_token(buf, len, &pos, out->sd, sizeof(out->sd)) < 0)
                return -1;
        }
    }

    /* Remaining is MSG */
    if (pos < len) {
        size_t msg_len = len - pos;
        if (msg_len >= sizeof(out->msg))
            msg_len = sizeof(out->msg) - 1;
        memcpy(out->msg, buf + pos, msg_len);
        out->msg[msg_len] = '\0';
    }

    /* Replace "-" tokens with empty strings */
    if (strcmp(out->timestamp, "-") == 0)
        out->timestamp[0] = '\0';
    if (strcmp(out->hostname, "-") == 0)
        out->hostname[0] = '\0';
    if (strcmp(out->app_name, "-") == 0)
        out->app_name[0] = '\0';
    if (strcmp(out->procid, "-") == 0)
        out->procid[0] = '\0';
    if (strcmp(out->msgid, "-") == 0)
        out->msgid[0] = '\0';
    if (strcmp(out->sd, "-") == 0)
        out->sd[0] = '\0';

    return 0;
}

/* ---------------------------------------------------------------
 * Name lookups
 * --------------------------------------------------------------- */

const char *nfs_syslog_facility_name(uint8_t facility) {
    static const char *names[] = {"kern",   "user",   "mail",   "daemon", "auth",     "syslog",
                                  "lpr",    "news",   "uucp",   "cron",   "authpriv", "ftp",
                                  "ntp",    "audit",  "alert",  "clock",  "local0",   "local1",
                                  "local2", "local3", "local4", "local5", "local6",   "local7"};
    if (facility > 23)
        return "unknown";
    return names[facility];
}

const char *nfs_syslog_severity_name(uint8_t severity) {
    static const char *names[] = {"emerg",   "alert",  "crit", "err",
                                  "warning", "notice", "info", "debug"};
    if (severity > 7)
        return "unknown";
    return names[severity];
}
