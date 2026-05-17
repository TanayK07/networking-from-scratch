#include "syslog5424.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[4096];

    /* Build a syslog message */
    struct nfs_syslog_msg msg;
    memset(&msg, 0, sizeof(msg));
    msg.facility = NFS_SYSLOG_LOCAL0;
    msg.severity = NFS_SYSLOG_INFO;
    msg.version = 1;
    strncpy(msg.timestamp, "2023-11-14T12:00:00.000Z", sizeof(msg.timestamp) - 1);
    strncpy(msg.hostname, "myhost", sizeof(msg.hostname) - 1);
    strncpy(msg.app_name, "myapp", sizeof(msg.app_name) - 1);
    strncpy(msg.procid, "1234", sizeof(msg.procid) - 1);
    strncpy(msg.msgid, "ID47", sizeof(msg.msgid) - 1);
    strncpy(msg.msg, "Application started successfully", sizeof(msg.msg) - 1);

    int n = nfs_syslog_build(buf, sizeof(buf), &msg);
    if (n > 0) {
        printf("Built syslog message (%d bytes):\n  %s\n\n", n, buf);
    }

    /* Parse it back */
    struct nfs_syslog_msg parsed;
    if (nfs_syslog_parse(buf, (size_t)n, &parsed) == 0) {
        printf("Parsed syslog message:\n");
        printf("  Facility: %s (%u)\n", nfs_syslog_facility_name(parsed.facility), parsed.facility);
        printf("  Severity: %s (%u)\n", nfs_syslog_severity_name(parsed.severity), parsed.severity);
        printf("  PRI:      %d\n", nfs_syslog_pri_encode(parsed.facility, parsed.severity));
        printf("  Version:  %u\n", parsed.version);
        printf("  Timestamp: %s\n", parsed.timestamp);
        printf("  Hostname: %s\n", parsed.hostname);
        printf("  App-Name: %s\n", parsed.app_name);
        printf("  ProcID:   %s\n", parsed.procid);
        printf("  MsgID:    %s\n", parsed.msgid);
        printf("  SD:       %s\n", parsed.sd[0] ? parsed.sd : "(none)");
        printf("  MSG:      %s\n", parsed.msg);
    }

    /* Build with structured data */
    printf("\n=== With Structured Data ===\n");
    memset(&msg, 0, sizeof(msg));
    msg.facility = NFS_SYSLOG_AUTH;
    msg.severity = NFS_SYSLOG_WARNING;
    msg.version = 1;
    strncpy(msg.timestamp, "2023-11-14T12:01:00Z", sizeof(msg.timestamp) - 1);
    strncpy(msg.hostname, "firewall", sizeof(msg.hostname) - 1);
    strncpy(msg.app_name, "sshd", sizeof(msg.app_name) - 1);
    strncpy(msg.procid, "5678", sizeof(msg.procid) - 1);
    strncpy(msg.sd, "[origin ip=\"192.168.1.1\"]", sizeof(msg.sd) - 1);
    strncpy(msg.msg, "Failed login attempt", sizeof(msg.msg) - 1);

    n = nfs_syslog_build(buf, sizeof(buf), &msg);
    if (n > 0) {
        printf("  %s\n", buf);
    }

    return 0;
}
