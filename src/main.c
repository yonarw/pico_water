#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/watchdog.h"
#include "lwip/netif.h"

#include "pico/bootrom.h"

#include "config.h"
#include "config_validate.h"
#include "gpio_control.h"
#include "rest_api.h"
#include "led.h"
#include "wifi_manager.h"
#include "status.h"
#include "log_buffer.h"

extern char __StackLimit;
#define STACK_CANARY 0xDEADC0DEu

static inline void stack_canary_init(void) {
    *(volatile uint32_t *)&__StackLimit = STACK_CANARY;
}

static inline void stack_canary_check(void) {
    if (*(volatile uint32_t *)&__StackLimit != STACK_CANARY) {
        printf("pico_water: stack overflow detected, rebooting...\n");
        watchdog_reboot(0, 0, 100);
        while (true) tight_loop_contents();
    }
}

static void check_usb_bootsel(void) {
    static const char magic[] = "BOOTSEL\n";
    static uint8_t pos = 0;
    int c = getchar_timeout_us(0);
    if (c < 0) return;
    if (c == magic[pos]) {
        pos++;
        if (magic[pos] == '\0')
            reset_usb_boot(0, 0);
    } else {
        pos = (c == magic[0]) ? 1 : 0;
    }
}

static bool tick_callback(repeating_timer_t *rt) {
    (void)rt;
    valve_tick();
    return true;
}

int main(void) {
    stdio_init_all();
    log_buffer_init();
#ifdef ENABLE_USB_DEBUG
    sleep_ms(2000);
#endif
    stack_canary_init();

    if (watchdog_caused_reboot())
        printf("pico_water: rebooted by watchdog\n");

    watchdog_enable(WATCHDOG_TIMEOUT_MS, true);

    printf("pico_water: starting\n");

    gpio_control_init();

    if (cyw43_arch_init()) {
        printf("pico_water: WiFi init failed, rebooting...\n");
        watchdog_reboot(0, 0, 2000);
        while (true) tight_loop_contents();
    }
    led_init();
    cyw43_arch_enable_sta_mode();
    netif_set_hostname(netif_default, HOSTNAME);

    wifi_manager_init();

    status_init();
    rest_api_init();

    repeating_timer_t timer;
    add_repeating_timer_ms(-1000, tick_callback, NULL, &timer);

    printf("pico_water: running\n");

    while (true) {
        watchdog_update();
        stack_canary_check();
        cyw43_arch_poll();
        uint32_t now = to_ms_since_boot(get_absolute_time());
        wifi_manager_tick(now);
        led_tick(now);
        rest_api_tick(now);
        status_tick(now);
        check_usb_bootsel();
        sleep_ms(1);
    }
}
