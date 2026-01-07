#ifndef CMD_H
#define CMD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Configurações de Tamanho */
#define CMD_MAX_DATA_SIZE  250
#define CMD_HDR_SIZE       5
#define CMD_TRAILER_SIZE   2
#define FRAME_MAX_CMD_SIZE (CMD_HDR_SIZE + CMD_MAX_DATA_SIZE + CMD_TRAILER_SIZE)
#define CMD_INVALID_ID     255
#define ADDR_MASTER        0x00 
#define ADDR_SLAVE         0x01 

/* * ENUMS: Mantidos para uso lógico no código (Switch/Case),
 * mas NÃO devem ser usados diretamente dentro das structs de rede (packed).
 */
typedef enum cmd_ids_e
{
     CMD_VERSION_REQ_ID                  = 0x01,
     CMD_VERSION_RES_ID                  = 0x02,
     CMD_GET_STATUS_REQ_ID               = 0x03, 
     CMD_GET_STATUS_RES_ID               = 0x04,
     CMD_SET_CONFIG_REQ_ID               = 0x10, 
     CMD_SET_CONFIG_RES_ID               = 0x11, 
     CMD_ACTION_RUN_REQ_ID               = 0x20, 
     CMD_ACTION_PAUSE_REQ_ID             = 0x21, 
     CMD_ACTION_ABORT_REQ_ID             = 0x22, 
     CMD_ACTION_PURGE_REQ_ID             = 0x23, 
     CMD_ACTION_BOLUS_REQ_ID             = 0x24, 
     CMD_ACTION_RES_ID                   = 0x2F, 
} cmd_ids_t;

typedef enum 
{
    CMD_OK = 0, 
    CMD_ERR_INVALID_STATE, 
    CMD_ERR_PARAM_RANGE,     
    CMD_ERR_UNKNOWN_CMD, 
    CMD_ERR_CHECKSUM,
} cmd_status_t;

/* --- ESTRUTURAS DE PACOTE (WIRE FORMAT) --- */
/* Todos os campos são tipos de tamanho fixo (uint8_t, uint16_t, uint32_t) */

typedef struct __attribute__((packed)) cmd_hdr_s 
{
    uint8_t dst; 
    uint8_t src; 
    uint8_t id;    /* Mapeia para cmd_ids_t */
    uint16_t size; /* Payload Size */
} cmd_hdr_t;

typedef struct __attribute__((packed)) cmd_trl_s 
{ 
    uint16_t crc; 
} cmd_trl_t;

/* --- PAYLOADS --- */

typedef struct cmd_version_req_s { } cmd_version_req_t;

typedef struct __attribute__((packed)) cmd_version_res_s 
{ 
    uint8_t major; 
    uint8_t minor; 
    uint8_t patch; 
} cmd_version_res_t;

typedef struct __attribute__((packed)) cmd_config_payload_s 
{ 
    uint32_t volume; 
    uint32_t flow_rate; 
    uint8_t diameter; 
    uint8_t mode; 
} cmd_config_payload_t;

typedef struct __attribute__((packed)) cmd_set_config_req_s 
{ 
    cmd_config_payload_t config; 
} cmd_set_config_req_t;

typedef struct __attribute__((packed)) cmd_set_config_res_s 
{ 
    uint8_t status; /* [CORRIGIDO] Era cmd_status_t. Agora é uint8_t (1 byte fixo) */
} cmd_set_config_res_t;

typedef struct __attribute__((packed)) cmd_status_payload_s 
{ 
    uint8_t current_state; /* Mapeia para pump_state_t */
    uint32_t volume; 
    uint32_t pressure; 
    uint8_t alarm_active; 
} cmd_status_payload_t;

typedef struct cmd_get_status_req_s { } cmd_get_status_req_t;

typedef struct __attribute__((packed)) cmd_get_status_res_s 
{ 
    cmd_status_payload_t status_data; 
} cmd_get_status_res_t;

/* Comandos de Ação (Payload Vazio) */
typedef struct cmd_action_run_req_s { } cmd_action_run_req_t;
typedef struct cmd_action_pause_req_s { } cmd_action_pause_req_t;
typedef struct cmd_action_abort_req_s { } cmd_action_abort_req_t;
typedef struct cmd_action_purge_req_s { } cmd_action_purge_req_t;
typedef struct cmd_action_bolus_req_s { } cmd_action_bolus_req_t;

