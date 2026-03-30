#pragma once

#include <stddef.h>

// Call once during startup, after cyw43 is initialised.
void status_init(void);

// Write a JSON status object into buf (null-terminated).
void status_get_json(char *buf, size_t size);
