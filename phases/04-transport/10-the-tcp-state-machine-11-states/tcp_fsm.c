/* TCP Finite State Machine (RFC 9293).
 *
 * Implements the 11-state TCP state machine as a pure function.
 * No networking, no timers -- just deterministic state transitions
 * based on events.
 *
 * The transition table follows the TCP state diagram from RFC 9293,
 * Section 3.3.2 (Figure 5).
 */

#include "tcp_fsm.h"

#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  State / event / action name tables                                 */
/* ------------------------------------------------------------------ */

static const char *state_names[] = {
    [NFS_TCP_CLOSED]      = "CLOSED",
    [NFS_TCP_LISTEN]      = "LISTEN",
    [NFS_TCP_SYN_SENT]    = "SYN_SENT",
    [NFS_TCP_SYN_RCVD]    = "SYN_RCVD",
    [NFS_TCP_ESTABLISHED] = "ESTABLISHED",
    [NFS_TCP_FIN_WAIT_1]  = "FIN_WAIT_1",
    [NFS_TCP_FIN_WAIT_2]  = "FIN_WAIT_2",
    [NFS_TCP_CLOSE_WAIT]  = "CLOSE_WAIT",
    [NFS_TCP_CLOSING]     = "CLOSING",
    [NFS_TCP_LAST_ACK]    = "LAST_ACK",
    [NFS_TCP_TIME_WAIT]   = "TIME_WAIT",
};

#define NUM_STATES (sizeof(state_names) / sizeof(state_names[0]))

static const char *event_names[] = {
    [NFS_TCP_EV_OPEN_PASSIVE] = "OPEN_PASSIVE",
    [NFS_TCP_EV_OPEN_ACTIVE]  = "OPEN_ACTIVE",
    [NFS_TCP_EV_CLOSE]        = "CLOSE",
    [NFS_TCP_EV_RECV_SYN]     = "RECV_SYN",
    [NFS_TCP_EV_RECV_SYNACK]  = "RECV_SYNACK",
    [NFS_TCP_EV_RECV_ACK]     = "RECV_ACK",
    [NFS_TCP_EV_RECV_FIN]     = "RECV_FIN",
    [NFS_TCP_EV_RECV_FINACK]  = "RECV_FINACK",
    [NFS_TCP_EV_TIMEOUT]      = "TIMEOUT",
};

#define NUM_EVENTS (sizeof(event_names) / sizeof(event_names[0]))

static const char *action_names[] = {
    [NFS_TCP_ACT_NONE]        = "NONE",
    [NFS_TCP_ACT_SEND_SYN]    = "SEND_SYN",
    [NFS_TCP_ACT_SEND_SYNACK] = "SEND_SYNACK",
    [NFS_TCP_ACT_SEND_ACK]    = "SEND_ACK",
    [NFS_TCP_ACT_SEND_FIN]    = "SEND_FIN",
    [NFS_TCP_ACT_SEND_FINACK] = "SEND_FINACK",
    [NFS_TCP_ACT_DELETE_TCB]   = "DELETE_TCB",
};

/* ------------------------------------------------------------------ */
/*  Transition table                                                   */
/*                                                                     */
/*  Each entry: {next_state, action}.                                  */
/*  A next_state of -1 means "invalid transition".                     */
/* ------------------------------------------------------------------ */

struct transition {
    int next_state;
    int action;
};

/* Sentinel for invalid transitions. */
#define INVALID {-1, NFS_TCP_ACT_NONE}