typedef struct __attribute__((packed)) cmd_action_res_s 
{ 
    uint8_t cmd_req_id; /* [CORRIGIDO] Era cmd_ids_t. Agora é uint8_t. */
    uint8_t status;     /* [CORRIGIDO] Era cmd_status_t. Agora é uint8_t. */
} cmd_action_res_t;

/* --- TAMANHOS ESPERADOS --- */
typedef enum cmd_sizes_e {
    CMD_VERSION_REQ_SIZE        = 0,
    CMD_VERSION_RES_SIZE        = sizeof(cmd_version_res_t),
    CMD_GET_STATUS_REQ_SIZE     = 0,
    CMD_GET_STATUS_RES_SIZE     = sizeof(cmd_get_status_res_t),
    CMD_SET_CONFIG_REQ_SIZE     = sizeof(cmd_set_config_req_t),
    CMD_SET_CONFIG_RES_SIZE     = sizeof(cmd_set_config_res_t),    
    CMD_ACTION_REQ_SIZE         = 0, 
    CMD_ACTION_RES_SIZE         = sizeof(cmd_action_res_t),
} cmd_sizes_t;

/* União para facilitar encode/decode genérico */
typedef union cmd_cmds_u {
    cmd_version_req_t       version_req;
    cmd_version_res_t       version_res;
    cmd_get_status_req_t    status_req;
    cmd_get_status_res_t    status_res;
    cmd_set_config_req_t    config_req;
    cmd_set_config_res_t    config_res;
    cmd_action_run_req_t    run_req;
    cmd_action_pause_req_t  pause_req;
    cmd_action_abort_req_t  abort_req;
    cmd_action_purge_req_t  purge_req;
    cmd_action_bolus_req_t  bolus_req;
    cmd_action_res_t        action_res;
} cmd_cmds_t;

#define CMD_NUM_CMDS  0x30

/* --- PROTÓTIPOS --- */

bool cmd_decode(uint8_t *buffer, size_t size, uint8_t *src, uint8_t *dst, cmd_ids_t *id, cmd_cmds_t *decoded_cmd);
bool cmd_encode(uint8_t *buffer, size_t *size, uint8_t *src, uint8_t *dst, cmd_ids_t *id, cmd_cmds_t *encoded_cmd);

/* Encoders Específicos */
bool cmd_encode_version_req(uint8_t dst, uint8_t src, cmd_version_req_t *cmd, uint8_t *buffer, size_t *size);
bool cmd_encode_version_res(uint8_t dst, uint8_t src, cmd_version_res_t *cmd, uint8_t *buffer, size_t *size);
bool cmd_encode_status_req(uint8_t dst, uint8_t src, cmd_get_status_req_t *cmd, uint8_t *buffer, size_t *size);
bool cmd_encode_status_res(uint8_t dst, uint8_t src, cmd_get_status_res_t *cmd, uint8_t *buffer, size_t *size);
bool cmd_encode_config_req(uint8_t dst, uint8_t src, cmd_set_config_req_t *cmd, uint8_t *buffer, size_t *size);
bool cmd_encode_config_res(uint8_t dst, uint8_t src, cmd_set_config_res_t *cmd, uint8_t *buffer, size_t *size);
bool cmd_encode_action_run_req(uint8_t dst, uint8_t src, cmd_action_run_req_t *cmd, uint8_t *buffer, size_t *size);
bool cmd_encode_action_pause_req(uint8_t dst, uint8_t src, cmd_action_pause_req_t *cmd, uint8_t *buffer, size_t *size);
bool cmd_encode_action_abort_req(uint8_t dst, uint8_t src, cmd_action_abort_req_t *cmd, uint8_t *buffer, size_t *size);
bool cmd_encode_action_res(uint8_t dst, uint8_t src, cmd_action_res_t *cmd, uint8_t *buffer, size_t *size);

/* Decoders Específicos */
bool cmd_decode_version_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);
bool cmd_decode_version_res(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);
bool cmd_decode_status_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);
bool cmd_decode_status_res(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);
bool cmd_decode_config_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);
bool cmd_decode_config_res(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);
bool cmd_decode_action_run_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);
bool cmd_decode_action_pause_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);
bool cmd_decode_action_abort_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);
bool cmd_decode_action_purge_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);
bool cmd_decode_action_bolus_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);
bool cmd_decode_action_res(cmd_cmds_t *cmd, uint8_t *buffer, size_t size);

uint16_t crc16_ccitt(const uint8_t *data, size_t length);

#endif