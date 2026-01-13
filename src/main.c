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
    /* 1. Inicialização Básica */
    if (device_is_ready(led.port)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    }
    
    LOG_INF("--- BLACKPILL BOOT (v%u.%u.%u) ---", APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_PATCHLEVEL);
    boot_write_img_confirmed();

    /* 2. Inicializa Comunicação SPI */
    if (hub_init() != 0) {
        LOG_ERR("Falha fatal no Hub SPI!");
        // Opcional: Entrar em loop de erro piscando LED rápido
    }

    pump_cmd_t cmd_package;

    /* 3. Loop Principal (Gateway Hub -> Logic) */
    while (1) 
    {
        /* Verifica se o Hub recebeu algo via SPI */
        if (hub_get_command(&cmd_package) == 0) 
        {
            // LOG_INF("Main: Repassando comando ID %d para Logic Engine", cmd_package.id);
            gpio_pin_toggle_dt(&led);
            
            /* Envia para a fila da Logic Engine */
            /* K_NO_WAIT: Se a fila estiver cheia, descartamos (evita travar o main) */
            k_msgq_put(&cmd_queue, &cmd_package, K_NO_WAIT);
        }

        k_msleep(100); 
    }
    return 0;
}