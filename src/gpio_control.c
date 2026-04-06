#include "gpio_control.h"
#include "config.h"

#include "hardware/gpio.h"
#include <stdio.h>

static const uint valve_pins[VALVE_COUNT] = { VALVE_PINS };
const char *valve_names[VALVE_COUNT] = { VALVE_NAMES };

static int32_t  valve_status[VALVE_COUNT];
static uint32_t valve_runtime[VALVE_COUNT];

int valve_active_count(void) {
    int n = 0;
    for (int i = 0; i < VALVE_COUNT; i++) {
        if (valve_status[i] != VALVE_STATUS_OFF) n++;
    }
    return n;
}

void gpio_control_init(void) {
    for (int i = 0; i < VALVE_COUNT; i++) {
        gpio_init(valve_pins[i]);
        gpio_set_dir(valve_pins[i], GPIO_OUT);
        gpio_put(valve_pins[i], 0);
        valve_status[i]  = VALVE_STATUS_OFF;
        valve_runtime[i] = 0;
    }
    printf("gpio_control: initialized %d valves\n", VALVE_COUNT);
}

bool valve_turn_on(valve_id_t id) {
    if (valve_status[id] == VALVE_STATUS_OFF && valve_active_count() >= MAX_ACTIVE_VALVES) {
        printf("valve %s: ON denied, max simultaneous valves (%d) reached\n", valve_names[id], MAX_ACTIVE_VALVES);
        return false;
    }
    valve_status[id]  = VALVE_STATUS_PERMANENT;
    valve_runtime[id] = 0;
    gpio_put(valve_pins[id], 1);
    printf("valve %s: ON (permanent)\n", valve_names[id]);
    return true;
}

void valve_turn_off(valve_id_t id) {
    gpio_put(valve_pins[id], 0);
    printf("valve %s: OFF (ran %u s)\n", valve_names[id], valve_runtime[id]);
    valve_status[id]  = VALVE_STATUS_OFF;
    valve_runtime[id] = 0;
}

void valve_turn_off_all(void) {
    for (int i = 0; i < VALVE_COUNT; i++) {
        if (valve_status[i] != VALVE_STATUS_OFF) {
            valve_turn_off((valve_id_t)i);
        }
    }
}

int32_t valve_get_status(valve_id_t id) {
    return valve_status[id];
}

void valve_tick(void) {
    for (int i = 0; i < VALVE_COUNT; i++) {
        if (valve_status[i] == VALVE_STATUS_OFF) continue;

        valve_runtime[i]++;

        if (valve_runtime[i] >= (uint32_t)MAX_VALVE_ACTIVE_SECONDS) {
            printf("valve %s: MAX_RUNTIME exceeded, forcing off\n", valve_names[i]);
            valve_turn_off((valve_id_t)i);
        }
    }
}
