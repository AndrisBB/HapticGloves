#include "event_loop.h"


int32_t event_loop_init(event_loop_t *loop)
{
    if(loop == NULL) {
        return -1;
    }

    k_fifo_init(&(loop->_events));

    return 0;
}

event_t* event_loop_get_event(event_loop_t *loop)
{
    return k_fifo_get(&(loop->_events), K_FOREVER);
}

event_t* event_loop_alloc_event(event_loop_t *loop, event_type_t type)
{
    event_t *evt = NULL;

    if(type == EVENT_KEY) {
        evt = (event_t*)k_malloc(sizeof(key_event_t));
        if(evt != NULL) {
            evt->_event_type = type;
        }
    }
    
    return evt;
}

void event_loop_put(event_loop_t *loop, event_t *event)
{
    k_fifo_put(&(loop->_events), event);
}

void event_loop_free_event(event_loop_t *loop, event_t *event)
{
    (void)loop;
    k_free(event);
}