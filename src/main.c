#include <zephyr.h>
#include <init.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>

#include <hal/nrf_gpio.h>

#include "bt_service.h"
#include "system_state.h"

LOG_MODULE_REGISTER(main);

#define SLEEP_TIME_MS   100

static void gpio_init();

void main(void)
{
	LOG_INF("Starting main app thread ...");

	gpio_init();
	state_init();
	
	// Start BT service
	// ret = bt_service_start();
	// if (ret < 0) {
	// 	LOG_INF("Failed to start BT service [%u]", ret);
	// 	return;
	// }

	while (1) {
		int32_t ret = state_update();
		if(ret) {
			LOG_ERR("Woops, something not right with state machine, exiting...");
			return;
		}

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
	// nrf_gpio_cfg_sense_set(DT_GPIO_PIN(DT_NODELABEL(button1), gpios),
	// 				NRF_GPIO_PIN_SENSE_LOW);

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