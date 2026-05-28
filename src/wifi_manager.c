#include "wifi_manager.h"
#include "config.h"
#include "led.h"

#include "hardware/watchdog.h"
#include "lwip/netif.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>

static uint32_t next_reconnect_ms = 0;
static int s_reconnect_failures = 0;

bool wifi_manager_init(void)
{
    printf("wifi: connecting to %s...\n", WIFI_SSID);
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK,
                                              WIFI_CONNECT_TIMEOUT_MS))
    {
        printf("wifi: connection failed, retrying...\n");
        watchdog_update();
    }
    printf("wifi: connected\n");
    led_set_mode(LED_MODE_RUNNING);
    return true;
}

void wifi_manager_tick(uint32_t now_ms)
{
    bool link_up = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_JOIN;
    bool has_ip = link_up && !ip4_addr_isany(netif_ip4_addr(netif_default));
    if (has_ip)
    {
        s_reconnect_failures = 0;
        return;
    }

    if ((int32_t)(now_ms - next_reconnect_ms) < 0)
        return;

    if (link_up)
        printf("wifi: IP lost, reconnecting...\n");
    else
        printf("wifi: link lost, reconnecting...\n");
    led_set_mode(LED_MODE_CONNECTING);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK,
                                           WIFI_CONNECT_TIMEOUT_MS)
        == 0)
    {
        printf("wifi: reconnected\n");
        led_set_mode(LED_MODE_RUNNING);
    } else
    {
        s_reconnect_failures++;
        if (s_reconnect_failures >= WIFI_MAX_RECONNECT_ATTEMPTS)
        {
            printf("wifi: %d consecutive reconnect failures, rebooting...\n", s_reconnect_failures);
            watchdog_reboot(0, 0, 100);
            while (true)
                tight_loop_contents();
        }
        printf("wifi: reconnection failed (%d/%d), retrying in %d s\n",
               s_reconnect_failures, WIFI_MAX_RECONNECT_ATTEMPTS,
               WIFI_RECONNECT_INTERVAL_MS / 1000);
        next_reconnect_ms = now_ms + WIFI_RECONNECT_INTERVAL_MS;
    }
}
