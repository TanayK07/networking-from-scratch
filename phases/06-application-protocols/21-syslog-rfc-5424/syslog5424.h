#ifndef NFS_SYSLOG5424_H
#define NFS_SYSLOG5424_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Syslog (RFC 5424)
 *
 * Message format:
 *   <PRI>VERSION SP TIMESTAMP SP HOSTNAME SP APP-NAME SP PROCID SP MSGID SP STRUCTURED-DATA SP MSG
 *
 * PRI = facility * 8 + severity
 * --------------------------------------------------------------- */

/* Facilities */
#define NFS_SYSLOG_KERN     0
#define NFS_SYSLOG_USER     1
#define NFS_SYSLOG_MAIL     2
#define NFS_SYSLOG_DAEMON   3
#define NFS_SYSLOG_AUTH     4
#define NFS_SYSLOG_SYSLOG   5
#define NFS_SYSLOG_LPR      6
#define NFS_SYSLOG_NEWS     7
#define NFS_SYSLOG_UUCP     8
#define NFS_SYSLOG_CRON     9
#define NFS_SYSLOG_AUTHPRIV 10
#define NFS_SYSLOG_FTP      11
#define NFS_SYSLOG_NTP      12
#define NFS_SYSLOG_AUDIT    13
#define NFS_SYSLOG_ALERT    14
#define NFS_SYSLOG_CLOCK    15
#define NFS_SYSLOG_LOCAL0   16
#define NFS_SYSLOG_LOCAL1   17
#define NFS_SYSLOG_LOCAL2   18
#define NFS_SYSLOG_LOCAL3   19
#define NFS_SYSLOG_LOCAL4   20
#define NFS_SYSLOG_LOCAL5   21
#define NFS_SYSLOG_LOCAL6   22
#define NFS_SYSLOG_LOCAL7   23

/* Severities */
#define NFS_SYSLOG_EMERG     0
#define NFS_SYSLOG_ALERT_SEV 1
#define NFS_SYSLOG_CRIT      2
#define NFS_SYSLOG_ERR       3
#define NFS_SYSLOG_WARNING   4
#define NFS_SYSLOG_NOTICE    5
#define NFS_SYSLOG_INFO      6
#define NFS_SYSLOG_DEBUG     7

/* Field size limits */
#define NFS_SYSLOG_MAX_HOSTNAME  255
#define NFS_SYSLOG_MAX_APPNAME   48
#define NFS_SYSLOG_MAX_PROCID    128
#define NFS_SYSLOG_MAX_MSGID     32
#define NFS_SYSLOG_MAX_SD        1024
#define NFS_SYSLOG_MAX_MSG       2048
#define NFS_SYSLOG_MAX_TIMESTAMP 48

/* Parsed syslog message */
struct nfs_syslog_msg {
    uint8_t facility;
    uint8_t severity;
    uint8_t version;
    char timestamp[NFS_SYSLOG_MAX_TIMESTAMP];
    char hostname[NFS_SYSLOG_MAX_HOSTNAME];
    char app_name[NFS_SYSLOG_MAX_APPNAME];
    char procid[NFS_SYSLOG_MAX_PROCID];
    char msgid[NFS_SYSLOG_MAX_MSGID];
    char sd[NFS_SYSLOG_MAX_SD]; /* structured data (raw) */
    char msg[NFS_SYSLOG_MAX_MSG];
};

/* Encode PRI value from facility and severity. */
static inline int nfs_syslog_pri_encode(uint8_t facility, uint8_t severity) {
    if (facility > 23 || severity > 7)
        return -1;
    return (int)(facility * 8 + severity);
}

/* Decode PRI value into facility and severity. */
static inline int nfs_syslog_pri_decode(int pri, uint8_t *facility, uint8_t *severity) {
    if (pri < 0 || pri > 191 || !facility || !severity)
        return -1;
    *facility = (uint8_t)(pri / 8);
    *severity = (uint8_t)(pri % 8);
    return 0;
}

/* Build a syslog message string.
 * Returns bytes written (excluding NUL), or -1 on error. */
int nfs_syslog_build(char *buf, size_t out_sz, const struct nfs_syslog_msg *msg);

/* Parse a syslog message string.
 * Returns 0 on success, -1 on error. */
int nfs_syslog_parse(const char *buf, size_t len, struct nfs_syslog_msg *out);

/* Get human-readable facility name. */
const char *nfs_syslog_facility_name(uint8_t facility);

/* Get human-readable severity name. */
const char *nfs_syslog_severity_name(uint8_t severity);

#endif /* NFS_SYSLOG5424_H */
