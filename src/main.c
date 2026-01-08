/* src/main.c */
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "hub.h"  
#include "protocol_defs.h" // <--- 1. Importante!

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Mapeamento de Estado (Logic State -> Protocol State) */
// Você vai precisar ajustar isso depois para usar pump_state_t nativamente
static uint32_t volume_infundido = 0;
static uint32_t vazao_configurada = 0;
static uint32_t pressao_atual = 0;
static uint8_t  estado_atual = STATE_IDLE; // Usando enum do protocol_defs.h

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void)
{
    if (device_is_ready(led.port)) {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    }
    
    LOG_INF("--- Sistema de Infusão Iniciado ---");

    if (hub_init() != 0) {
        LOG_ERR("Falha fatal no Hub SPI!");
        return 0;
    }

    while (1) 
    {
        // Atualiza status (cast simples por enquanto)
        hub_set_status(estado_atual, volume_infundido, vazao_configurada, pressao_atual, 0);

        /* 2. Mude para a struct interna */
        pump_cmd_t cmd_package; 
        
        if (hub_get_command(&cmd_package) == 0) 
        {
            LOG_INF("Main recebeu comando ID: %d Param: %d", cmd_package.id, (int)cmd_package.param);
            
            /* 3. Use os IDs internos (Desacoplados) */
            switch (cmd_package.id) {
                case CMD_START:
                    LOG_INF("-> Iniciando Infusão (Run)");
                    estado_atual = STATE_RUNNING;
                    break;

                case CMD_PAUSE:
                    LOG_INF("-> Pausando Infusão");
                    estado_atual = STATE_PAUSED;
                    break;
                
                case CMD_STOP:
                    LOG_INF("-> Parando Infusão");
                    estado_atual = STATE_IDLE;
                    break;

                case CMD_SET_BOLUS:
                    LOG_INF("-> BOLUS Solicitado!");
                    estado_atual = STATE_BOLUS;
                    break; 

                case CMD_SET_RATE:
                    vazao_configurada = (uint32_t)cmd_package.param;
                    LOG_INF("-> Config Vazão: %d ml/h", (int)cmd_package.param);
                    break;

                case CMD_SET_VOLUME:
                    LOG_INF("-> Config Volume Alvo: %d ml", (int)cmd_package.param);
                    break;

                default:
                    break;
            }
        }
        if (estado_atual == STATE_RUNNING || estado_atual == STATE_BOLUS) {
            volume_infundido++;
            gpio_pin_toggle_dt(&led); 
        } else {
            // Heartbeat lento em Idle/Pause
            if (k_uptime_get() % 1000 < 100) gpio_pin_set_dt(&led, 1);
            else gpio_pin_set_dt(&led, 0);
        }
        k_msleep(100); 
    }
    return 0;
}