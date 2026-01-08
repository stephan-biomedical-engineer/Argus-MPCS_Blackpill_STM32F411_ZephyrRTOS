#include "cmd.h"
#include "utl_io.h"
#include "utl_crc16.h"

bool cmd_decode(uint8_t *buffer, size_t size, uint8_t *src, uint8_t *dst, cmd_ids_t *id, cmd_cmds_t *decoded_cmd)
{
    if(size < CMD_HDR_SIZE + CMD_TRAILER_SIZE) return false; 

    uint8_t *pbuf = buffer;
    *dst = utl_io_get8_fl_ap(pbuf); 
    *src = utl_io_get8_fl_ap(pbuf);
    uint8_t raw_id = utl_io_get8_fl_ap(pbuf); 
    *id  = (cmd_ids_t)raw_id;
    uint16_t payload_size = utl_io_get16_fl_ap(pbuf);

    if(*id >= CMD_NUM_CMDS) return false;

    size_t real_packet_len = payload_size + CMD_HDR_SIZE + CMD_TRAILER_SIZE;
    if(size < real_packet_len) return false;
    
    uint16_t crc = utl_io_get16_fl(buffer + real_packet_len - 2);
    uint16_t crc_calc = utl_crc16_data(buffer, real_packet_len - 2, 0xFFFF);
    
    if (crc != crc_calc) return false;

    switch(*id)
    {
        case CMD_VERSION_REQ_ID: 
        case CMD_VERSION_RES_ID:
        case CMD_GET_STATUS_REQ_ID: 
        case CMD_GET_STATUS_RES_ID:
        case CMD_SET_CONFIG_REQ_ID: 
        case CMD_SET_CONFIG_RES_ID:
        case CMD_ACTION_RUN_REQ_ID: 
        case CMD_ACTION_PAUSE_REQ_ID:
        case CMD_ACTION_ABORT_REQ_ID: 
        case CMD_ACTION_PURGE_REQ_ID:
        case CMD_ACTION_BOLUS_REQ_ID: 
        case CMD_ACTION_RES_ID:
            break;
        default: return false;
    }

    /* Tabela de Decoders - ATENÇÃO AQUI: O Mestre precisa saber decodificar RESPOSTAS */
    bool (*decoders[])(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) =
    {
        [CMD_VERSION_REQ_ID]        = cmd_decode_version_req,
        [CMD_VERSION_RES_ID]        = cmd_decode_version_res, 
        [CMD_GET_STATUS_REQ_ID]     = cmd_decode_status_req,
        [CMD_GET_STATUS_RES_ID]     = cmd_decode_status_res,  
        [CMD_SET_CONFIG_REQ_ID]     = cmd_decode_config_req,
        
        /* ESTES ESTAVAM FALTANDO NO MESTRE: */
        [CMD_SET_CONFIG_RES_ID]     = cmd_decode_config_res,  
        [CMD_ACTION_RES_ID]         = cmd_decode_action_res, 
        
        [CMD_ACTION_RUN_REQ_ID]     = cmd_decode_action_run_req,
        [CMD_ACTION_PAUSE_REQ_ID]   = cmd_decode_action_pause_req,
        [CMD_ACTION_ABORT_REQ_ID]   = cmd_decode_action_abort_req,
        [CMD_ACTION_PURGE_REQ_ID]   = cmd_decode_action_purge_req,
        [CMD_ACTION_BOLUS_REQ_ID]   = cmd_decode_action_bolus_req,
    };
     
    if (decoders[(uint8_t)*id] == NULL) return false;

    return decoders[(uint8_t)*id](decoded_cmd, pbuf, payload_size);
}