/*
 * Transition table indexed by [current_state][event].
 *
 * Key transitions from RFC 9293:
 *
 * CLOSED:
 *   OPEN_PASSIVE -> LISTEN (create TCB)
 *   OPEN_ACTIVE  -> SYN_SENT (send SYN)
 *
 * LISTEN:
 *   RECV_SYN     -> SYN_RCVD (send SYN,ACK)
 *   CLOSE        -> CLOSED (delete TCB)
 *
 * SYN_SENT:
 *   RECV_SYNACK  -> ESTABLISHED (send ACK)
 *   RECV_SYN     -> SYN_RCVD (send SYN,ACK) -- simultaneous open
 *   CLOSE        -> CLOSED (delete TCB)
 *
 * SYN_RCVD:
 *   RECV_ACK     -> ESTABLISHED
 *   CLOSE        -> FIN_WAIT_1 (send FIN)
 *
 * ESTABLISHED:
 *   CLOSE        -> FIN_WAIT_1 (send FIN)
 *   RECV_FIN     -> CLOSE_WAIT (send ACK)
 *
 * FIN_WAIT_1:
 *   RECV_ACK     -> FIN_WAIT_2
 *   RECV_FIN     -> CLOSING (send ACK) -- simultaneous close
 *   RECV_FINACK  -> TIME_WAIT (send ACK)
 *
 * FIN_WAIT_2:
 *   RECV_FIN     -> TIME_WAIT (send ACK)
 *
 * CLOSE_WAIT:
 *   CLOSE        -> LAST_ACK (send FIN)
 *
 * CLOSING:
 *   RECV_ACK     -> TIME_WAIT
 *
 * LAST_ACK:
 *   RECV_ACK     -> CLOSED (delete TCB)
 *
 * TIME_WAIT:
 *   TIMEOUT      -> CLOSED (delete TCB) -- 2MSL timeout
 */
