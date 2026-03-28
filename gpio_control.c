#include "gpio_control.h"
#include "config.h"

#include "hardware/gpio.h"
#include <stdio.h>

static const uint valve_pins[VALVE_COUNT] = {
    PIN_RASEN_1, PIN_RASEN_2, PIN_BEETE_1, PIN_BEETE_2
};

const char *valve_names[VALVE_COUNT] = {
    "rasen_1", "rasen_2", "beete_1", "beete_2"
};

static int32_t  valve_status[VALVE_COUNT];
static uint32_t valve_runtime[VALVE_COUNT];  // seconds elapsed since turned on

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

void valve_turn_on(valve_id_t id) {
    valve_status[id]  = VALVE_STATUS_PERMANENT;
    valve_runtime[id] = 0;
    gpio_put(valve_pins[id], 1);
    printf("valve %s: ON (permanent)\n", valve_names[id]);
}

void valve_turn_on_timed(valve_id_t id, int32_t seconds) {
    valve_status[id]  = seconds;
    valve_runtime[id] = 0;
    gpio_put(valve_pins[id], 1);
    printf("valve %s: ON for %d s\n", valve_names[id], seconds);
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

        if (valve_runtime[i] >= (uint32_t)MAX_RUNTIME_SECONDS) {
            printf("valve %s: MAX_RUNTIME exceeded, forcing off\n", valve_names[i]);
            valve_turn_off((valve_id_t)i);
            continue;
        }

        if (valve_status[i] > 0) {
            valve_status[i]--;
            if (valve_status[i] == 0) {
                valve_turn_off((valve_id_t)i);
            }
        }
    }
}
