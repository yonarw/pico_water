#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "config.h"
#include "gpio_control.h"
#include "rest_api.h"
#include "led.h"
#include "lwip/netif.h"

static bool tick_callback(repeating_timer_t *rt) {
    (void)rt;
    valve_tick();
    return true;
}

int main(void) {
    stdio_init_all();
    sleep_ms(2000);  // allow USB serial to enumerate before first printf

    printf("pico_water: starting\n");

    gpio_control_init();

    if (cyw43_arch_init()) {
        printf("pico_water: WiFi init failed\n");
        return 1;
    }
    led_init();
    cyw43_arch_enable_sta_mode();
    netif_set_hostname(netif_default, HOSTNAME);

    printf("pico_water: connecting to WiFi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("pico_water: WiFi connection failed\n");
        return 1;
    }
    printf("pico_water: WiFi connected\n");

    rest_api_init();

    repeating_timer_t timer;
    add_repeating_timer_ms(-1000, tick_callback, NULL, &timer);

    printf("pico_water: running\n");

    while (true) {
        cyw43_arch_poll();
        uint32_t now = to_ms_since_boot(get_absolute_time());
        led_tick(now);
        rest_api_tick(now);
        sleep_ms(1);
    }
}
