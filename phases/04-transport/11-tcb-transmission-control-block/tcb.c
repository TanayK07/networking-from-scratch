#include "tcb.h"
#include <stdio.h>
#include <string.h>

void nfs_tcb_init(struct nfs_tcb *tcb,
                  uint16_t local_port, uint16_t remote_port,
                  uint32_t local_addr, uint32_t remote_addr)
{
    memset(tcb, 0, sizeof(*tcb));
    tcb->state       = NFS_TCB_CLOSED;
    tcb->local_port  = local_port;
    tcb->remote_port = remote_port;
    tcb->local_addr  = local_addr;
    tcb->remote_addr = remote_addr;
    tcb->snd_wnd     = 65535;
    tcb->rcv_wnd     = 65535;
}

void nfs_tcb_set_iss(struct nfs_tcb *tcb, uint32_t iss)
{
    tcb->iss     = iss;
    tcb->snd_una = iss;
    tcb->snd_nxt = iss + 1;
}

void nfs_tcb_set_irs(struct nfs_tcb *tcb, uint32_t irs)
{
    tcb->irs     = irs;
    tcb->rcv_nxt = irs + 1;
}

int nfs_tcb_snd_available(const struct nfs_tcb *tcb)
{
    uint32_t in_flight = tcb->snd_nxt - tcb->snd_una;
    if (in_flight >= tcb->snd_wnd)
        return 0;
    return (int)(tcb->snd_wnd - in_flight);
}

void nfs_tcb_advance_snd(struct nfs_tcb *tcb, uint32_t nbytes)
{
    tcb->snd_nxt += nbytes;
}

void nfs_tcb_ack_received(struct nfs_tcb *tcb, uint32_t ack_num)
{
    /* Only advance if ack_num is ahead of snd_una.
     * Use modular arithmetic comparison for sequence number wraparound:
     * ack_num > snd_una iff (ack_num - snd_una) is in (0, 2^31). */
    int32_t diff = (int32_t)(ack_num - tcb->snd_una);
    if (diff > 0) {
        tcb->snd_una = ack_num;
    }
}

void nfs_tcb_data_received(struct nfs_tcb *tcb, uint32_t nbytes)
{
    tcb->rcv_nxt += nbytes;
}

/* Helper: format an IPv4 address from host-order uint32_t. */
static void fmt_addr(uint32_t addr, char *buf, size_t sz)
{
    snprintf(buf, sz, "%u.%u.%u.%u",
             (addr >> 24) & 0xFF,
             (addr >> 16) & 0xFF,
             (addr >>  8) & 0xFF,
              addr        & 0xFF);
}

void nfs_tcb_format(const struct nfs_tcb *tcb, char *buf, size_t sz)
{
    char la[16], ra[16];
    fmt_addr(tcb->local_addr,  la, sizeof(la));
    fmt_addr(tcb->remote_addr, ra, sizeof(ra));

    snprintf(buf, sz,
             "TCB %s:%u -> %s:%u  "
             "snd_una=%u snd_nxt=%u snd_wnd=%u  "
             "rcv_nxt=%u rcv_wnd=%u",
             la, tcb->local_port, ra, tcb->remote_port,
             tcb->snd_una, tcb->snd_nxt, tcb->snd_wnd,
             tcb->rcv_nxt, tcb->rcv_wnd);
}

int nfs_tcb_matches(const struct nfs_tcb *tcb,
                    uint32_t src_addr, uint16_t src_port,
                    uint32_t dst_addr, uint16_t dst_port)
{
    /* A packet from (src_addr:src_port) to (dst_addr:dst_port) matches
     * if src matches remote and dst matches local. */
    return (tcb->remote_addr == src_addr &&
            tcb->remote_port == src_port &&
            tcb->local_addr  == dst_addr &&
            tcb->local_port  == dst_port) ? 1 : 0;
}
