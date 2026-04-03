#pragma once

#include <stdint.h>

typedef enum {
    LED_MODE_BOOTING,     // solid on — waiting for first connection
    LED_MODE_CONNECTING,  // rapid blink — reconnecting after link loss
    LED_MODE_RUNNING,
} led_mode_t;

void led_init(void);
void led_set_mode(led_mode_t mode);

// Trigger a double-blink (call from request handler, only active in RUNNING mode)
void led_notify_request(void);

// Call from the main loop with current time in ms
void led_tick(uint32_t now_ms);
