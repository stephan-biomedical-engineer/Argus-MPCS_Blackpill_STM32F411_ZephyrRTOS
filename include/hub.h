#ifndef HUB_H
#define HUB_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include "protocol_defs.h"

#define HUB_THREAD_STACK_SIZE 4096
#define HUB_THREAD_PRIORITY   1

int hub_init(void);

void hub_set_status(const pump_status_t *status);

void hub_thread_entry(void *p1, void *p2, void *p3);

int hub_get_command(pump_cmd_t *cmd) ;


#endif