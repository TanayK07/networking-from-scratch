/* CLI: interactive TCP state machine simulator.
 *
 * Usage:
 *   $ ./tcp_fsm
 *   State: CLOSED
 *   Event: OPEN_ACTIVE
 *   -> State: SYN_SENT (action: SEND_SYN)
 *   Event: RECV_SYNACK
 *   -> State: ESTABLISHED (action: SEND_ACK)
 *   Event: quit
 *
 * Or non-interactive:
 *   $ echo -e "OPEN_ACTIVE\nRECV_SYNACK\nCLOSE" | ./tcp_fsm
 */

#include "tcp_fsm.h"

#include <stdio.h>
#include <string.h>

/* Map event name string to event enum.  Returns -1 if not found. */
static int parse_event(const char *name) {
    struct { const char *str; int ev; } events[] = {
        { "OPEN_PASSIVE", NFS_TCP_EV_OPEN_PASSIVE },
        { "OPEN_ACTIVE",  NFS_TCP_EV_OPEN_ACTIVE  },
        { "CLOSE",        NFS_TCP_EV_CLOSE         },
        { "RECV_SYN",     NFS_TCP_EV_RECV_SYN      },
        { "RECV_SYNACK",  NFS_TCP_EV_RECV_SYNACK   },
        { "RECV_ACK",     NFS_TCP_EV_RECV_ACK      },
        { "RECV_FIN",     NFS_TCP_EV_RECV_FIN      },
        { "RECV_FINACK",  NFS_TCP_EV_RECV_FINACK   },
        { "TIMEOUT",      NFS_TCP_EV_TIMEOUT        },
    };

    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); i++) {
        if (strcmp(name, events[i].str) == 0)
            return events[i].ev;
    }
    return -1;
}

int main(void) {
    struct nfs_tcp_fsm fsm;
    nfs_tcp_fsm_init(&fsm);

    printf("TCP State Machine Simulator\n");
    printf("State: %s\n", nfs_tcp_state_name(fsm.state));

    char line[128];
    while (1) {
        printf("Event: ");
        if (!fgets(line, sizeof(line), stdin))
            break;

        /* Strip trailing newline. */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0)
            break;

        if (line[0] == '\0')
            continue;

        int ev = parse_event(line);
        if (ev < 0) {
            printf("Unknown event '%s'. Valid: OPEN_PASSIVE, OPEN_ACTIVE, CLOSE, "
                   "RECV_SYN, RECV_SYNACK, RECV_ACK, RECV_FIN, RECV_FINACK, TIMEOUT\n", line);
            continue;
        }

        int action;
        int result = nfs_tcp_fsm_handle(&fsm, ev, &action);
        if (result < 0) {
            printf("-> Invalid transition from %s on %s\n",
                   nfs_tcp_state_name(fsm.state), nfs_tcp_event_name(ev));
        } else {
            printf("-> State: %s (action: %s)\n",
                   nfs_tcp_state_name(result), nfs_tcp_action_name(action));
        }
    }

    printf("Final state: %s\n", nfs_tcp_state_name(fsm.state));
    return 0;
}
