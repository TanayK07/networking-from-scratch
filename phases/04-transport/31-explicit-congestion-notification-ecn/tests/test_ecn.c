/* Unit tests for ECN (Explicit Congestion Notification). */

#include "../ecn.h"
#include <arpa/inet.h>
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

/* ---- IP ECN get/set tests ---- */

static void test_ecn_get_not_ect(void) {
    ASSERT_EQ(nfs_ecn_get_ip(0x00), NFS_ECN_NOT_ECT);
    ASSERT_EQ(nfs_ecn_get_ip(0xFC), NFS_ECN_NOT_ECT); /* DSCP all 1s, ECN 00 */
}

static void test_ecn_get_ect0(void) {
    ASSERT_EQ(nfs_ecn_get_ip(0x02), NFS_ECN_ECT0);
    ASSERT_EQ(nfs_ecn_get_ip(0xB2), NFS_ECN_ECT0); /* DSCP EF + ECT(0) */
}

static void test_ecn_get_ect1(void) {
    ASSERT_EQ(nfs_ecn_get_ip(0x01), NFS_ECN_ECT1);
}

static void test_ecn_get_ce(void) {
    ASSERT_EQ(nfs_ecn_get_ip(0x03), NFS_ECN_CE);
    ASSERT_EQ(nfs_ecn_get_ip(0xFF), NFS_ECN_CE);
}

static void test_ecn_set_preserves_dscp(void) {
    uint8_t tos = nfs_ecn_set_ip(0xB8, NFS_ECN_ECT0); /* EF DSCP = 0xB8 */
    ASSERT_EQ(tos, 0xBA);                             /* 0xB8 | 0x02 */
    ASSERT_EQ(tos & 0xFC, 0xB8);                      /* DSCP preserved */
    ASSERT_EQ(nfs_ecn_get_ip(tos), NFS_ECN_ECT0);
}

static void test_ecn_set_all_codepoints(void) {
    uint8_t base = 0x20; /* some DSCP */
    ASSERT_EQ(nfs_ecn_set_ip(base, NFS_ECN_NOT_ECT), 0x20);
    ASSERT_EQ(nfs_ecn_set_ip(base, NFS_ECN_ECT1), 0x21);
    ASSERT_EQ(nfs_ecn_set_ip(base, NFS_ECN_ECT0), 0x22);
    ASSERT_EQ(nfs_ecn_set_ip(base, NFS_ECN_CE), 0x23);
}

static void test_ecn_set_overwrites_old(void) {
    uint8_t tos = nfs_ecn_set_ip(0x03, NFS_ECN_NOT_ECT); /* CE -> Not-ECT */
    ASSERT_EQ(nfs_ecn_get_ip(tos), NFS_ECN_NOT_ECT);
}

/* ---- ECT / CE checks ---- */

static void test_is_ect(void) {
    ASSERT_EQ(nfs_ecn_is_ect(0x02), 1); /* ECT(0) */
    ASSERT_EQ(nfs_ecn_is_ect(0x01), 1); /* ECT(1) */
    ASSERT_EQ(nfs_ecn_is_ect(0x00), 0); /* Not-ECT */
    ASSERT_EQ(nfs_ecn_is_ect(0x03), 0); /* CE is not ECT */
}

static void test_is_ce(void) {
    ASSERT_EQ(nfs_ecn_is_ce(0x03), 1);
    ASSERT_EQ(nfs_ecn_is_ce(0x02), 0);
    ASSERT_EQ(nfs_ecn_is_ce(0x00), 0);
}

static void test_mark_ce_from_ect0(void) {
    int result = nfs_ecn_mark_ce(0xB2); /* EF DSCP + ECT(0) */
    ASSERT_TRUE(result >= 0);
    ASSERT_EQ(nfs_ecn_get_ip((uint8_t)result), NFS_ECN_CE);
    ASSERT_EQ((uint8_t)result & 0xFC, 0xB0); /* DSCP preserved */
}

