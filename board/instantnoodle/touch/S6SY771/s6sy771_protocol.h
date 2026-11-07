/*
 * Samsung S6SY771 protocol decoder for the OnePlus 8 (instantnoodle).
 *
 * This file is platform independent: it contains no Linux, IOKit, or
 * hardware access.  It is intended to be reused by a future XNU driver.
 * The wire format and command IDs are derived from the supplied Oplus
 * S6SY771 driver and its header.
 */
#ifndef S6SY771_PROTOCOL_H
#define S6SY771_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define S6SY771_I2C_ADDRESS       0x48u
#define S6SY771_EVENT_SIZE        8u
#define S6SY771_EVENT_QUEUE_MAX   32u
#define S6SY771_MAX_CONTACTS      10u
#define S6SY771_PANEL_MAX_X       1079u
#define S6SY771_PANEL_MAX_Y       2399u

/* Commands documented in sec_drivers_s6sy771.h. */
#define S6SY771_CMD_READ_ONE_EVENT    0x60u
#define S6SY771_CMD_READ_ALL_EVENT    0x61u
#define S6SY771_CMD_CLEAR_EVENT_STACK 0x62u
#define S6SY771_CMD_SENSE_ON          0x10u
#define S6SY771_CMD_SENSE_OFF         0x11u
#define S6SY771_CMD_SOFT_RESET        0x12u
#define S6SY771_CMD_READ_DEVICE_ID    0x22u
#define S6SY771_CMD_READ_BOOT_STATUS  0x55u

/* Values encoded in bits 1:0 of byte 0. */
enum s6sy771_event_id {
    S6SY771_EVENT_COORDINATE = 0,
    S6SY771_EVENT_STATUS     = 1,
    S6SY771_EVENT_GESTURE    = 2,
    S6SY771_EVENT_EMPTY      = 3
};

/* Values encoded in bits 7:6 of coordinate-event byte 0. */
enum s6sy771_contact_state {
    S6SY771_CONTACT_NONE    = 0,
    S6SY771_CONTACT_PRESS   = 1,
    S6SY771_CONTACT_MOVE    = 2,
    S6SY771_CONTACT_RELEASE = 3
};

enum s6sy771_parse_result {
    S6SY771_PARSE_OK = 0,
    S6SY771_PARSE_INVALID_ARGUMENT = -1,
    S6SY771_PARSE_BAD_LENGTH = -2,
    S6SY771_PARSE_INVALID_EVENT_ID = -3,
    S6SY771_PARSE_INVALID_CONTACT_ID = -4
};

struct s6sy771_contact {
    uint8_t  id;       /* 1..10, as reported by the controller */
    uint8_t  slot;     /* 0..9, for a host multitouch slot */
    uint8_t  state;    /* enum s6sy771_contact_state */
    uint16_t x;        /* 12-bit coordinate */
    uint16_t y;        /* 12-bit coordinate */
    uint8_t  major;
    uint8_t  minor;
    uint8_t  pressure; /* six-bit Z value from the packet */
    uint8_t  touch_type; /* four bits assembled from bytes 6 and 7 */
    uint8_t  in_panel_bounds; /* 1 if x<=1079 and y<=2399 */
};

struct s6sy771_event {
    uint8_t event_id;
    uint8_t events_remaining; /* low six bits for coordinate/status events */
    uint8_t events_remaining_valid; /* gestures do not define this field */
    union {
        struct s6sy771_contact contact;
        struct {
            uint8_t subtype; /* bits 5:2 of byte 0 */
            uint8_t flags;   /* bits 7:6 of byte 0 */
            uint8_t status_id;
            uint8_t data[5]; /* bytes 2..6 */
        } status;
        struct {
            uint8_t gesture_type; /* bits 5:2 of byte 0 */
            uint8_t flags;        /* bits 7:6 of byte 0 */
            uint8_t gesture_id;
            uint8_t coordinate_length;
            uint8_t data;
        } gesture;
    } payload;
};

struct s6sy771_frame {
    uint16_t active_mask; /* bit n is set when contact slot n is active */
    struct s6sy771_contact contacts[S6SY771_MAX_CONTACTS];
};

enum s6sy771_frame_result {
    S6SY771_FRAME_UPDATED = 0,
    S6SY771_FRAME_IGNORED = 1,
    S6SY771_FRAME_INVALID_ARGUMENT = -1,
    S6SY771_FRAME_INVALID_SLOT = -2
};

/* Decode exactly one eight-byte controller event. */
int s6sy771_decode_event(const uint8_t *raw, size_t length,
                         struct s6sy771_event *out_event);

/* Host-side state accumulator; safe to reuse from a future XNU driver. */
void s6sy771_frame_reset(struct s6sy771_frame *frame);
int s6sy771_frame_apply(struct s6sy771_frame *frame,
                        const struct s6sy771_event *event);

#ifdef __cplusplus
}
#endif
#endif
