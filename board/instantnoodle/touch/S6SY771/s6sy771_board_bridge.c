/* SPDX-License-Identifier: BSD-2-Clause */
#include "s6sy771_board_bridge.h"
#include <string.h>

int
s6sy771_board_bridge_start(struct s6sy771_board_bridge *bridge,
    const struct s6sy771_board_ops *ops, void *board_context, void *hid_context)
{
    int rc;
    if (!bridge || !ops || !ops->bus_start || !ops->bus_stop ||
        !ops->irq_pending || !ops->read_command || !ops->report_contact)
        return -1;
    memset(bridge, 0, sizeof(*bridge));
    bridge->ops = *ops;
    bridge->board_context = board_context;
    bridge->hid_context = hid_context;
    rc = bridge->ops.bus_start(board_context);
    if (rc) {
        bridge->state = S6SY771_BRIDGE_FAULT;
        return -2;
    }
    rc = s6sy771_xnu_core_init(&bridge->core, bridge->ops.read_command,
        bridge->ops.clear_event_stack, bridge->ops.report_contact,
        board_context, hid_context);
    if (rc) {
        bridge->ops.bus_stop(board_context);
        bridge->state = S6SY771_BRIDGE_FAULT;
        return -3;
    }
    bridge->state = S6SY771_BRIDGE_READY;
    return 0;
}

int
s6sy771_board_bridge_handle_irq(struct s6sy771_board_bridge *bridge)
{
    int pending;
    if (!bridge || bridge->state != S6SY771_BRIDGE_READY)
        return -1;
    pending = bridge->ops.irq_pending(bridge->board_context);
    if (pending < 0) return -2;
    if (!pending) return 0;
    return s6sy771_xnu_core_poll_once(&bridge->core);
}

void
s6sy771_board_bridge_stop(struct s6sy771_board_bridge *bridge)
{
    if (!bridge || bridge->state == S6SY771_BRIDGE_OFF) return;
    if (bridge->ops.bus_stop) bridge->ops.bus_stop(bridge->board_context);
    bridge->state = S6SY771_BRIDGE_OFF;
}
