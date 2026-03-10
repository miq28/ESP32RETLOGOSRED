#pragma once
#include <driver/twai.h>
#include <stdint.h>

bool can_init(uint32_t speed);
bool can_set_speed(uint32_t speed);
void can_stop();

bool can_send(uint32_t id, bool ext, bool rtr, uint8_t len, uint8_t *data);

void can_rx_task(void *arg);