#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_version.h>
#include <zephyr/sys/reboot.h>
#include <string.h>
#include <soc.h>
#include <stm32_ll_spi.h>
#include "hub.h"
#include "cmd.h"
#include "ota_handler.h"
#include "utl_io.h"
#include "sensor_data.h"

LOG_MODULE_REGISTER(hub, LOG_LEVEL_INF);

K_THREAD_DEFINE(hub_thread_data, HUB_THREAD_STACK_SIZE, hub_thread_entry, NULL, NULL, NULL, HUB_THREAD_PRIORITY, 0, 0);

static const struct device* spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi1));
static const struct gpio_dt_spec ready_pin = GPIO_DT_SPEC_GET(DT_ALIAS(spi_ready), gpios);

K_MSGQ_DEFINE(hub_cmd_q, sizeof(pump_cmd_t), 10, 4);

// --- DEFINIÇÕES DE FRAMING (SOF) ---
#define SOF_BYTE_1 0xAA
#define SOF_BYTE_2 0x55

#define SPI_PACKET_SIZE 64                     // Tamanho do chunk físico lido do SPI
static uint8_t rx_raw_buffer[SPI_PACKET_SIZE]; // Buffer DMA
static uint8_t tx_buffer[SPI_PACKET_SIZE];     // Buffer de resposta

// --- VARIÁVEIS DO PARSER (MÁQUINA DE ESTADOS) ---
typedef enum
{
    STATE_WAIT_SOF1,
    STATE_WAIT_SOF2,
    STATE_READ_HEADER,
    STATE_READ_PAYLOAD,
    STATE_READ_CRC
} parser_state_t;

static parser_state_t p_state = STATE_WAIT_SOF1;
static uint8_t p_buffer[CMD_MAX_DATA_SIZE + 16]; // Buffer de montagem
static uint16_t p_index = 0;
static uint16_t p_expected_len = 0;

static int spi_consecutive_errors = 0;

static const struct spi_config spi_cfg = 
{
    // Modo 0: CPOL=0, CPHA=0
    .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_SLAVE,
    .frequency = 1000000,
    .slave = 0,
};

static pump_status_t status_cache;

// --- RESET DE HARDWARE (Auto-Cura) ---
static void reset_spi_peripheral(void)
{
    SPI_TypeDef* spi_regs = (SPI_TypeDef*) DT_REG_ADDR(DT_NODELABEL(spi1));

    // Sequência de Reset Seguro
    spi_regs->CR1 &= ~SPI_CR1_SPE; // Desabilita
    k_busy_wait(100);

    // Limpa Flags
    volatile uint32_t temp;
    temp = spi_regs->DR;
    temp = spi_regs->SR;
    (void) temp;

    spi_regs->CR1 |= SPI_CR1_SPE; // Habilita
    LOG_WRN(">>> SPI HARDWARE RESET <<<");
}

int hub_init(void)
{
    if(!device_is_ready(spi_dev))
        return -1;

    memset(tx_buffer, 0, sizeof(tx_buffer));
    memset(rx_raw_buffer, 0, sizeof(rx_raw_buffer));
    memset(&status_cache, 0, sizeof(status_cache));

    if(device_is_ready(ready_pin.port))
    {
        gpio_pin_configure_dt(&ready_pin, GPIO_OUTPUT_INACTIVE);
    }
    return 0;
}

static void fill_status_payload(cmd_status_payload_t* payload)
{
    payload->current_state = (uint8_t) status_cache.current_state;
    payload->volume = (uint32_t) status_cache.infused_volume;
    payload->flow_rate_set = status_cache.configured_flow_rate;
    payload->pressure = status_cache.pressure_mmhg;
    payload->alarm_active = (status_cache.current_state >= STATE_ALARM_BUBBLE);
}

void hub_set_status(const pump_status_t* status)
{
    status_cache = *status;
}
int hub_get_command(pump_cmd_t* cmd)
{
    return k_msgq_get(&hub_cmd_q, cmd, K_NO_WAIT);
}

