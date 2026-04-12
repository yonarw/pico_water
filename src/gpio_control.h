#pragma once

#include "config.h"
#include <stdbool.h>
#include <stdint.h>

typedef int valve_id_t;

#define VALVE_STATUS_OFF       0
#define VALVE_STATUS_PERMANENT (-1)

extern const char* valve_names[VALVE_COUNT];

void gpio_control_init(void);
bool valve_turn_on(valve_id_t id);
void valve_turn_off(valve_id_t id);
void valve_turn_off_all(void);
int32_t valve_get_status(valve_id_t id);

int valve_active_count(void);

// Call once per second from the main tick
void valve_tick(void);

// Register a callback invoked whenever a valve turns on or off.
// Called from both the main loop and the 1 Hz timer IRQ; keep it short.
typedef void (*valve_state_change_cb_t)(valve_id_t id);
void gpio_control_set_state_change_cb(valve_state_change_cb_t cb);
