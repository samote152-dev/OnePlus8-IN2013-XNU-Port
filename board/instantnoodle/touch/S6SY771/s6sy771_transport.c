#include "s6sy771_transport.h"

#include <string.h>

int
s6sy771_read_event_batch(s6sy771_i2c_read_fn read_fn, void *context,
                         struct s6sy771_event_batch *batch)
{
    uint8_t first[S6SY771_EVENT_SIZE];
    uint8_t remaining_raw[(S6SY771_EVENT_QUEUE_MAX - 1u) * S6SY771_EVENT_SIZE];
    uint8_t remaining;
    size_t i;

    if (read_fn == NULL || batch == NULL) {
        return S6SY771_BATCH_INVALID_ARGUMENT;
    }

    memset(batch, 0, sizeof(*batch));
    memset(first, 0, sizeof(first));
    if (read_fn(context, S6SY771_CMD_READ_ONE_EVENT, first, sizeof(first)) != 0) {
        return S6SY771_BATCH_I2C_ERROR;
    }

    /* The supplied Linux driver treats byte 0 == 0 as an empty queue. */
    if (first[0] == 0u) {
        return S6SY771_BATCH_EMPTY;
    }

    if (s6sy771_decode_event(first, sizeof(first), &batch->events[0]) !=
        S6SY771_PARSE_OK) {
        return S6SY771_BATCH_DECODE_ERROR;
    }
    batch->count = 1u;

    /* Coordinate/status events carry the queue count in byte 7[5:0]. */
    remaining = (uint8_t)(first[7] & 0x3fu);
    if (remaining > (S6SY771_EVENT_QUEUE_MAX - 1u)) {
        /* Caller may issue 0x62 after deciding how to recover from overflow. */
        return S6SY771_BATCH_OVERFLOW;
    }
    if (remaining == 0u) {
        return S6SY771_BATCH_OK;
    }

    memset(remaining_raw, 0, sizeof(remaining_raw));
    if (read_fn(context, S6SY771_CMD_READ_ALL_EVENT, remaining_raw,
                (size_t)remaining * S6SY771_EVENT_SIZE) != 0) {
        batch->count = 0u;
        return S6SY771_BATCH_I2C_ERROR;
    }

    for (i = 0; i < remaining; i++) {
        const uint8_t *raw = &remaining_raw[i * S6SY771_EVENT_SIZE];
        if (s6sy771_decode_event(raw, S6SY771_EVENT_SIZE,
                                 &batch->events[i + 1u]) != S6SY771_PARSE_OK) {
            batch->count = 0u;
            return S6SY771_BATCH_DECODE_ERROR;
        }
    }

    batch->count = (size_t)remaining + 1u;
    return S6SY771_BATCH_OK;
}
