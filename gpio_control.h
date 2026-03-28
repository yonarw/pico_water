#pragma once

#include <stdint.h>

typedef enum {
    VALVE_RASEN_1 = 0,
    VALVE_RASEN_2 = 1,
    VALVE_BEETE_1 = 2,
    VALVE_BEETE_2 = 3,
    VALVE_COUNT   = 4
} valve_id_t;

#define VALVE_STATUS_OFF       0
#define VALVE_STATUS_PERMANENT (-1)

extern const char *valve_names[VALVE_COUNT];

void    gpio_control_init(void);
void    valve_turn_on(valve_id_t id);
void    valve_turn_on_timed(valve_id_t id, int32_t seconds);
void    valve_turn_off(valve_id_t id);
void    valve_turn_off_all(void);
int32_t valve_get_status(valve_id_t id);

// Call once per second from the main tick
void valve_tick(void);
