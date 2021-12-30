#include <zephyr.h>
#include <init.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>

#include <hal/nrf_gpio.h>

#include "event_loop.h"
#include "bt_service.h"
#include "system_state.h"

LOG_MODULE_REGISTER(main);

#define SLEEP_TIME_MS			100
#define BUTTON_GPIO_PIN			DT_GPIO_PIN(DT_ALIAS(sw1), gpios)

static struct gpio_callback 	button_cb_data;
static event_loop_t 			event_loop;
static state_t 					state;

// Power button debounce variables (TODO: Add to gpio_callback or higher struct)
#define DEBOUNCE_CNT			5
#define DEBOUNCE_TIMER_PERIOD	K_MSEC(1)
static bool 					debounce = false;
static uint32_t 				debounce_cnt = DEBOUNCE_CNT;
static struct k_timer 			debounce_timer;

static void gpio_init();
static void button_event_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
static void debounce_timer_handler(struct k_timer *timer_id);
static void button_press_handler();

void main(void)
{
	LOG_INF("Starting main app thread ...");

	int32_t ret = 0;

	ret = event_loop_init(&event_loop);
	if(ret != 0) {
		LOG_ERR("Failed to initialize event loop");
		return;
	}

	gpio_init();
	state_init(&state);
		
	// Create a timer for the debounce code
	k_timer_init(&debounce_timer, debounce_timer_handler, NULL);
	
	event_t *event = NULL;
	while(1) 
	{
		event = event_loop_get_event(&event_loop);

		if(event != NULL) {
			ret = state_update(&state, event);
			if(ret) {
				LOG_ERR("Woops, something not right with state machine, exiting...");
				break;
			}

			event_loop_free_event(&event_loop, event);
		}
	}

	while (1) {
		k_msleep(SLEEP_TIME_MS);
	}
}

void gpio_init()
{
	int32_t ret;
	const struct device *dev_led = NULL;

	// Setup BTN PIN. For some reason Zephyr gpio driver functions do not setup
	// GPIO interrupt in a way, that system can be woken up from the POWER_OFF state.
	// So use NRF GPIO Hal functions
	// [TODO] Need to test it, but possible that I just need to use it for IRQ setup 
	nrf_gpio_cfg_input(DT_GPIO_PIN(DT_NODELABEL(button1), gpios),
					NRF_GPIO_PIN_PULLUP);

	const struct device *dev_btn = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(sw1), gpios));
	if(dev_btn == NULL) {
		LOG_ERR("BTN device is NULL");
		return;
	}

	gpio_init_callback(&button_cb_data, button_event_handler, BIT(BUTTON_GPIO_PIN));
	gpio_add_callback(dev_btn, &button_cb_data);


	// Setup STATE LED
	dev_led = device_get_binding(DT_GPIO_LABEL(DT_ALIAS(led0), gpios));
	if(dev_led == NULL) {
		LOG_ERR("LED0 device is NULL");
		return;
	}

	ret = gpio_pin_configure(dev_led, DT_GPIO_PIN(DT_ALIAS(led0), gpios), GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(DT_ALIAS(led0), gpios));
	if (ret < 0) {
		LOG_ERR("Failed to initialize 'Status LED' [%u]", ret);
		return;
	}
}

void button_event_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	debounce = true;
	debounce_cnt = DEBOUNCE_CNT;

	k_timer_start(&debounce_timer, DEBOUNCE_TIMER_PERIOD, DEBOUNCE_TIMER_PERIOD);
}

void debounce_timer_handler(struct k_timer *timer_id)
{
	if(debounce == true) {
		debounce_cnt--;
		if(debounce_cnt == 0) {
			debounce = false;
			debounce_cnt = DEBOUNCE_CNT;
			k_timer_stop(&debounce_timer);

			button_press_handler();
		}
	}
}

void button_press_handler()
{
	key_event_t *event = (key_event_t *)event_loop_alloc_event(&event_loop, EVENT_KEY);
	if(event == NULL) {
		LOG_ERR("Failed to allocate memory for the key event");
		return;
	}

	event->key_state = gpio_pin_get(state.dev_btn, BUTTON_GPIO_PIN);

	event_loop_put(&event_loop, (event_t *)event);
}