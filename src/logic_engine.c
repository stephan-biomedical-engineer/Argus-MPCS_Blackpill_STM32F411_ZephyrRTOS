#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensor_data.h"
#include "encoder.h"
#include "motor_driver.h" 
#include "hub.h"    

LOG_MODULE_REGISTER(logic_engine, LOG_LEVEL_INF);

/* Definição das Filas */
K_MSGQ_DEFINE(cmd_queue, sizeof(pump_cmd_t), 10, 4);
/* A sensor_data_q já foi definida no adc_driver.c ou aqui. 
   Se estiver no adc_driver, use 'extern'. Vamos assumir extern aqui */
extern struct k_msgq sensor_data_q; 

/* Estado Global da Aplicação (ÚNICA FONTE DE VERDADE) */
static pump_status_t global_status = {
    .current_state = STATE_IDLE,
    .infused_volume = 0.0f,
    .target_volume = 100.0f,
    .configured_flow_rate = 0,
    .pressure_mmhg = 0
};

/* Calibração: ml por grau do encoder (Exemplo) */
#define ML_PER_DEGREE 0.005f 

static bool test_mode_warned = false;

/* Adicione no topo do logic_engine.c */
#define RATE_PURGE_DEFAULT  1200 // ml/h máxima
#define RATE_BOLUS_DEFAULT  600  // ml/h padrão de bolus

/* Precisamos saber qual vazão aplicar dependendo do estado */
uint32_t get_target_rate(pump_status_t *status) {
    switch (status->current_state) {
        case STATE_RUNNING: return status->configured_flow_rate;
        case STATE_BOLUS:   return RATE_BOLUS_DEFAULT; 
        case STATE_PURGE:   return RATE_PURGE_DEFAULT;
        default: return 0;
    }
}

void update_motor_hardware(pump_status_t *status) 
{
    // Verifica se é QUALQUER estado de movimento
    if (status->current_state == STATE_RUNNING || 
        status->current_state == STATE_BOLUS   || 
        status->current_state == STATE_PURGE) 
    {
        motor_enable(true);
        
        // Pega a vazão correta para o estado atual
        uint32_t rate = get_target_rate(status);
        
        motor_run(rate, status->syringe_diameter);
        
        LOG_INF("Motor ON: Estado=%d, Vazao=%d ml/h", status->current_state, rate);
    } 
    else 
    {
        motor_stop();
        motor_enable(false);
        LOG_INF("Motor OFF: Estado=%d", status->current_state);
    }
}