bool cmd_encode(uint8_t *buffer, size_t *size, uint8_t *src, uint8_t *dst, cmd_ids_t *id, cmd_cmds_t *encoded_cmd)
{
    bool status = false;

    switch(*id)
    {
        /* REQs (Usados pelo Master) */
        case CMD_VERSION_REQ_ID:
            status = cmd_encode_version_req(*dst, *src, &encoded_cmd->version_req, buffer, size);
            break;
        case CMD_GET_STATUS_REQ_ID:
            status = cmd_encode_status_req(*dst, *src, &encoded_cmd->status_req, buffer, size);
            break;
        case CMD_SET_CONFIG_REQ_ID:
            status = cmd_encode_config_req(*dst, *src, &encoded_cmd->config_req, buffer, size);
            break;
        case CMD_ACTION_RUN_REQ_ID:
            status = cmd_encode_action_run_req(*dst, *src, &encoded_cmd->run_req, buffer, size);
            break;
        case CMD_ACTION_PAUSE_REQ_ID:
            status = cmd_encode_action_pause_req(*dst, *src, &encoded_cmd->pause_req, buffer, size);
            break;
        case CMD_ACTION_ABORT_REQ_ID:
            status = cmd_encode_action_abort_req(*dst, *src, &encoded_cmd->abort_req, buffer, size);
            break;
        
        /* RESs (Usados pelo Slave) */
        case CMD_VERSION_RES_ID:
            status = cmd_encode_version_res(*dst, *src, &encoded_cmd->version_res, buffer, size);
            break;
        case CMD_GET_STATUS_RES_ID:
            status = cmd_encode_status_res(*dst, *src, &encoded_cmd->status_res, buffer, size);
            break;
        case CMD_SET_CONFIG_RES_ID:
            status = cmd_encode_config_res(*dst, *src, &encoded_cmd->config_res, buffer, size);
            break;
        case CMD_ACTION_RES_ID:
            status = cmd_encode_action_res(*dst, *src, &encoded_cmd->action_res, buffer, size);
            break;
            
        default:
            status = false;
            break;
    }

    return status;
}


bool cmd_decode_version_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) { return size == 0; }
bool cmd_decode_status_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) { return size == 0; }
bool cmd_decode_action_run_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) { return size == 0; }
bool cmd_decode_action_pause_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) { return size == 0; }
bool cmd_decode_action_abort_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) { return size == 0; }
bool cmd_decode_action_purge_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) { return size == 0; }
bool cmd_decode_action_bolus_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) { return size == 0; }

bool cmd_decode_config_req(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) {
    uint8_t *pbuf = buffer;
    if(size != CMD_SET_CONFIG_REQ_SIZE) return false;
    cmd->config_req.config.volume = utl_io_get32_fl_ap(pbuf);
    cmd->config_req.config.flow_rate = utl_io_get32_fl_ap(pbuf);
    cmd->config_req.config.diameter = utl_io_get8_fl_ap(pbuf);
    cmd->config_req.config.mode = utl_io_get8_fl_ap(pbuf);
    return true;
}

/* Decoders de Resposta (Necessários para o Master ler o Slave) */
bool cmd_decode_version_res(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) {
    uint8_t *pbuf = buffer;
    if(size != CMD_VERSION_RES_SIZE) return false;
    cmd->version_res.major = utl_io_get8_fl_ap(pbuf);
    cmd->version_res.minor = utl_io_get8_fl_ap(pbuf);
    cmd->version_res.patch = utl_io_get8_fl_ap(pbuf);
    return true;
}

bool cmd_decode_status_res(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) {
    uint8_t *pbuf = buffer;
    if(size != CMD_GET_STATUS_RES_SIZE) return false;
    cmd->status_res.status_data.current_state = utl_io_get8_fl_ap(pbuf);
    cmd->status_res.status_data.volume = utl_io_get32_fl_ap(pbuf);
    cmd->status_res.status_data.flow_rate_set = utl_io_get32_fl_ap(pbuf);
    cmd->status_res.status_data.pressure = utl_io_get32_fl_ap(pbuf);
    cmd->status_res.status_data.alarm_active = utl_io_get8_fl_ap(pbuf);
    return true;
}