static const struct transition table[11][9] = {
    /* CLOSED */
    [NFS_TCP_CLOSED] = {
        [NFS_TCP_EV_OPEN_PASSIVE] = { NFS_TCP_LISTEN,   NFS_TCP_ACT_NONE     },
        [NFS_TCP_EV_OPEN_ACTIVE]  = { NFS_TCP_SYN_SENT, NFS_TCP_ACT_SEND_SYN },
        [NFS_TCP_EV_CLOSE]        = INVALID,
        [NFS_TCP_EV_RECV_SYN]     = INVALID,
        [NFS_TCP_EV_RECV_SYNACK]  = INVALID,
        [NFS_TCP_EV_RECV_ACK]     = INVALID,
        [NFS_TCP_EV_RECV_FIN]     = INVALID,
        [NFS_TCP_EV_RECV_FINACK]  = INVALID,
        [NFS_TCP_EV_TIMEOUT]      = INVALID,
    },

    /* LISTEN */
    [NFS_TCP_LISTEN] = {
        [NFS_TCP_EV_OPEN_PASSIVE] = INVALID,
        [NFS_TCP_EV_OPEN_ACTIVE]  = INVALID,
        [NFS_TCP_EV_CLOSE]        = { NFS_TCP_CLOSED,   NFS_TCP_ACT_DELETE_TCB  },
        [NFS_TCP_EV_RECV_SYN]     = { NFS_TCP_SYN_RCVD, NFS_TCP_ACT_SEND_SYNACK },
        [NFS_TCP_EV_RECV_SYNACK]  = INVALID,
        [NFS_TCP_EV_RECV_ACK]     = INVALID,
        [NFS_TCP_EV_RECV_FIN]     = INVALID,
        [NFS_TCP_EV_RECV_FINACK]  = INVALID,
        [NFS_TCP_EV_TIMEOUT]      = INVALID,
    },

    /* SYN_SENT */
    [NFS_TCP_SYN_SENT] = {
        [NFS_TCP_EV_OPEN_PASSIVE] = INVALID,
        [NFS_TCP_EV_OPEN_ACTIVE]  = INVALID,
        [NFS_TCP_EV_CLOSE]        = { NFS_TCP_CLOSED,      NFS_TCP_ACT_DELETE_TCB  },
        [NFS_TCP_EV_RECV_SYN]     = { NFS_TCP_SYN_RCVD,    NFS_TCP_ACT_SEND_SYNACK },
        [NFS_TCP_EV_RECV_SYNACK]  = { NFS_TCP_ESTABLISHED,  NFS_TCP_ACT_SEND_ACK    },
        [NFS_TCP_EV_RECV_ACK]     = INVALID,
        [NFS_TCP_EV_RECV_FIN]     = INVALID,
        [NFS_TCP_EV_RECV_FINACK]  = INVALID,
        [NFS_TCP_EV_TIMEOUT]      = INVALID,
    },

    /* SYN_RCVD */
    [NFS_TCP_SYN_RCVD] = {
        [NFS_TCP_EV_OPEN_PASSIVE] = INVALID,
        [NFS_TCP_EV_OPEN_ACTIVE]  = INVALID,
        [NFS_TCP_EV_CLOSE]        = { NFS_TCP_FIN_WAIT_1,   NFS_TCP_ACT_SEND_FIN },
        [NFS_TCP_EV_RECV_SYN]     = INVALID,
        [NFS_TCP_EV_RECV_SYNACK]  = INVALID,
        [NFS_TCP_EV_RECV_ACK]     = { NFS_TCP_ESTABLISHED,  NFS_TCP_ACT_NONE     },
        [NFS_TCP_EV_RECV_FIN]     = INVALID,
        [NFS_TCP_EV_RECV_FINACK]  = INVALID,
        [NFS_TCP_EV_TIMEOUT]      = INVALID,
    },

    /* ESTABLISHED */
    [NFS_TCP_ESTABLISHED] = {
        [NFS_TCP_EV_OPEN_PASSIVE] = INVALID,
        [NFS_TCP_EV_OPEN_ACTIVE]  = INVALID,
        [NFS_TCP_EV_CLOSE]        = { NFS_TCP_FIN_WAIT_1,  NFS_TCP_ACT_SEND_FIN },
        [NFS_TCP_EV_RECV_SYN]     = INVALID,
        [NFS_TCP_EV_RECV_SYNACK]  = INVALID,
        [NFS_TCP_EV_RECV_ACK]     = INVALID,
        [NFS_TCP_EV_RECV_FIN]     = { NFS_TCP_CLOSE_WAIT,  NFS_TCP_ACT_SEND_ACK },
        [NFS_TCP_EV_RECV_FINACK]  = INVALID,
        [NFS_TCP_EV_TIMEOUT]      = INVALID,
    },

    /* FIN_WAIT_1 */
    [NFS_TCP_FIN_WAIT_1] = {
        [NFS_TCP_EV_OPEN_PASSIVE] = INVALID,
        [NFS_TCP_EV_OPEN_ACTIVE]  = INVALID,
        [NFS_TCP_EV_CLOSE]        = INVALID,
        [NFS_TCP_EV_RECV_SYN]     = INVALID,
        [NFS_TCP_EV_RECV_SYNACK]  = INVALID,
        [NFS_TCP_EV_RECV_ACK]     = { NFS_TCP_FIN_WAIT_2,  NFS_TCP_ACT_NONE     },
        [NFS_TCP_EV_RECV_FIN]     = { NFS_TCP_CLOSING,     NFS_TCP_ACT_SEND_ACK },
        [NFS_TCP_EV_RECV_FINACK]  = { NFS_TCP_TIME_WAIT,   NFS_TCP_ACT_SEND_ACK },
        [NFS_TCP_EV_TIMEOUT]      = INVALID,
    },

    /* FIN_WAIT_2 */
    [NFS_TCP_FIN_WAIT_2] = {
        [NFS_TCP_EV_OPEN_PASSIVE] = INVALID,
        [NFS_TCP_EV_OPEN_ACTIVE]  = INVALID,
        [NFS_TCP_EV_CLOSE]        = INVALID,
        [NFS_TCP_EV_RECV_SYN]     = INVALID,
        [NFS_TCP_EV_RECV_SYNACK]  = INVALID,
        [NFS_TCP_EV_RECV_ACK]     = INVALID,
        [NFS_TCP_EV_RECV_FIN]     = { NFS_TCP_TIME_WAIT,   NFS_TCP_ACT_SEND_ACK },
        [NFS_TCP_EV_RECV_FINACK]  = INVALID,
        [NFS_TCP_EV_TIMEOUT]      = INVALID,
    },

    /* CLOSE_WAIT */
    [NFS_TCP_CLOSE_WAIT] = {
        [NFS_TCP_EV_OPEN_PASSIVE] = INVALID,
        [NFS_TCP_EV_OPEN_ACTIVE]  = INVALID,
        [NFS_TCP_EV_CLOSE]        = { NFS_TCP_LAST_ACK,    NFS_TCP_ACT_SEND_FIN },
        [NFS_TCP_EV_RECV_SYN]     = INVALID,
        [NFS_TCP_EV_RECV_SYNACK]  = INVALID,
        [NFS_TCP_EV_RECV_ACK]     = INVALID,
        [NFS_TCP_EV_RECV_FIN]     = INVALID,
        [NFS_TCP_EV_RECV_FINACK]  = INVALID,
        [NFS_TCP_EV_TIMEOUT]      = INVALID,
    },

    /* CLOSING */
    [NFS_TCP_CLOSING] = {
        [NFS_TCP_EV_OPEN_PASSIVE] = INVALID,
        [NFS_TCP_EV_OPEN_ACTIVE]  = INVALID,
        [NFS_TCP_EV_CLOSE]        = INVALID,
        [NFS_TCP_EV_RECV_SYN]     = INVALID,
        [NFS_TCP_EV_RECV_SYNACK]  = INVALID,
        [NFS_TCP_EV_RECV_ACK]     = { NFS_TCP_TIME_WAIT,   NFS_TCP_ACT_NONE     },
        [NFS_TCP_EV_RECV_FIN]     = INVALID,
        [NFS_TCP_EV_RECV_FINACK]  = INVALID,
        [NFS_TCP_EV_TIMEOUT]      = INVALID,
    },

    /* LAST_ACK */
    [NFS_TCP_LAST_ACK] = {
        [NFS_TCP_EV_OPEN_PASSIVE] = INVALID,
        [NFS_TCP_EV_OPEN_ACTIVE]  = INVALID,
        [NFS_TCP_EV_CLOSE]        = INVALID,
        [NFS_TCP_EV_RECV_SYN]     = INVALID,
        [NFS_TCP_EV_RECV_SYNACK]  = INVALID,
        [NFS_TCP_EV_RECV_ACK]     = { NFS_TCP_CLOSED,      NFS_TCP_ACT_DELETE_TCB },
        [NFS_TCP_EV_RECV_FIN]     = INVALID,
        [NFS_TCP_EV_RECV_FINACK]  = INVALID,
        [NFS_TCP_EV_TIMEOUT]      = INVALID,
    },

    /* TIME_WAIT */
    [NFS_TCP_TIME_WAIT] = {
        [NFS_TCP_EV_OPEN_PASSIVE] = INVALID,
        [NFS_TCP_EV_OPEN_ACTIVE]  = INVALID,
        [NFS_TCP_EV_CLOSE]        = INVALID,
        [NFS_TCP_EV_RECV_SYN]     = INVALID,
        [NFS_TCP_EV_RECV_SYNACK]  = INVALID,
        [NFS_TCP_EV_RECV_ACK]     = INVALID,
        [NFS_TCP_EV_RECV_FIN]     = INVALID,
        [NFS_TCP_EV_RECV_FINACK]  = INVALID,
        [NFS_TCP_EV_TIMEOUT]      = { NFS_TCP_CLOSED,      NFS_TCP_ACT_DELETE_TCB },
    },
};

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