void logic_thread_entry(void *p1, void *p2, void *p3)
{
    struct k_poll_event events[2];
    pump_cmd_t cmd;
    sensor_packet_t sensor;
    
    /* Inicializa Hardware Específico */
    encoder_init();
    motor_init();

    LOG_INF("Logic Engine Iniciada. Aguardando comandos...");

    /* Configura eventos de poll */
    k_poll_event_init(&events[0], K_POLL_TYPE_MSGQ_DATA_AVAILABLE, 
                      K_POLL_MODE_NOTIFY_ONLY, &cmd_queue);
    
    k_poll_event_init(&events[1], K_POLL_TYPE_MSGQ_DATA_AVAILABLE, 
                      K_POLL_MODE_NOTIFY_ONLY, &sensor_data_q);

    while (1) 
    {
        /* Espera por algo acontecer (Comando ou Sensor) */
        k_poll(events, 2, K_MSEC(100)); // Timeout de 100ms para garantir leitura do encoder

        /* --- 1. Processamento de Comandos (Vindo do SPI/Hub) --- */
        if (events[0].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE) 
        {
            k_msgq_get(&cmd_queue, &cmd, K_NO_WAIT);
            
            LOG_INF("CMD Proc: ID=%d Param=%d", cmd.id, (int)cmd.param);

            bool state_changed = false;

            switch (cmd.id) 
            {
                case CMD_START:
                    // Aceita Start se estiver parado ou pausado
                    if (global_status.current_state == STATE_IDLE || 
                        global_status.current_state == STATE_PAUSED) 
                    {
                        global_status.current_state = STATE_RUNNING;
                        state_changed = true;
                    }
                    break;

                case CMD_PAUSE:
                    // PAUSE deve funcionar em QUALQUER estado de movimento
                    if (global_status.current_state == STATE_RUNNING ||
                        global_status.current_state == STATE_BOLUS   ||
                        global_status.current_state == STATE_PURGE) 
                    {
                        global_status.current_state = STATE_PAUSED;
                        state_changed = true;
                    }
                    break;

                case CMD_STOP:
                    global_status.current_state = STATE_IDLE;
                    global_status.infused_volume = 0.0f; // Reseta volume
                    state_changed = true;
                    break;
                
                // --- NOVOS CASES (Isso faltava!) ---
                case CMD_SET_BOLUS:
                    if (global_status.current_state == STATE_IDLE || global_status.current_state == STATE_PAUSED) 
                    {
                        global_status.current_state = STATE_BOLUS;
                        state_changed = true;
                    }
                    break;

                case CMD_SET_PURGE:
                    if (global_status.current_state == STATE_IDLE || global_status.current_state == STATE_PAUSED) 
                    {
                        global_status.current_state = STATE_PURGE;
                        state_changed = true;
                    }
                    break;
                // -----------------------------------

                case CMD_SET_RATE:
                    // IMPORTANTE: Isso muda apenas a vazão do modo RUNNING
                    global_status.configured_flow_rate = (uint32_t)cmd.param;
                    LOG_INF("Vazão RUN configurada para: %d", global_status.configured_flow_rate);
                    // Se já estiver rodando em modo RUN, atualiza motor na hora
                    if (global_status.current_state == STATE_RUNNING) state_changed = true;
                    break;

                case CMD_SET_VOLUME:
                    global_status.target_volume = (float)cmd.param;
                    break;                
                case CMD_SET_DIAMETER:
                    global_status.syringe_diameter = (uint8_t)cmd.param;
                    break;
                case CMD_SET_MODE:
                    global_status.infusion_mode = (uint8_t)cmd.param;
                    break;
                default:
                    LOG_WRN("Comando desconhecido ou não tratado: %d", cmd.id);
                    break;
            }
            
            events[0].state = K_POLL_STATE_NOT_READY;
            
            // Só chama o driver se houve mudança relevante
            if (state_changed) {
                update_motor_hardware(&global_status);
            }
        }

        /* --- 2. Processamento de Sensores Analógicos (Bolha/Oclusão) --- */
        if (events[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE) 
        {
            k_msgq_get(&sensor_data_q, &sensor, K_NO_WAIT);

            /* --- MODO DE TESTE (Bypass de Hardware) --- */
            /* Sobrescrevemos os valores lidos com valores fixos seguros */
            sensor.bolha_mv = 3000;  // > 2000 (Sem bolha)
            sensor.oclusao_mv = 0;   // 0 pressão
            
            if (!test_mode_warned) 
            {
                LOG_WRN("MODO TESTE ATIVO: Sensores simulados via software (Log único para evitar spam)");
                test_mode_warned = true;
            }
                
            // Atualiza pressão baseada no sensor de oclusão (calibração necessária)
            global_status.pressure_mmhg = sensor.oclusao_mv / 10; 

            // Lógica de Segurança
            if (sensor.bolha_mv < 2000 && global_status.current_state == STATE_RUNNING) 
            {
                LOG_ERR("BOLHA DETECTADA! Parando sistema.");
                global_status.current_state = STATE_ALARM_BUBBLE;
                update_motor_hardware(&global_status);
            }

            events[1].state = K_POLL_STATE_NOT_READY;
        }

        int32_t delta = encoder_get_delta();

        if (delta != 0) {
            /* CENÁRIO A: O Encoder é um botão de ajuste (Knob) no painel */
            if (global_status.current_state == STATE_IDLE || global_status.current_state == STATE_PAUSED) {
                global_status.configured_flow_rate += delta;
                LOG_INF("Ajuste de Vazão: %d ml/h", global_status.configured_flow_rate);
            }
            
            /* CENÁRIO B: O Encoder está no motor (Feedback de movimento) */
            /* Se o motor girou, calculamos o volume infundido real */
             if (global_status.current_state == STATE_RUNNING) {
                 float vol_increment = (float)delta * ML_PER_DEGREE;
                 // Só soma se for positivo (sentido da infusão)
                 if (vol_increment > 0) {
                     global_status.infused_volume += vol_increment;
                 }
                 
                 // Verifica se atingiu o alvo (VTBI - Volume To Be Infused)
                 if (global_status.infused_volume >= global_status.target_volume) {
                     LOG_INF("Volume Alvo Atingido!");
                     global_status.current_state = STATE_END_INFUSION;
                     update_motor_hardware(&global_status); // Para o motor
                 }
             }
        }

        /* --- 4. Atualizar o Hub SPI --- */
        // Envia o estado atualizado para que o Hub possa responder ao próximo poll do Mestre
        hub_set_status(&global_status);
    }
}

K_THREAD_DEFINE(logic_tid, 2048, logic_thread_entry, NULL, NULL, NULL, 3, 0, 0);