bool cmd_decode_config_res(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) {
    uint8_t *pbuf = buffer;
    if(size != CMD_SET_CONFIG_RES_SIZE) return false;
    cmd->config_res.status = (cmd_status_t)utl_io_get8_fl_ap(pbuf);
    return true;
}

bool cmd_decode_action_res(cmd_cmds_t *cmd, uint8_t *buffer, size_t size) {
    uint8_t *pbuf = buffer;
    if(size != CMD_ACTION_RES_SIZE) return false;
    cmd->action_res.cmd_req_id = (cmd_ids_t)utl_io_get8_fl_ap(pbuf);
    cmd->action_res.status = (cmd_status_t)utl_io_get8_fl_ap(pbuf);
    return true;
}

static bool cmd_encode_header_only(uint8_t dst, uint8_t src, cmd_ids_t id, uint8_t *buffer, size_t *size) {
    uint8_t *pbuf = buffer;
    utl_io_put8_tl_ap(dst, pbuf);
    utl_io_put8_tl_ap(src, pbuf);
    utl_io_put8_tl_ap(id, pbuf);
    utl_io_put16_tl_ap(0, pbuf); // Payload Size 0
    utl_io_put16_tl_ap(utl_crc16_data(buffer, CMD_HDR_SIZE, 0xFFFF), pbuf);
    *size = (pbuf - buffer);
    return true;
}

bool cmd_encode_version_req(uint8_t dst, uint8_t src, cmd_version_req_t *cmd, uint8_t *buffer, size_t *size) {
    return cmd_encode_header_only(dst, src, CMD_VERSION_REQ_ID, buffer, size);
}
bool cmd_encode_status_req(uint8_t dst, uint8_t src, cmd_get_status_req_t *cmd, uint8_t *buffer, size_t *size) {
    return cmd_encode_header_only(dst, src, CMD_GET_STATUS_REQ_ID, buffer, size);
}
bool cmd_encode_action_run_req(uint8_t dst, uint8_t src, cmd_action_run_req_t *cmd, uint8_t *buffer, size_t *size) {
    return cmd_encode_header_only(dst, src, CMD_ACTION_RUN_REQ_ID, buffer, size);
}
bool cmd_encode_action_pause_req(uint8_t dst, uint8_t src, cmd_action_pause_req_t *cmd, uint8_t *buffer, size_t *size) {
    return cmd_encode_header_only(dst, src, CMD_ACTION_PAUSE_REQ_ID, buffer, size);
}
bool cmd_encode_action_abort_req(uint8_t dst, uint8_t src, cmd_action_abort_req_t *cmd, uint8_t *buffer, size_t *size) {
    return cmd_encode_header_only(dst, src, CMD_ACTION_ABORT_REQ_ID, buffer, size);
}

bool cmd_encode_config_req(uint8_t dst, uint8_t src, cmd_set_config_req_t *cmd, uint8_t *buffer, size_t *size) {
    uint8_t *pbuf = buffer;
    utl_io_put8_tl_ap(dst, pbuf);
    utl_io_put8_tl_ap(src, pbuf);
    utl_io_put8_tl_ap(CMD_SET_CONFIG_REQ_ID, pbuf);
    utl_io_put16_tl_ap(CMD_SET_CONFIG_REQ_SIZE, pbuf);
    
    utl_io_put32_tl_ap(cmd->config.volume, pbuf);
    utl_io_put32_tl_ap(cmd->config.flow_rate, pbuf);
    utl_io_put8_tl_ap(cmd->config.diameter, pbuf);
    utl_io_put8_tl_ap(cmd->config.mode, pbuf);
    
    utl_io_put16_tl_ap(utl_crc16_data(buffer, CMD_HDR_SIZE + CMD_SET_CONFIG_REQ_SIZE, 0xFFFF), pbuf);
    *size = (pbuf - buffer);
    return true;
}

