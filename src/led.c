#include "led.h"

#include "pico/cyw43_arch.h"

// Connecting: rapid flash at 4 Hz — 125 ms on, 125 ms off.
// Running heartbeat: short 100 ms pulse every 2 s.
// Request blink: two 80 ms pulses with 80 ms gap, then resume heartbeat.

#define CONNECTING_ON_MS     125
#define CONNECTING_PERIOD_MS 250

#define HEARTBEAT_ON_MS      100
#define HEARTBEAT_PERIOD_MS  2000

#define BLINK_PULSE_MS       80
#define BLINK_GAP_MS         80

typedef enum {
    STATE_HEARTBEAT,
    STATE_BLINK1_ON,
    STATE_BLINK1_OFF,
    STATE_BLINK2_ON,
    STATE_BLINK2_OFF,
} led_state_t;

static led_mode_t mode = LED_MODE_BOOTING;
static led_state_t state = STATE_HEARTBEAT;
static uint32_t state_until = 0;
static bool request_queued = false;

static void set_led(bool on) { cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on ? 1 : 0); }

void led_init(void) { set_led(false); }

void led_set_mode(led_mode_t new_mode)
{
    mode = new_mode;
    state = STATE_HEARTBEAT;
    state_until = 0;
    request_queued = false;
}

void led_notify_activity(void)
{
    if (mode == LED_MODE_RUNNING)
        request_queued = true;
}

#define after(now, deadline) ((int32_t)((now) - (deadline)) >= 0)

void led_tick(uint32_t now_ms)
{
    switch (state)
    {
    case STATE_HEARTBEAT:
        if (!after(now_ms, state_until))
            break;

        if (mode == LED_MODE_RUNNING && request_queued)
        {
            request_queued = false;
            set_led(true);
            state = STATE_BLINK1_ON;
            state_until = now_ms + BLINK_PULSE_MS;
        } else if (mode == LED_MODE_BOOTING)
        {
            set_led(true);
        } else if (mode == LED_MODE_CONNECTING)
        {
            bool on = (now_ms % CONNECTING_PERIOD_MS) < CONNECTING_ON_MS;
            set_led(on);
        } else
        {
            bool on = (now_ms % HEARTBEAT_PERIOD_MS) < HEARTBEAT_ON_MS;
            set_led(on);
        }
        break;

    case STATE_BLINK1_ON:
        if (!after(now_ms, state_until))
            break;
        set_led(false);
        state = STATE_BLINK1_OFF;
        state_until = now_ms + BLINK_GAP_MS;
        break;

    case STATE_BLINK1_OFF:
        if (!after(now_ms, state_until))
            break;
        set_led(true);
        state = STATE_BLINK2_ON;
        state_until = now_ms + BLINK_PULSE_MS;
        break;

    case STATE_BLINK2_ON:
        if (!after(now_ms, state_until))
            break;
        set_led(false);
        state = STATE_BLINK2_OFF;
        state_until = now_ms + BLINK_GAP_MS;
        break;

    case STATE_BLINK2_OFF:
        if (!after(now_ms, state_until))
            break;
        state = STATE_HEARTBEAT;
        state_until = now_ms;
        break;
    }
}