static void test_mark_ce_from_ect1(void) {
    int result = nfs_ecn_mark_ce(0x01);
    ASSERT_TRUE(result >= 0);
    ASSERT_EQ(nfs_ecn_get_ip((uint8_t)result), NFS_ECN_CE);
}

static void test_mark_ce_from_not_ect(void) {
    ASSERT_EQ(nfs_ecn_mark_ce(0x00), -1); /* Can't mark non-ECT */
}

static void test_mark_ce_from_ce(void) {
    ASSERT_EQ(nfs_ecn_mark_ce(0x03), -1); /* Already CE, not ECT */
}

/* ---- TCP flag operations ---- */

static void test_tcp_get_flags(void) {
    /* data_off=5 (20 bytes), SYN flag set: 0x5002 in host order */
    uint16_t dof = htons(0x5002);
    uint16_t flags = nfs_ecn_tcp_get_flags(dof);
    ASSERT_EQ(flags & NFS_TCP_FLAG_SYN, NFS_TCP_FLAG_SYN);
    ASSERT_EQ(flags & NFS_TCP_FLAG_ACK, 0);
}

static void test_tcp_set_flags(void) {
    uint16_t dof = htons(0x5002); /* data_off=5, SYN */
    dof = nfs_ecn_tcp_set_flags(dof, NFS_TCP_FLAG_ECE | NFS_TCP_FLAG_CWR);
    uint16_t flags = nfs_ecn_tcp_get_flags(dof);
    ASSERT_TRUE(flags & NFS_TCP_FLAG_SYN);
    ASSERT_TRUE(flags & NFS_TCP_FLAG_ECE);
    ASSERT_TRUE(flags & NFS_TCP_FLAG_CWR);
}

static void test_tcp_clear_flags(void) {
    uint16_t dof = htons(0x50C2); /* data_off=5, SYN+ECE+CWR */
    dof = nfs_ecn_tcp_clear_flags(dof, NFS_TCP_FLAG_CWR);
    uint16_t flags = nfs_ecn_tcp_get_flags(dof);
    ASSERT_TRUE(flags & NFS_TCP_FLAG_ECE);
    ASSERT_TRUE(!(flags & NFS_TCP_FLAG_CWR));
}

static void test_tcp_has_flags(void) {
    uint16_t dof = htons(0x50C2); /* SYN+ECE+CWR */
    ASSERT_EQ(nfs_ecn_tcp_has_flags(dof, NFS_TCP_FLAG_ECE | NFS_TCP_FLAG_CWR), 1);
    ASSERT_EQ(nfs_ecn_tcp_has_flags(dof, NFS_TCP_FLAG_ACK), 0);
}

static void test_tcp_ns_flag(void) {
    uint16_t dof = htons(0x5102); /* NS + SYN */
    uint16_t flags = nfs_ecn_tcp_get_flags(dof);
    ASSERT_TRUE(flags & NFS_TCP_FLAG_NS);
}

/* ---- ECN negotiation tests ---- */

static void test_nego_init(void) {
    struct nfs_ecn_negotiation nego;
    nfs_ecn_nego_init(&nego);
    ASSERT_EQ(nego.state, NFS_ECN_STATE_UNKNOWN);
    ASSERT_EQ(nego.ecn_capable, 0);
}

static void test_nego_syn_with_ecn(void) {
    struct nfs_ecn_negotiation nego;
    nfs_ecn_nego_init(&nego);
    uint16_t syn_flags = nfs_ecn_build_syn_flags();
    nfs_ecn_state_t st = nfs_ecn_nego_process(&nego, syn_flags, 0);
    ASSERT_EQ(st, NFS_ECN_STATE_REQUESTED);
}

static void test_nego_synack_accept(void) {
    struct nfs_ecn_negotiation nego;
    nfs_ecn_nego_init(&nego);
    uint16_t flags = nfs_ecn_build_synack_accept_flags();
    nfs_ecn_state_t st = nfs_ecn_nego_process(&nego, flags, 1);
    ASSERT_EQ(st, NFS_ECN_STATE_ACCEPTED);
    ASSERT_EQ(nego.ecn_capable, 1);
}

