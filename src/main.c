/* src/main.c */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "hub.h"  

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static uint32_t volume_infundido = 0;
static uint32_t pressao_atual = 0;
static uint8_t  estado_atual = STATE_OFF; 

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void)
{
    if (device_is_ready(led.port)) 
    {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    }
    
    LOG_INF("--- Sistema de Infusão Iniciado ---");

    if (hub_init() != 0) 
    {
        LOG_ERR("Falha fatal no Hub SPI!");
        return 0;
    }


    while (1) 
    {
        hub_set_status(estado_atual, volume_infundido, pressao_atual, 0);

        cmd_ids_t cmd;
        if (hub_get_command(&cmd) == 0) 
        {
            
            LOG_INF("Main recebeu comando ID: %d", cmd);
            
            switch (cmd) {
                case CMD_ACTION_RUN_REQ_ID:
                    LOG_INF("-> Iniciando Infusão");
                    estado_atual = STATE_RUNNING;
                    break;

                case CMD_ACTION_PAUSE_REQ_ID:
                    LOG_INF("-> Pausando Infusão");
                    estado_atual = STATE_PAUSED;
                    break;
                
                case CMD_ACTION_BOLUS_REQ_ID:
                    LOG_INF("-> BOLUS Solicitado!");
                    break;                    
                default:
                    break;
            }
        }

        if (estado_atual == STATE_RUNNING) 
        {
            volume_infundido++;
            gpio_pin_toggle_dt(&led); 
        } else {
            if (k_uptime_get() % 1000 < 100) gpio_pin_set_dt(&led, 1);
            else gpio_pin_set_dt(&led, 0);
        }
        k_msleep(100); 
    }
    return 0;
}