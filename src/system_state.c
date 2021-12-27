#include "system_state.h"

#include <zephyr.h>
#include <init.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <pm/pm.h>
#include <logging/log.h>
#include <smf.h>

#include <string.h>

#include <hal/nrf_gpio.h>

#define BTN_PRESSED 0
#define BTN_TICKS_THRESHOLD_EXIT_RESET  20
#define BTN_TICKS_THRESHOLD_SYSTEM_OFF  30

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

static struct s_object 
{
    struct smf_ctx ctx;
    const struct device *dev_led;
    const struct device *dev_btn;

    uint32_t btn_down_ticks;
} 
s_obj;

// ------------------------------------------------------
//   DEEP_SLEEP state
// ------------------------------------------------------
static void deep_sleep_entry(void *o)
{
    LOG_INF("%s", __func__);

    struct s_object *state = (struct s_object *)o;
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
    LOG_INF("%s: Should go sleep after this", __func__);
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

    struct s_object *state = (struct s_object *)o;
    if((state->dev_btn == NULL) || (state->dev_led == NULL)){
        LOG_WRN("Woops, something not right:%s", __func__);
        return;
    }

    // Turn status LED OFF
    gpio_pin_set(state->dev_led, DT_GPIO_PIN(DT_ALIAS(led0), gpios), 0);

    // Check if btn is still down, might be some glich or something or
    // power could be just connected.
    int32_t btn_state = gpio_pin_get(state->dev_btn, DT_GPIO_PIN(DT_ALIAS(sw1), gpios));
    if(btn_state != BTN_PRESSED) {
        LOG_INF("%s: Button is not pressed, going into DEEP SLEEP", __func__);
        smf_set_state(SMF_CTX(state), &system_states[DEEP_SLEEP]);
    }

    state->btn_down_ticks = 0;
}
static void reset_run(void *o)
{
    LOG_INF("%s", __func__);

    struct s_object *state = (struct s_object *)o;
    if((state->dev_btn == NULL) || (state->dev_led == NULL)){
        LOG_WRN("Woops, something not right:%s", __func__);
        return;
    }

    int32_t btn_state = gpio_pin_get(state->dev_btn, DT_GPIO_PIN(DT_ALIAS(sw1), gpios));
    if(btn_state == BTN_PRESSED) {
        state->btn_down_ticks++;
    }
    else {
        // If we are in the RESET state and button is UP, then
        // switch pack to DEEP SLEEP
        smf_set_state(SMF_CTX(state), &system_states[DEEP_SLEEP]);
    }

    if(state->btn_down_ticks >= BTN_TICKS_THRESHOLD_EXIT_RESET) {
        LOG_INF("Turning device full on");
        smf_set_state(SMF_CTX(state), &system_states[ADVERTISE]);
    }

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

    struct s_object *state = (struct s_object *)o;
    if((state->dev_btn == NULL) || (state->dev_led == NULL)){
        LOG_WRN("Woops, something not right:%s", __func__);
        return;
    }

    // Turn status LED ON
    gpio_pin_set(state->dev_led, DT_GPIO_PIN(DT_ALIAS(led0), gpios), 1);
}

static void advertise_run(void *o)
{
    struct s_object *state = (struct s_object *)o;
    if((state->dev_btn == NULL) || (state->dev_led == NULL)){
        LOG_WRN("Woops, something not right:%s", __func__);
        return;
    }

    int32_t btn_state = gpio_pin_get(state->dev_btn, DT_GPIO_PIN(DT_ALIAS(sw1), gpios));

    if(btn_state == BTN_PRESSED) {
        state->btn_down_ticks++;
    }
    else {
        state->btn_down_ticks = 0;
    }

    if(state->btn_down_ticks >= BTN_TICKS_THRESHOLD_SYSTEM_OFF) {
        LOG_INF("%s: Reached SYSTEM_OFF threshold", __func__);
        smf_set_state(SMF_CTX(state), &system_states[DEEP_SLEEP]);
    }
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

int32_t state_init()
{
    s_obj.dev_led = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(led0), gpios));
    if(s_obj.dev_led == NULL) {
        LOG_ERR("LED0 device is NULL");
        return -1;
    }

    s_obj.dev_btn = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(sw1), gpios));
    if(s_obj.dev_btn == NULL) {
        LOG_ERR("BTN device is NULL");
        return -1;
    }

    smf_set_initial(SMF_CTX(&s_obj), &system_states[RESET]);

    return 0;
}

int32_t state_update()
{
    return smf_run_state(SMF_CTX(&s_obj));
}