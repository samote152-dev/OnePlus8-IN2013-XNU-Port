/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef S6SY771_XNU_CORE_H
#define S6SY771_XNU_CORE_H
#include <stdint.h>
#include "s6sy771_transport.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Hardware and HID are injected by the XNU board glue. This core performs no MMIO. */
typedef void (*s6sy771_hid_contact_fn)(void *context, unsigned slot,
    int down, unsigned x, unsigned y);
typedef int (*s6sy771_clear_fn)(void *context);
struct s6sy771_xnu_core {
    s6sy771_i2c_read_fn read_command;
    s6sy771_clear_fn clear_event_stack;
    s6sy771_hid_contact_fn report_contact;
    void *transport_context;
    void *hid_context;
    struct s6sy771_frame frame;
    unsigned consecutive_errors;
};

int s6sy771_xnu_core_init(struct s6sy771_xnu_core *core,
    s6sy771_i2c_read_fn read_command, s6sy771_clear_fn clear_event_stack,
    s6sy771_hid_contact_fn report_contact, void *transport_context,
    void *hid_context);
/* One bounded poll. Returns 0=processed/empty, negative=error. */
int s6sy771_xnu_core_poll_once(struct s6sy771_xnu_core *core);
#ifdef __cplusplus
}
#endif
#endif
