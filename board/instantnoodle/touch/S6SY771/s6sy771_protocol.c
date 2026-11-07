#include "s6sy771_protocol.h"

#include <string.h>

int
s6sy771_decode_event(const uint8_t *raw, size_t length,
                     struct s6sy771_event *out_event)
{
    uint8_t eid;

    if (raw == NULL || out_event == NULL) {
        return S6SY771_PARSE_INVALID_ARGUMENT;
    }
    if (length != S6SY771_EVENT_SIZE) {
        return S6SY771_PARSE_BAD_LENGTH;
    }

    /* Avoid exposing stale union bytes to the caller. */
    memset(out_event, 0, sizeof(*out_event));
    out_event->event_id = (uint8_t)(raw[0] & 0x03u);
    eid = out_event->event_id;

    switch (eid) {
    case S6SY771_EVENT_COORDINATE: {
        struct s6sy771_contact *c = &out_event->payload.contact;
        uint8_t one_based_id = (uint8_t)((raw[0] >> 2) & 0x0fu);

        /* Controller contact IDs are one-based. Zero is not a valid slot. */
        if (one_based_id == 0u || one_based_id > S6SY771_MAX_CONTACTS) {
            return S6SY771_PARSE_INVALID_CONTACT_ID;
        }

        out_event->events_remaining = (uint8_t)(raw[7] & 0x3fu);
        out_event->events_remaining_valid = 1;
        c->id = one_based_id;
        c->slot = (uint8_t)(one_based_id - 1u);
        c->state = (uint8_t)((raw[0] >> 6) & 0x03u);

        /*
         * Match sec_event_coordinate in the supplied Linux driver:
         * byte 3 low nibble is y_3_0; high nibble is x_3_0.
         */
        c->x = (uint16_t)(((uint16_t)raw[1] << 4) |
                          ((raw[3] >> 4) & 0x0fu));
        c->y = (uint16_t)(((uint16_t)raw[2] << 4) |
                          (raw[3] & 0x0fu));
        c->major = raw[4];
        c->minor = raw[5];
        c->pressure = (uint8_t)(raw[6] & 0x3fu);
        c->touch_type = (uint8_t)(((raw[6] >> 6) << 2) | (raw[7] >> 6));
        c->in_panel_bounds = (uint8_t)(c->x <= S6SY771_PANEL_MAX_X &&
                                       c->y <= S6SY771_PANEL_MAX_Y);
        return S6SY771_PARSE_OK;
    }

    case S6SY771_EVENT_STATUS:
        out_event->events_remaining = (uint8_t)(raw[7] & 0x3fu);
        out_event->events_remaining_valid = 1;
        out_event->payload.status.subtype = (uint8_t)((raw[0] >> 2) & 0x0fu);
        out_event->payload.status.flags = (uint8_t)((raw[0] >> 6) & 0x03u);
        out_event->payload.status.status_id = raw[1];
        out_event->payload.status.data[0] = raw[2];
        out_event->payload.status.data[1] = raw[3];
        out_event->payload.status.data[2] = raw[4];
        out_event->payload.status.data[3] = raw[5];
        out_event->payload.status.data[4] = raw[6];
        return S6SY771_PARSE_OK;

    case S6SY771_EVENT_GESTURE:
        out_event->payload.gesture.gesture_type = (uint8_t)((raw[0] >> 2) & 0x0fu);
        out_event->payload.gesture.flags = (uint8_t)((raw[0] >> 6) & 0x03u);
        out_event->payload.gesture.gesture_id = raw[1];
        out_event->payload.gesture.coordinate_length = raw[2];
        out_event->payload.gesture.data = raw[3];
        return S6SY771_PARSE_OK;

    case S6SY771_EVENT_EMPTY:
        return S6SY771_PARSE_OK;

    default:
        return S6SY771_PARSE_INVALID_EVENT_ID;
    }
}
