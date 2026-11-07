#include "s6sy771_protocol.h"

#include <string.h>

void
s6sy771_frame_reset(struct s6sy771_frame *frame)
{
    if (frame != NULL) {
        memset(frame, 0, sizeof(*frame));
    }
}

int
s6sy771_frame_apply(struct s6sy771_frame *frame,
                    const struct s6sy771_event *event)
{
    const struct s6sy771_contact *contact;
    uint16_t bit;

    if (frame == NULL || event == NULL) {
        return S6SY771_FRAME_INVALID_ARGUMENT;
    }
    if (event->event_id != S6SY771_EVENT_COORDINATE) {
        return S6SY771_FRAME_IGNORED;
    }

    contact = &event->payload.contact;
    if (contact->slot >= S6SY771_MAX_CONTACTS ||
        contact->id != (uint8_t)(contact->slot + 1u)) {
        return S6SY771_FRAME_INVALID_SLOT;
    }

    bit = (uint16_t)(1u << contact->slot);
    switch (contact->state) {
    case S6SY771_CONTACT_PRESS:
    case S6SY771_CONTACT_MOVE:
        frame->contacts[contact->slot] = *contact;
        frame->active_mask = (uint16_t)(frame->active_mask | bit);
        return S6SY771_FRAME_UPDATED;

    case S6SY771_CONTACT_RELEASE:
        frame->contacts[contact->slot] = *contact;
        frame->active_mask = (uint16_t)(frame->active_mask & (uint16_t)~bit);
        return S6SY771_FRAME_UPDATED;

    case S6SY771_CONTACT_NONE:
    default:
        return S6SY771_FRAME_IGNORED;
    }
}
