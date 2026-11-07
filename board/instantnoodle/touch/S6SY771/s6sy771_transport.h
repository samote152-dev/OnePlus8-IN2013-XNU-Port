/*
 * S6SY771 event-queue reader contract.
 *
 * This layer deliberately does not implement IOKit, GENI, GPIO or power.
 * The future XNU provider supplies a command-based I2C read callback.
 */
#ifndef S6SY771_TRANSPORT_H
#define S6SY771_TRANSPORT_H

#include "s6sy771_protocol.h"

typedef int (*s6sy771_i2c_read_fn)(void *context, uint8_t command,
                                   uint8_t *buffer, size_t length);

struct s6sy771_event_batch {
    size_t count;
    struct s6sy771_event events[S6SY771_EVENT_QUEUE_MAX];
};

enum s6sy771_batch_result {
    S6SY771_BATCH_OK = 0,
    S6SY771_BATCH_EMPTY = 1,
    S6SY771_BATCH_INVALID_ARGUMENT = -1,
    S6SY771_BATCH_I2C_ERROR = -2,
    S6SY771_BATCH_OVERFLOW = -3,
    S6SY771_BATCH_DECODE_ERROR = -4
};

/*
 * Reads one event with 0x60, then any remaining events with 0x61.
 * The callback returns 0 on success and a non-zero value on failure.
 * This function never writes to the controller or updates its firmware.
 */
int s6sy771_read_event_batch(s6sy771_i2c_read_fn read_fn, void *context,
                             struct s6sy771_event_batch *batch);

#endif
