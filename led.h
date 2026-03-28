#pragma once

#include <stdint.h>

void led_init(void);

// Trigger a double-blink (call from request handler)
void led_notify_request(void);

// Call from the main loop with current time in ms
void led_tick(uint32_t now_ms);
