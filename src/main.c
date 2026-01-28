#include <zephyr/kernel.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_version.h>
#include "hub.h"
#include "protocol_defs.h"
#include "sensor_data.h"
#include "logic_engine.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void)
{
    if (device_is_ready(led.port))
    {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    }

    LOG_INF("--- BLACKPILL BOOT (v%u.%u.%u) ---",
            APP_VERSION_MAJOR,
            APP_VERSION_MINOR,
            APP_PATCHLEVEL);

    boot_write_img_confirmed();

    if (hub_init() != 0)
    {
        LOG_ERR("Falha fatal no Hub SPI!");
        return -1;
    }

    LOG_INF("Sistema inicializado. Threads ativas.");

    /* Nada mais a fazer.
       O sistema roda via threads. */
    while (1)
    {
        k_sleep(K_FOREVER);
    }
}

