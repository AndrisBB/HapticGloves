#include "system_state.h"

#include <zephyr.h>
#include <init.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <pm/pm.h>
#include <logging/log.h>

#include <string.h>

#include <hal/nrf_gpio.h>

#define BTN_PRESSED                     0
#define BTN_RELEASED                    1
#define SYSTEM_ON_THRESHOLD_MS          2000
#define PAIRING_THRESHOLD_MS            4000

#define BUTTON_GPIO_PIN                 DT_GPIO_PIN(DT_ALIAS(sw1), gpios)

LOG_MODULE_REGISTER(state);


static const struct smf_state system_states[];

enum system_state 
{
    DEEP_SLEEP = 0,
    RESET,
    ADVERTISE,
    PAIRING,
    CONNECTED
};

// ------------------------------------------------------
//   DEEP_SLEEP state
// ------------------------------------------------------
static void deep_sleep_entry(void *o)
{
    LOG_INF("%s", __func__);

    state_t *state = (state_t *)o;
    if((state->dev_btn == NULL) || (state->dev_led == NULL)){
        LOG_WRN("Woops, something not right:%s", __func__);
        return;
    }

    // Turn status LED OFF
    gpio_pin_set(state->dev_led, DT_GPIO_PIN(DT_ALIAS(led0), gpios), 0);

    // Setup GPIO IRQ, so that system can wake up on BTN interrupt
    nrf_gpio_cfg_sense_set(DT_GPIO_PIN(DT_NODELABEL(button1), gpios),NRF_GPIO_PIN_SENSE_LOW);

    // Force DEEP SLEEP mode
    pm_power_state_force(0u, (struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});
}

static void deep_sleep_run(void *o)
{
    LOG_INF("%s: Should go to sleep after this", __func__);
}

static void deep_sleep_exit(void *o)
{
    LOG_WRN("%s: Deep sleep state: How did we get here?", __func__);
}


// ------------------------------------------------------
//   RESET state
// ------------------------------------------------------
static void reset_entry(void *o)
{
    LOG_INF("%s", __func__);

    state_t *state = (state_t *)o;
    if((state->dev_btn == NULL) || (state->dev_led == NULL)){
        LOG_WRN("Woops, something not right:%s", __func__);
        return;
    }

    // Turn status LED OFF
    gpio_pin_set(state->dev_led, DT_GPIO_PIN(DT_ALIAS(led0), gpios), 0);

    // Check if btn is still down, might be some glich or something or
    // power could be just connected.
    int32_t btn_state = gpio_pin_get(state->dev_btn, BUTTON_GPIO_PIN);
    if(btn_state != BTN_PRESSED) {
        LOG_INF("%s: Button is not pressed, going into DEEP SLEEP", __func__);
        smf_set_state(SMF_CTX(state), &system_states[DEEP_SLEEP]);
    }

    state->pending_pairing = 0;
}

static void reset_run(void *o)
{
    LOG_INF("%s called", __func__);

    enum system_state next_state = RESET;

    state_t *state = (state_t *)o;
    event_t *evt = state->evt;
    
    if((state->dev_btn == NULL) || (state->dev_led == NULL)){
        LOG_WRN("Woops, something not right:%s", __func__);
        return;
    }

    if(evt->_event_type == EVENT_KEY) {
        key_event_t *key_event = (key_event_t *)evt;

        LOG_INF("Handle key event. Key state:%u", key_event->key_state);

        if(key_event->key_state == BTN_RELEASED) {
            uint32_t up_time = k_uptime_get_32();

            LOG_INF("Kernel uptime:%u", up_time);

            if(up_time >= SYSTEM_ON_THRESHOLD_MS) {
                LOG_INF("Reached system ON threshold");
                next_state = ADVERTISE;
            }
            else {
                next_state = DEEP_SLEEP;
            }

            if(up_time >= PAIRING_THRESHOLD_MS) {
                state->pending_pairing = 1;
            }
        }
    }

    if(next_state != RESET) {
        smf_set_state(SMF_CTX(state), &system_states[next_state]);
    }

    LOG_INF("%s completed", __func__);
}

static void reset_exit(void *o)
{
    LOG_INF("%s", __func__);
}

// ------------------------------------------------------
//   ADEVERISE state
// ------------------------------------------------------
static void advertise_entry(void *o)
{
    LOG_INF("%s", __func__);

    state_t *state = (state_t *)o;
    if((state->dev_btn == NULL) || (state->dev_led == NULL)){
        LOG_WRN("Woops, something not right:%s", __func__);
        return;
    }

    // Turn status LED ON
    gpio_pin_set(state->dev_led, DT_GPIO_PIN(DT_ALIAS(led0), gpios), 1);

    LOG_INF("Pending pairing:%u", state->pending_pairing);
}

static void advertise_run(void *o)
{
    LOG_INF("%s", __func__);

    state_t *state = (state_t *)o;
    if((state->dev_btn == NULL) || (state->dev_led == NULL)){
        LOG_WRN("Woops, something not right:%s", __func__);
        return;
    }

    // int32_t btn_state = gpio_pin_get(state->dev_btn, DT_GPIO_PIN(DT_ALIAS(sw1), gpios));

    // if(btn_state == BTN_PRESSED) {
    //     state->btn_down_ticks++;
    // }
    // else {
    //     state->btn_down_ticks = 0;
    // }

    // if(state->btn_down_ticks >= BTN_TICKS_THRESHOLD_SYSTEM_OFF) {
    //     LOG_INF("%s: Reached SYSTEM_OFF threshold", __func__);
    //     smf_set_state(SMF_CTX(state), &system_states[DEEP_SLEEP]);
    // }
}

static void advertise_exit(void *o)
{
    LOG_INF("%s", __func__);
}

static const struct smf_state system_states[] = 
{
    [DEEP_SLEEP]    = SMF_CREATE_STATE(deep_sleep_entry, deep_sleep_run, deep_sleep_exit),
    [RESET]         = SMF_CREATE_STATE(reset_entry, reset_run, reset_exit),
    [ADVERTISE]     = SMF_CREATE_STATE(advertise_entry, advertise_run, advertise_exit)
};

int32_t state_init(state_t *state)
{
    int32_t ret = 0;

    state->dev_led = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(led0), gpios));
    if(state->dev_led == NULL) {
        LOG_ERR("LED0 device is NULL");
        return -1;
    }

    state->dev_btn = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(sw1), gpios));
    if(state->dev_btn == NULL) {
        LOG_ERR("BTN device is NULL");
        return -1;
    }

    ret = gpio_pin_interrupt_configure(state->dev_btn, BUTTON_GPIO_PIN, GPIO_INT_EDGE_BOTH);
    if(ret != 0) {
        LOG_ERR("%s: Failed to initialize button IRQ", __func__);
        return -1;
    }

    smf_set_initial(SMF_CTX(state), &system_states[RESET]);

    return 0;
}

int32_t state_update(state_t *state, event_t *evt)
{
    state->evt = evt;

    return smf_run_state(SMF_CTX(state));
}