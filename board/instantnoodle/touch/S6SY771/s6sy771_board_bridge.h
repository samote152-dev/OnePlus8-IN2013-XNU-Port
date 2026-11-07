/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef S6SY771_BOARD_BRIDGE_H
#define S6SY771_BOARD_BRIDGE_H
#include "s6sy771_xnu_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef int (*s6sy771_bus_start_fn)(void *context);
typedef void (*s6sy771_bus_stop_fn)(void *context);
typedef int (*s6sy771_irq_pending_fn)(void *context);

struct s6sy771_board_ops {
    s6sy771_bus_start_fn bus_start;
    s6sy771_bus_stop_fn bus_stop;
    s6sy771_irq_pending_fn irq_pending;
    s6sy771_i2c_read_fn read_command;
    s6sy771_clear_fn clear_event_stack;
    s6sy771_hid_contact_fn report_contact;
};

enum s6sy771_bridge_state {
    S6SY771_BRIDGE_OFF = 0,
    S6SY771_BRIDGE_READY = 1,
    S6SY771_BRIDGE_FAULT = 2
};

struct s6sy771_board_bridge {
    struct s6sy771_board_ops ops;
    void *board_context;
    void *hid_context;
    struct s6sy771_xnu_core core;
    enum s6sy771_bridge_state state;
};

int s6sy771_board_bridge_start(struct s6sy771_board_bridge *bridge,
    const struct s6sy771_board_ops *ops, void *board_context, void *hid_context);
int s6sy771_board_bridge_handle_irq(struct s6sy771_board_bridge *bridge);
void s6sy771_board_bridge_stop(struct s6sy771_board_bridge *bridge);
#ifdef __cplusplus
}
#endif
#endif
