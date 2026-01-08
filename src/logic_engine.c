#include <zephyr/kernel.h>
#include "protocol_defs.h"
#include "sensor_data.h" 

K_MSGQ_DEFINE(cmd_queue, sizeof(pump_cmd_t), 10, 4);     
extern struct k_msgq sensor_data_q;                   

static pump_state_t current_state = STATE_POWER_ON;
static float volume_infused = 0.0f;
static float target_volume = 100.0f; 
static bool bubble_detection_enabled = true;

void update_spi_status(void) 
{
    pump_status_t status = 
    {
        .current_state = current_state,
        .infused_volume = volume_infused,
        // ... preencher resto
    };
    // hub_spi_update_buffer(&status); // Função hipotética do seu driver SPI
}

void logic_thread_entry(void *p1, void *p2, void *p3)
{
    struct k_poll_event events[2];
    pump_cmd_t cmd;
    sensor_packet_t sensor;

    k_poll_event_init(&events[0], K_POLL_TYPE_MSGQ_DATA_AVAILABLE, 
                      K_POLL_MODE_NOTIFY_ONLY, &cmd_queue);
    
    k_poll_event_init(&events[1], K_POLL_TYPE_MSGQ_DATA_AVAILABLE, 
                      K_POLL_MODE_NOTIFY_ONLY, &sensor_data_q);

    current_state = STATE_IDLE; 

    while (1) 
    {
        k_poll(events, 2, K_FOREVER);

        // --- 1. Chegou Comando do SPI/Celular? ---
        if (events[0].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE) {
            k_msgq_get(&cmd_queue, &cmd, K_NO_WAIT);
            
            switch (cmd.id) {
                case CMD_START:
                    if (current_state == STATE_IDLE || current_state == STATE_PAUSED) {
                        current_state = STATE_RUNNING;
                        // motor_start();
                    }
                    break;
                case CMD_PAUSE:
                    if (current_state == STATE_RUNNING || current_state == STATE_BOLUS) {
                        current_state = STATE_PAUSED;
                        // motor_stop();
                    }
                    break;
                case CMD_SET_BOLUS:
                    if (current_state == STATE_IDLE) {
                        current_state = STATE_BOLUS;
                        // motor_set_speed_high();
                    }
                    break;
                // ... Tratar outros comandos
            }
            events[0].state = K_POLL_STATE_NOT_READY; // Reset event
        }

        if (events[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE) 
        {
            k_msgq_get(&sensor_data_q, &sensor, K_NO_WAIT);

            if (volume_infused >= (target_volume * 0.7f)) 
            {
                bubble_detection_enabled = false;
            }

            if (bubble_detection_enabled && sensor.bolha_mv < 2000) 
            {
                current_state = STATE_ALARM_BUBBLE;
                // motor_emergency_stop();
            }
            
            if (sensor.oclusao_mv > 2800) {
                current_state = STATE_ALARM_OCCLUSION;
                // motor_emergency_stop();
            }

            if (current_state == STATE_RUNNING) 
            {
                // volume_infused += ...;
            }
            
            if (volume_infused >= target_volume && current_state == STATE_RUNNING) 
            {
                current_state = STATE_KVO;
                // motor_set_speed_kvo();
            }

            events[1].state = K_POLL_STATE_NOT_READY; 
        }

        update_spi_status();
    }
}
