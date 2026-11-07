#include "s6sy771_xnu_core.h"
#include <string.h>

int
s6sy771_xnu_core_init(struct s6sy771_xnu_core *core,
    s6sy771_i2c_read_fn read_command, s6sy771_clear_fn clear_event_stack,
    s6sy771_hid_contact_fn report_contact, void *transport_context,
    void *hid_context)
{
    if (!core || !read_command || !report_contact) return -1;
    memset(core, 0, sizeof(*core));
    core->read_command = read_command;
    core->clear_event_stack = clear_event_stack;
    core->report_contact = report_contact;
    core->transport_context = transport_context;
    core->hid_context = hid_context;
    s6sy771_frame_reset(&core->frame);
    return 0;
}

int
s6sy771_xnu_core_poll_once(struct s6sy771_xnu_core *core)
{
    struct s6sy771_event_batch batch;
    int result;
    if (!core || !core->read_command || !core->report_contact) return -1;
    result = s6sy771_read_event_batch(core->read_command,
        core->transport_context, &batch);
    if (result == S6SY771_BATCH_EMPTY) {
        core->consecutive_errors = 0;
        return 0;
    }
    if (result == S6SY771_BATCH_OVERFLOW) {
        ++core->consecutive_errors;
        /* Clear only on an explicit overflow, never on an ordinary parse/I2C error. */
        if (core->clear_event_stack) (void)core->clear_event_stack(core->transport_context);
        return -2;
    }
    if (result != S6SY771_BATCH_OK) {
        ++core->consecutive_errors;
        return -3;
    }
    core->consecutive_errors = 0;
    for (size_t i = 0; i < batch.count; ++i) {
        const struct s6sy771_event *event = &batch.events[i];
        int applied = s6sy771_frame_apply(&core->frame, event);
        if (applied == S6SY771_FRAME_UPDATED) {
            const struct s6sy771_contact *c = &event->payload.contact;
            core->report_contact(core->hid_context, c->slot,
                c->state != S6SY771_CONTACT_RELEASE,
                c->x, c->y);
        } else if (applied < 0) {
            ++core->consecutive_errors;
            return -4;
        }
    }
    return 0;
}
