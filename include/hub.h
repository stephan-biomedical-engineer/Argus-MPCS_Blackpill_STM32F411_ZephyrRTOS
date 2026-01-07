#ifndef HUB_H
#define HUB_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include "cmd.h"

#define HUB_THREAD_STACK_SIZE 1024
#define HUB_THREAD_PRIORITY   5

int hub_init(void);

void hub_set_status(uint8_t state, uint32_t volume, uint32_t pressure, uint8_t alarm);

void hub_thread_entry(void *p1, void *p2, void *p3);

int hub_get_command(cmd_ids_t *cmd);


#endif