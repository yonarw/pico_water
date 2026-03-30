#pragma once

#include <stdbool.h>
#include <stdint.h>

// Call once after cyw43_arch_init() and cyw43_arch_enable_sta_mode().
// Returns true on successful initial connection.
bool wifi_manager_init(void);

// Call every loop iteration — attempts reconnection if the link is down.
void wifi_manager_tick(uint32_t now_ms);
