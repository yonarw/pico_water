#pragma once

#include <stdint.h>

void mqtt_manager_init(void);
void mqtt_manager_tick(uint32_t now_ms);
