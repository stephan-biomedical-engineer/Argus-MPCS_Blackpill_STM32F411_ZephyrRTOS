#ifndef HUB_H
#define HUB_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include "protocol_defs.h"
#include "cmd.h"

#define HUB_THREAD_STACK_SIZE 4096
#define HUB_THREAD_PRIORITY   1

// --- VARIÁVEIS DO PARSER (MÁQUINA DE ESTADOS) ---
typedef enum
{
    STATE_WAIT_SOF1,
    STATE_WAIT_SOF2,
    STATE_READ_HEADER,
    STATE_READ_PAYLOAD,
    STATE_READ_CRC
} parser_state_t;

typedef enum {
    LOGIC_OK = 0,
    LOGIC_ERR_INVALID_STATE,
    LOGIC_ERR_NOT_ALLOWED,
} logic_result_t;

typedef struct {
    cmd_ids_t cmd_id;
    logic_result_t result;
} logic_ack_t;


extern volatile logic_result_t logic_last_result;
extern volatile command_id_t logic_last_cmd;
extern struct k_sem logic_cmd_sem;

int hub_init(void);

void hub_set_status(const pump_status_t* status);

void hub_thread_entry(void* p1, void* p2, void* p3);

int hub_get_command(pump_cmd_t* cmd);

#endif
