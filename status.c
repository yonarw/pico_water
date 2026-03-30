#include "status.h"
#include "gpio_control.h"

#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "pico/time.h"
#include <stdio.h>

#ifndef FW_VERSION
#define FW_VERSION "unknown"
#endif

void status_init(void) {
    adc_init();
    adc_set_temp_sensor_enabled(true);
}

static float read_temp_c(void) {
    adc_select_input(4);
    uint16_t raw = adc_read();
    float voltage = raw * (3.3f / 4096.0f);
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

void status_get_json(char *buf, size_t size) {
    int32_t rssi = 0;
    cyw43_wifi_get_rssi(&cyw43_state, &rssi);

    float temp      = read_temp_c();
    int   temp_i    = (int)temp;
    int   temp_frac = (int)((temp - (float)temp_i) * 10.0f);
    if (temp_frac < 0) temp_frac = -temp_frac;

    uint32_t uptime_s = to_ms_since_boot(get_absolute_time()) / 1000;

    snprintf(buf, size,
        "{\"version\":\"%s\",\"uptime_s\":%u,\"rssi_dbm\":%d,\"temp_c\":%d.%d,\"active_valves\":%d}",
        FW_VERSION,
        (unsigned int)uptime_s,
        (int)rssi,
        temp_i, temp_frac,
        valve_active_count());
}
