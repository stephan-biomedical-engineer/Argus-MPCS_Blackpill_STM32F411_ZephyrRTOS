#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensor_data.h" 

LOG_MODULE_REGISTER(pump_logic, LOG_LEVEL_INF);

#define THRESHOLD_BUBBLE_MV    2000
#define THRESHOLD_OCCLUSION_MV 2800
#define VOLUME_HYSTERESIS      50

void logic_thread_entry(void *p1, void *p2, void *p3)
{
    sensor_packet_t data;
    int32_t last_volume = 0;

    LOG_INF("Logic Engine Started");

    while (1) 
    {
        // Espera dados chegarem na fila (Bloqueante)
        if (k_msgq_get(&sensor_data_q, &data, K_FOREVER) == 0) 
        {
            // Lógica de Bolha
            // if (data.bolha_mv < THRESHOLD_BUBBLE_MV) 
            // {
            //     LOG_ERR("EMERGÊNCIA: Ar na linha! (%d mV)", data.bolha_mv);
            //     // TODO: Parar Motor
            // }

            // // Lógica de Oclusão
            // if (data.oclusao_mv > THRESHOLD_OCCLUSION_MV) 
            // {
            //     LOG_WRN("ALERTA: Oclusão (%d mV)", data.oclusao_mv);
            // }

            // // Lógica de Volume (Histerese)
            // if (abs(data.volume_mv - last_volume) > VOLUME_HYSTERESIS) 
            // {
            //     last_volume = data.volume_mv;
            //     LOG_INF("Novo Volume: %d", data.volume_mv);
            // }
            // LOG_INF("Sensor Data - Bolha: %d mV, Oclusão: %d mV, Volume: %d, Timestamp: %d",
            //          data.bolha_mv, data.oclusao_mv, data.volume_mv, data.timestamp);
        }
    }
}

K_THREAD_DEFINE(logic_tid, 1024, logic_thread_entry, NULL, NULL, NULL, 3, 0, 0);