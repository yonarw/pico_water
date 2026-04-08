#pragma once

#include <stddef.h>
#include <stdint.h>

// Call once during startup, after cyw43 is initialised.
void status_init(void);

// Call from the main loop to refresh cached values (e.g. RSSI).
// Must be called outside of any lwIP/CYW43 callback context.
void status_tick(uint32_t now_ms);

// Write a JSON status object into buf (null-terminated).
void status_get_json(char* buf, size_t size);
