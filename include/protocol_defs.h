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
    float pressure_val;
    uint8_t battery_level;
} pump_status_t;

#endif