/* Encoders de Resposta (Já existiam, mantidos) */
bool cmd_encode_version_res(uint8_t dst, uint8_t src, cmd_version_res_t *cmd, uint8_t *buffer, size_t *size)
{
    uint8_t *pbuf = buffer;
    utl_io_put8_tl_ap(dst,pbuf);
    utl_io_put8_tl_ap(src,pbuf);
    utl_io_put8_tl_ap(CMD_VERSION_RES_ID,pbuf);
    utl_io_put16_tl_ap(CMD_VERSION_RES_SIZE,pbuf);
    utl_io_put8_tl_ap(cmd->major,pbuf);
    utl_io_put8_tl_ap(cmd->minor,pbuf);
    utl_io_put8_tl_ap(cmd->patch,pbuf);
    utl_io_put16_tl_ap(utl_crc16_data(buffer,CMD_HDR_SIZE + CMD_VERSION_RES_SIZE,0xFFFF),pbuf);
    *size = (pbuf - buffer);
    return true;
}

bool cmd_encode_status_res(uint8_t dst, uint8_t src, cmd_get_status_res_t *cmd, uint8_t *buffer, size_t *size)
{
    uint8_t *pbuf = buffer;
    utl_io_put8_tl_ap(dst,pbuf);
    utl_io_put8_tl_ap(src,pbuf);
    utl_io_put8_tl_ap(CMD_GET_STATUS_RES_ID,pbuf);
    utl_io_put16_tl_ap(CMD_GET_STATUS_RES_SIZE,pbuf);
    utl_io_put8_tl_ap(cmd->status_data.current_state,pbuf);
    utl_io_put32_tl_ap(cmd->status_data.volume,pbuf);
    utl_io_put32_tl_ap(cmd->status_data.flow_rate_set,pbuf);
    utl_io_put32_tl_ap(cmd->status_data.pressure,pbuf);
    utl_io_put8_tl_ap(cmd->status_data.alarm_active,pbuf);
    utl_io_put16_tl_ap(utl_crc16_data(buffer,CMD_HDR_SIZE + CMD_GET_STATUS_RES_SIZE,0xFFFF),pbuf);
    *size = (pbuf - buffer);
    return true;
}

bool cmd_encode_config_res(uint8_t dst, uint8_t src, cmd_set_config_res_t *cmd, uint8_t *buffer, size_t *size)
{
    uint8_t *pbuf = buffer;
    utl_io_put8_tl_ap(dst,pbuf);
    utl_io_put8_tl_ap(src,pbuf);
    utl_io_put8_tl_ap(CMD_SET_CONFIG_RES_ID,pbuf);
    utl_io_put16_tl_ap(CMD_SET_CONFIG_RES_SIZE,pbuf);
    utl_io_put8_tl_ap(cmd->status,pbuf);
    utl_io_put16_tl_ap(utl_crc16_data(buffer,CMD_HDR_SIZE + CMD_SET_CONFIG_RES_SIZE,0xFFFF),pbuf);
    *size = (pbuf - buffer);
    return true;
}

bool cmd_encode_action_res(uint8_t dst, uint8_t src, cmd_action_res_t *cmd, uint8_t *buffer, size_t *size)
{
    uint8_t *pbuf = buffer;
    utl_io_put8_tl_ap(dst,pbuf);
    utl_io_put8_tl_ap(src,pbuf);
    utl_io_put8_tl_ap(CMD_ACTION_RES_ID,pbuf);
    utl_io_put16_tl_ap(CMD_ACTION_RES_SIZE,pbuf);
    utl_io_put8_tl_ap(cmd->cmd_req_id,pbuf);
    utl_io_put8_tl_ap(cmd->status,pbuf);
    utl_io_put16_tl_ap(utl_crc16_data(buffer,CMD_HDR_SIZE + CMD_ACTION_RES_SIZE,0xFFFF),pbuf);
    *size = (pbuf - buffer);
    return true;
}