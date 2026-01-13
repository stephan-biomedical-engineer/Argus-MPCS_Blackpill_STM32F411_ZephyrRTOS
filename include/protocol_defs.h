#ifndef PROTOCOL_DEFS_H
#define PROTOCOL_DEFS_H

#include <stdint.h>

typedef enum 
{
    STATE_POWER_ON,
    STATE_IDLE,         
    STATE_RUNNING,      
    STATE_BOLUS,
    STATE_PURGE,
    STATE_PAUSED,
    STATE_KVO,          
    STATE_END_INFUSION,
    STATE_ALARM_BUBBLE,
    STATE_ALARM_OCCLUSION,
    STATE_ALARM_DOOR,
    STATE_OFF
} pump_state_t;

typedef enum 
{
    CMD_NONE = 0,
    CMD_START,
    CMD_PAUSE,
    CMD_STOP,
    CMD_SET_BOLUS,
    CMD_SET_PURGE,
    CMD_SET_RATE,
    CMD_SET_VOLUME,
    CMD_SET_DIAMETER, 
    CMD_SET_MODE,     
    CMD_CLEAR_ALARM
} command_id_t;

typedef struct 
{
    command_id_t id;
    float param;
} pump_cmd_t;

typedef struct 
{
    pump_state_t current_state;
    float infused_volume;       
    float target_volume;        
    uint32_t configured_flow_rate; 
    uint8_t syringe_diameter;   
    uint8_t infusion_mode;     
    uint32_t pressure_mmhg;     
    uint8_t alarm_code;         
} pump_status_t;

#endif