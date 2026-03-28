#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef int valve_id_t;

#define VALVE_STATUS_OFF       0
#define VALVE_STATUS_PERMANENT (-1)

extern const char *valve_names[VALVE_COUNT];

void    gpio_control_init(void);
bool    valve_turn_on(valve_id_t id);
bool    valve_turn_on_timed(valve_id_t id, int32_t seconds);
void    valve_turn_off(valve_id_t id);
void    valve_turn_off_all(void);
int32_t valve_get_status(valve_id_t id);

// Call once per second from the main tick
void valve_tick(void);