// --- PROCESSADOR DE PACOTE VÁLIDO ---
static void process_valid_packet(uint8_t* buffer, size_t len)
{
    // Zera erros pois tivemos sucesso
    if(spi_consecutive_errors > 0)
    {
        LOG_INF("SPI Recuperado! Erros zerados.");
        spi_consecutive_errors = 0;
    }

    uint8_t src, dst;
    cmd_ids_t req_id, res_id;
    cmd_cmds_t req_data, res_data;
    size_t tx_len = 0;

    memset(&res_data, 0, sizeof(res_data));

    // O 'buffer' aqui começa no byte 0 (que agora é SOF1 0xAA) graças à lógica do parser.
    bool decode_success = cmd_decode(buffer, len, &src, &dst, &req_id, &req_data);

    if(!decode_success)
    {
        LOG_WRN("Erro logico no cmd_decode (Estrutura invalida)");
        return;
    }

    pump_cmd_t internal_cmd = {.id = CMD_NONE, .param = 0.0f};

    switch(req_id)
    {
    case CMD_GET_STATUS_REQ_ID:
        res_id = CMD_GET_STATUS_RES_ID;
        fill_status_payload(&res_data.status_res.status_data);
        break;

    case CMD_VERSION_REQ_ID:
        res_id = CMD_VERSION_RES_ID;
        res_data.version_res.major = APP_VERSION_MAJOR;
        res_data.version_res.minor = APP_VERSION_MINOR;
        res_data.version_res.patch = APP_PATCHLEVEL;
        break;

    case CMD_SET_CONFIG_REQ_ID:
        internal_cmd.id = CMD_SET_RATE;
        internal_cmd.param = (float) req_data.config_req.config.flow_rate;
        k_msgq_put(&hub_cmd_q, &internal_cmd, K_NO_WAIT);
        internal_cmd.id = CMD_SET_VOLUME;
        internal_cmd.param = (float) req_data.config_req.config.volume;
        k_msgq_put(&hub_cmd_q, &internal_cmd, K_NO_WAIT);
        internal_cmd.id = CMD_SET_DIAMETER;
        internal_cmd.param = (float) req_data.config_req.config.diameter;
        k_msgq_put(&hub_cmd_q, &internal_cmd, K_NO_WAIT);
        internal_cmd.id = CMD_SET_MODE;
        internal_cmd.param = (float) req_data.config_req.config.mode;
        k_msgq_put(&hub_cmd_q, &internal_cmd, K_NO_WAIT);

        res_id = CMD_SET_CONFIG_RES_ID;
        res_data.config_res.status = CMD_OK;
        break;

    // --- LÓGICA OTA RESTAURADA E CORRIGIDA PARA HEADER V2 ---
    case CMD_OTA_START_REQ_ID: {
        // Payload começa no byte 7 (Header Size)
        // Estrutura: [Size (4 bytes)]
        uint32_t size = utl_io_get32_fl(&buffer[CMD_HDR_SIZE]);

        LOG_INF("Comando OTA START Recebido. Tamanho: %d", size);
        ota_start(size);

        res_id = CMD_OTA_RES_ID;
        res_data.action_res.cmd_req_id = req_id;
        res_data.action_res.status = CMD_OK;
        break;
    }

    case CMD_OTA_CHUNK_REQ_ID: {
        // Payload começa no byte 7
        // Estrutura Chunk: [Offset (4)] + [Len (1)] + [Data...]
        uint8_t* payload_ptr = &buffer[CMD_HDR_SIZE];

        // Offset está nos primeiros 4 bytes (indices 0-3 do payload)
        // uint32_t offset = utl_io_get32_fl(payload_ptr); // Não usamos aqui, mas existe

        // Len está no byte 4 do payload
        uint8_t chunk_len = payload_ptr[4];

        // Data começa no byte 5 do payload
        uint8_t* data_ptr = &payload_ptr[5];

        if(ota_write_chunk(data_ptr, chunk_len) == 0)
        {
            res_data.action_res.status = CMD_OK;
        }
        else
        {
            res_data.action_res.status = CMD_ERR_INVALID_STATE;
        }
        res_id = CMD_OTA_RES_ID;
        res_data.action_res.cmd_req_id = req_id;
        break;
    }

    case CMD_OTA_END_REQ_ID: {
        LOG_INF("Comando OTA END Recebido.");
        ota_finish();
        res_id = CMD_OTA_RES_ID;
        res_data.action_res.cmd_req_id = req_id;
        res_data.action_res.status = CMD_OK;
        break;
    }
        // ----------------------------------------------------

    default:
        res_id = CMD_ACTION_RES_ID;
        res_data.action_res.status = CMD_OK;
        res_data.action_res.cmd_req_id = req_id;

        if(req_id >= CMD_ACTION_RUN_REQ_ID && req_id <= CMD_ACTION_BOLUS_REQ_ID)
        {
            switch(req_id)
            {
            case CMD_ACTION_RUN_REQ_ID:
                internal_cmd.id = CMD_START;
                break;
            case CMD_ACTION_PAUSE_REQ_ID:
                internal_cmd.id = CMD_PAUSE;
                break;
            case CMD_ACTION_ABORT_REQ_ID:
                internal_cmd.id = CMD_STOP;
                break;
            case CMD_ACTION_BOLUS_REQ_ID:
                internal_cmd.id = CMD_SET_BOLUS;
                break;
            case CMD_ACTION_PURGE_REQ_ID:
                internal_cmd.id = CMD_SET_PURGE;
                break;
            default:
                internal_cmd.id = CMD_NONE;
                break;
            }
            if(internal_cmd.id != CMD_NONE)
            {
                internal_cmd.param = 0.0f;
                k_msgq_put(&hub_cmd_q, &internal_cmd, K_NO_WAIT);
            }
        }
        break;
    }

    // Prepara a resposta para a PRÓXIMA transação SPI
    cmd_encode(tx_buffer, &tx_len, &dst, &src, &res_id, &res_data);
}

