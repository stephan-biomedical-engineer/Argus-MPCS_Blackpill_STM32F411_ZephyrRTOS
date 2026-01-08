#ifndef HUB_H
#define HUB_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include "protocol_defs.h"

#define HUB_THREAD_STACK_SIZE 4096
#define HUB_THREAD_PRIORITY   1

int hub_init(void);

void hub_set_status(uint8_t state, uint32_t volume, uint32_t flow_set, uint32_t pressure, uint8_t alarm);

void hub_thread_entry(void *p1, void *p2, void *p3);

int hub_get_command(pump_cmd_t *cmd) ;


#endif