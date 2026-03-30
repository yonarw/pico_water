#include "wifi_manager.h"
#include "config.h"

#include "pico/cyw43_arch.h"
#include <stdio.h>

#define CONNECT_TIMEOUT_MS   30000
#define RECONNECT_INTERVAL_MS 5000

static uint32_t next_reconnect_ms = 0;

bool wifi_manager_init(void) {
    printf("wifi: connecting to %s...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK,
                                           CONNECT_TIMEOUT_MS)) {
        printf("wifi: initial connection failed\n");
        return false;
    }
    printf("wifi: connected\n");
    return true;
}

void wifi_manager_tick(uint32_t now_ms) {
    if (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_JOIN)
        return;

    if ((int32_t)(now_ms - next_reconnect_ms) < 0)
        return;

    printf("wifi: link lost, reconnecting...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK,
                                           CONNECT_TIMEOUT_MS) == 0) {
        printf("wifi: reconnected\n");
    } else {
        printf("wifi: reconnection failed, retrying in %d s\n",
               RECONNECT_INTERVAL_MS / 1000);
        next_reconnect_ms = now_ms + RECONNECT_INTERVAL_MS;
    }
}