void nfs_tcp_fsm_init(struct nfs_tcp_fsm *fsm) {
    if (fsm)
        fsm->state = NFS_TCP_CLOSED;
}

int nfs_tcp_fsm_handle(struct nfs_tcp_fsm *fsm, int event, int *action) {
    if (!fsm || !action)
        return -1;

    if (fsm->state < 0 || fsm->state >= (int)NUM_STATES)
        return -1;
    if (event < 0 || event >= (int)NUM_EVENTS)
        return -1;

    const struct transition *t = &table[fsm->state][event];
    if (t->next_state < 0) {
        *action = NFS_TCP_ACT_NONE;
        return -1;
    }

    fsm->state = t->next_state;
    *action = t->action;
    return fsm->state;
}

const char *nfs_tcp_state_name(int state) {
    if (state < 0 || state >= (int)NUM_STATES)
        return "UNKNOWN";
    return state_names[state];
}

const char *nfs_tcp_event_name(int event) {
    if (event < 0 || event >= (int)NUM_EVENTS)
        return "UNKNOWN";
    return event_names[event];
}

const char *nfs_tcp_action_name(int action) {
    if (action < 0 || action >= (int)(sizeof(action_names) / sizeof(action_names[0])))
        return "UNKNOWN";
    return action_names[action];
}