static void test_nego_synack_reject(void) {
    struct nfs_ecn_negotiation nego;
    nfs_ecn_nego_init(&nego);
    uint16_t flags = nfs_ecn_build_synack_reject_flags();
    nfs_ecn_state_t st = nfs_ecn_nego_process(&nego, flags, 1);
    ASSERT_EQ(st, NFS_ECN_STATE_REJECTED);
    ASSERT_EQ(nego.ecn_capable, 0);
}

static void test_nego_null(void) {
    ASSERT_EQ(nfs_ecn_nego_process(NULL, 0, 0), NFS_ECN_STATE_UNKNOWN);
}

/* ---- Name tests ---- */

static void test_codepoint_names(void) {
    ASSERT_TRUE(strcmp(nfs_ecn_codepoint_name(NFS_ECN_NOT_ECT), "Not-ECT") == 0);
    ASSERT_TRUE(strcmp(nfs_ecn_codepoint_name(NFS_ECN_ECT0), "ECT(0)") == 0);
    ASSERT_TRUE(strcmp(nfs_ecn_codepoint_name(NFS_ECN_ECT1), "ECT(1)") == 0);
    ASSERT_TRUE(strcmp(nfs_ecn_codepoint_name(NFS_ECN_CE), "CE") == 0);
}

static void test_state_names(void) {
    ASSERT_TRUE(strcmp(nfs_ecn_state_name(NFS_ECN_STATE_UNKNOWN), "UNKNOWN") == 0);
    ASSERT_TRUE(strcmp(nfs_ecn_state_name(NFS_ECN_STATE_ACCEPTED), "ACCEPTED") == 0);
    ASSERT_TRUE(strcmp(nfs_ecn_state_name(NFS_ECN_STATE_REJECTED), "REJECTED") == 0);
}

/* ---- Build flag tests ---- */

static void test_build_syn_flags(void) {
    uint16_t f = nfs_ecn_build_syn_flags();
    ASSERT_TRUE(f & NFS_TCP_FLAG_SYN);
    ASSERT_TRUE(f & NFS_TCP_FLAG_ECE);
    ASSERT_TRUE(f & NFS_TCP_FLAG_CWR);
}

static void test_build_synack_accept(void) {
    uint16_t f = nfs_ecn_build_synack_accept_flags();
    ASSERT_TRUE(f & NFS_TCP_FLAG_SYN);
    ASSERT_TRUE(f & NFS_TCP_FLAG_ACK);
    ASSERT_TRUE(f & NFS_TCP_FLAG_ECE);
    ASSERT_TRUE(!(f & NFS_TCP_FLAG_CWR));
}

static void test_build_synack_reject(void) {
    uint16_t f = nfs_ecn_build_synack_reject_flags();
    ASSERT_TRUE(f & NFS_TCP_FLAG_SYN);
    ASSERT_TRUE(f & NFS_TCP_FLAG_ACK);
    ASSERT_TRUE(!(f & NFS_TCP_FLAG_ECE));
    ASSERT_TRUE(!(f & NFS_TCP_FLAG_CWR));
}

int main(void) {
    printf("ECN Tests\n");

    test_ecn_get_not_ect();
    test_ecn_get_ect0();
    test_ecn_get_ect1();
    test_ecn_get_ce();
    test_ecn_set_preserves_dscp();
    test_ecn_set_all_codepoints();
    test_ecn_set_overwrites_old();
    test_is_ect();
    test_is_ce();
    test_mark_ce_from_ect0();
    test_mark_ce_from_ect1();
    test_mark_ce_from_not_ect();
    test_mark_ce_from_ce();
    test_tcp_get_flags();
    test_tcp_set_flags();
    test_tcp_clear_flags();
    test_tcp_has_flags();
    test_tcp_ns_flag();
    test_nego_init();
    test_nego_syn_with_ecn();
    test_nego_synack_accept();
    test_nego_synack_reject();
    test_nego_null();
    test_codepoint_names();
    test_state_names();
    test_build_syn_flags();
    test_build_synack_accept();
    test_build_synack_reject();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
