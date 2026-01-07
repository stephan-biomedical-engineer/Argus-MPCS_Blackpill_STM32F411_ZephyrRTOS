#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <stdint.h>
#include <stdlib.h>
#include <zephyr/kernel.h> 

typedef struct {
    int32_t bolha_mv;
    int32_t oclusao_mv;
    int32_t volume_mv;
    uint32_t timestamp;
} sensor_packet_t;

extern struct k_msgq sensor_data_q;

#endif