// --- MÁQUINA DE ESTADOS (FEEDER) ---
void protocol_feed_byte(uint8_t byte)
{
    switch(p_state)
    {
    case STATE_WAIT_SOF1:
        if(byte == SOF_BYTE_1)
            p_state = STATE_WAIT_SOF2;
        break;

    case STATE_WAIT_SOF2:
        if(byte == SOF_BYTE_2)
        {
            p_state = STATE_READ_HEADER;
            p_index = 0;
            // Salva o SOF no buffer para validação do CRC depois
            p_buffer[p_index++] = SOF_BYTE_1;
            p_buffer[p_index++] = SOF_BYTE_2;
        }
        else if(byte == SOF_BYTE_1)
        {
            p_state = STATE_WAIT_SOF2; // Sequência AA AA...
        }
        else
        {
            p_state = STATE_WAIT_SOF1; // Lixo
        }
        break;

    case STATE_READ_HEADER:
        p_buffer[p_index++] = byte;
        // Header(7 bytes) = SOF1+SOF2 + 5 bytes restantes
        if(p_index >= CMD_HDR_SIZE)
        {
            // Extrai o tamanho esperado do payload
            // O campo size está nos últimos 2 bytes do header (indices 5 e 6)
            p_expected_len = utl_io_get16_fl(&p_buffer[5]);

            if(p_expected_len > CMD_MAX_DATA_SIZE)
            {
                p_state = STATE_WAIT_SOF1; // Tamanho inválido
            }
            else
            {
                p_state = STATE_READ_PAYLOAD;
            }
        }
        break;

    case STATE_READ_PAYLOAD:
        // Lê bytes de Payload + CRC
        if(p_expected_len > 0 || p_index < (CMD_HDR_SIZE + CMD_TRAILER_SIZE))
        {
            p_buffer[p_index++] = byte;
        }

        // Verifica se completou o frame (Header + Payload + CRC)
        if(p_index >= (CMD_HDR_SIZE + p_expected_len + CMD_TRAILER_SIZE))
        {
            process_valid_packet(p_buffer, p_index);
            p_state = STATE_WAIT_SOF1; // Volta a caçar
        }
        break;

    default:
        p_state = STATE_WAIT_SOF1;
        break;
    }
}

void hub_thread_entry(void* p1, void* p2, void* p3)
{
    LOG_INF("Hub Parser (Stream Mode) Iniciado.");

    while(1)
    {
        struct spi_buf rx_buf = {.buf = rx_raw_buffer, .len = SPI_PACKET_SIZE};
        struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1};

        struct spi_buf tx_buf_s = {.buf = tx_buffer, .len = SPI_PACKET_SIZE};
        struct spi_buf_set tx_set = {.buffers = &tx_buf_s, .count = 1};

        gpio_pin_set_dt(&ready_pin, 1);
        int ret = spi_transceive(spi_dev, &spi_cfg, &tx_set, &rx_set);
        gpio_pin_set_dt(&ready_pin, 0);

        // --- TRATAMENTO DE ERRO FÍSICO ---
        if(ret < 0)
        {
            LOG_ERR("Erro SPI Driver: %d", ret);
            spi_consecutive_errors++;

            // Tenta curar
            reset_spi_peripheral();

            if(spi_consecutive_errors > 10) // 10 erros seguidos = Morte
            {
                LOG_ERR("FALHA CRITICA: Reiniciando Sistema...");
                k_sleep(K_MSEC(200));
                sys_reboot(SYS_REBOOT_COLD);
            }
            k_sleep(K_MSEC(10));
            continue;
        }

        // --- ALIMENTA O PARSER ---
        for(int i = 0; i < SPI_PACKET_SIZE; i++)
        {
            protocol_feed_byte(rx_raw_buffer[i]);
        }

        memset(rx_raw_buffer, 0, SPI_PACKET_SIZE);
        ota_check_and_reboot();
    }
}
