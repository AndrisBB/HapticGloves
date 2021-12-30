#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <device.h>
#include <smf.h>

#include "event_loop.h"

typedef struct 
{
    struct          smf_ctx ctx;
    const struct    device *dev_led;
    const struct    device *dev_btn;

    uint32_t        pending_pairing;

    event_t *       evt;                    // This is workarounf for the SMF. SMF do not 
                                            // allow to attach user data when calling update function
} 
state_t;

int32_t state_init(state_t *state);
int32_t state_update(state_t *state, event_t *evt);

#endif