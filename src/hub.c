#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "hub.h"
#include "cmd.h"
#include "utl_io.h"

LOG_MODULE_REGISTER(hub, LOG_LEVEL_INF); 

K_THREAD_DEFINE(hub_thread_data, HUB_THREAD_STACK_SIZE,
                    hub_thread_entry,
                    NULL, NULL, NULL,
                    HUB_THREAD_PRIORITY, 0, 0);

static const struct device *spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi1));
static const struct gpio_dt_spec ready_pin = GPIO_DT_SPEC_GET(DT_ALIAS(spi_ready), gpios);

K_MSGQ_DEFINE(hub_cmd_q, sizeof(pump_cmd_t), 10, 4);

#define HUB_BUFFER_SIZE 300 
#define SPI_PACKET_SIZE 64
static uint8_t rx_buffer[HUB_BUFFER_SIZE];
static uint8_t tx_buffer[HUB_BUFFER_SIZE];

static const struct spi_config spi_cfg = 
{
    .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_SLAVE | SPI_MODE_CPOL | SPI_MODE_CPHA,
    .frequency = 1000000,
    .slave = 0,
};

static cmd_status_payload_t status_cache;

int hub_init(void)
{
    if (!device_is_ready(spi_dev)) return -1;

    memset(tx_buffer, 0, sizeof(tx_buffer));
    memset(rx_buffer, 0, sizeof(rx_buffer));
    memset(&status_cache, 0, sizeof(status_cache));

    if (device_is_ready(ready_pin.port)) {
        gpio_pin_configure_dt(&ready_pin, GPIO_OUTPUT_INACTIVE);
    }
    return 0;
}

void hub_set_status(uint8_t state, uint32_t volume, uint32_t flow_set, uint32_t pressure, uint8_t alarm)
{
    status_cache.current_state = state;
    status_cache.volume = volume;
    status_cache.flow_rate_set = flow_set;
    status_cache.pressure = pressure;
    status_cache.alarm_active = alarm;
}

int hub_get_command(pump_cmd_t *cmd) 
{
    return k_msgq_get(&hub_cmd_q, cmd, K_NO_WAIT);
}

void hub_thread_entry(void *p1, void *p2, void *p3) 
{
    while(1)
    {
        struct spi_buf rx_buf = { .buf = rx_buffer, .len = SPI_PACKET_SIZE };
        struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };

        gpio_pin_set_dt(&ready_pin, 1);
        int ret = spi_transceive(spi_dev, &spi_cfg, NULL, &rx_set);
        gpio_pin_set_dt(&ready_pin, 0);

        if (ret < 0) continue; 

        uint16_t payload_size = utl_io_get16_fl(&rx_buffer[3]); 
        if (payload_size > CMD_MAX_DATA_SIZE) payload_size = CMD_MAX_DATA_SIZE;

        size_t total_valid_len = CMD_HDR_SIZE + payload_size + CMD_TRAILER_SIZE;
        uint8_t src, dst;
        cmd_ids_t req_id, res_id;
        cmd_cmds_t req_data, res_data;
        size_t tx_len = 0;

        memset(&res_data, 0, sizeof(res_data));

        bool decode_success = cmd_decode(rx_buffer, total_valid_len, &src, &dst, &req_id, &req_data);
        
        if(decode_success)
        {            
            pump_cmd_t internal_cmd = 
            { 
                .id = CMD_NONE, 
                .param = 0.0f 
            };
            
            switch(req_id) 
            {
                case CMD_GET_STATUS_REQ_ID:
                    res_id = CMD_GET_STATUS_RES_ID;
                    res_data.status_res.status_data = status_cache;
                    break;
                
                case CMD_VERSION_REQ_ID:
                    res_id = CMD_VERSION_RES_ID;
                    res_data.version_res.major = 1;
                    res_data.version_res.minor = 2;
                    res_data.version_res.patch = 3;
                    break;

                case CMD_SET_CONFIG_REQ_ID:                    
                    internal_cmd.id = CMD_SET_RATE;
                    internal_cmd.param = (float)req_data.config_req.config.flow_rate; 
                    k_msgq_put(&hub_cmd_q, &internal_cmd, K_NO_WAIT);

                    internal_cmd.id = CMD_SET_VOLUME; 
                    internal_cmd.param = (float)req_data.config_req.config.volume;
                    k_msgq_put(&hub_cmd_q, &internal_cmd, K_NO_WAIT);

                    res_id = CMD_SET_CONFIG_RES_ID;
                    res_data.config_res.status = CMD_OK;
                    break;

                default:
                    res_id = CMD_ACTION_RES_ID;
                    res_data.action_res.status = CMD_OK;
                    res_data.action_res.cmd_req_id = req_id;
                    
                    if (req_id >= CMD_ACTION_RUN_REQ_ID && req_id <= CMD_ACTION_BOLUS_REQ_ID) 
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

                        if (internal_cmd.id != CMD_NONE) 
                        {
                            internal_cmd.param = 0.0f;
                            k_msgq_put(&hub_cmd_q, &internal_cmd, K_NO_WAIT);
                        }
                    }
                    break;
            }
            cmd_encode(tx_buffer, &tx_len, &dst, &src, &res_id, &res_data);
        }
        else 
        {
            res_id = CMD_GET_STATUS_RES_ID;
            uint8_t err_src = ADDR_SLAVE;
            uint8_t err_dst = ADDR_MASTER;
            res_data.status_res.status_data = status_cache;
            cmd_encode(tx_buffer, &tx_len, &err_dst, &err_src, &res_id, &res_data);
        }

        if (SPI_PACKET_SIZE > tx_len) 
        {
            memset(&tx_buffer[tx_len], 0, SPI_PACKET_SIZE - tx_len);
        }

        struct spi_buf tx_buf_s = { .buf = tx_buffer, .len = SPI_PACKET_SIZE };
        struct spi_buf_set tx_set = { .buffers = &tx_buf_s, .count = 1 };
        
        gpio_pin_set_dt(&ready_pin, 1);
        spi_transceive(spi_dev, &spi_cfg, &tx_set, NULL);
        gpio_pin_set_dt(&ready_pin, 0);
    }
}

