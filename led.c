#include "led.h"

#include "pico/cyw43_arch.h"

// Heartbeat: 100 ms on, 900 ms off — short pulse once per second.
// Request blink: two 80 ms pulses with 80 ms gap, then resume heartbeat.

#define HEARTBEAT_ON_MS   100
#define HEARTBEAT_PERIOD  1000

#define BLINK_PULSE_MS    80
#define BLINK_GAP_MS      80

typedef enum {
    STATE_HEARTBEAT,
    STATE_BLINK1_ON,
    STATE_BLINK1_OFF,
    STATE_BLINK2_ON,
    STATE_BLINK2_OFF,
} led_state_t;

static led_state_t state         = STATE_HEARTBEAT;
static uint32_t    state_until   = 0;
static bool        request_queued = false;

static void set_led(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on ? 1 : 0);
}

void led_init(void) {
    set_led(false);
}

void led_notify_request(void) {
    request_queued = true;
}

#define after(now, deadline) ((int32_t)((now) - (deadline)) >= 0)

void led_tick(uint32_t now_ms) {
    switch (state) {
        case STATE_HEARTBEAT:
            if (!after(now_ms, state_until)) break;

            if (request_queued) {
                request_queued = false;
                set_led(true);
                state       = STATE_BLINK1_ON;
                state_until = now_ms + BLINK_PULSE_MS;
            } else {
                bool currently_on = (now_ms % HEARTBEAT_PERIOD) < HEARTBEAT_ON_MS;
                set_led(currently_on);
            }
            break;

        case STATE_BLINK1_ON:
            if (!after(now_ms, state_until)) break;
            set_led(false);
            state       = STATE_BLINK1_OFF;
            state_until = now_ms + BLINK_GAP_MS;
            break;

        case STATE_BLINK1_OFF:
            if (!after(now_ms, state_until)) break;
            set_led(true);
            state       = STATE_BLINK2_ON;
            state_until = now_ms + BLINK_PULSE_MS;
            break;

        case STATE_BLINK2_ON:
            if (!after(now_ms, state_until)) break;
            set_led(false);
            state       = STATE_BLINK2_OFF;
            state_until = now_ms + BLINK_GAP_MS;
            break;

        case STATE_BLINK2_OFF:
            if (!after(now_ms, state_until)) break;
            state       = STATE_HEARTBEAT;
            state_until = now_ms;  // already elapsed — proceed on next tick
            break;
    }
}
