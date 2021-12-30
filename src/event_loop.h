#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <zephyr.h>

typedef struct 
{
    struct k_fifo _events;
} 
event_loop_t;

typedef enum 
{
    EVENT_NONE = 0,
    EVENT_KEY
} 
event_type_t;

typedef struct 
{
    void *          _fifo_reserved;
    event_type_t    _event_type;
}
event_t;

typedef struct
{
    event_t         _event;
    uint32_t        key_state;
} 
key_event_t;



int32_t     event_loop_init(event_loop_t *loop);
event_t*    event_loop_get_event(event_loop_t *loop);
event_t*    event_loop_alloc_event(event_loop_t *loop, event_type_t type);
void        event_loop_put(event_loop_t *loop, event_t *event);
void        event_loop_free_event(event_loop_t *loop, event_t *event);


#endif