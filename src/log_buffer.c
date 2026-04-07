#include "log_buffer.h"

#include <stdio.h>
#include <string.h>
#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include "pico/time.h"
#include "hardware/sync.h"

static char     ring[LOG_BUFFER_SIZE];
static uint16_t head = 0;
static uint16_t used = 0;

static void write_ring(const char *buf, int len) {
    int first = LOG_BUFFER_SIZE - head;
    if (first >= len) {
        memcpy(ring + head, buf, len);
    } else {
        memcpy(ring + head, buf, first);
        memcpy(ring, buf + first, len - first);
    }
    head = (uint16_t)((head + len) % LOG_BUFFER_SIZE);
    if (used + len >= LOG_BUFFER_SIZE)
        used = LOG_BUFFER_SIZE;
    else
        used = (uint16_t)(used + len);
}

static void log_out_chars(const char *buf, int len) {
    char prefix[20];
    uint32_t ms = to_ms_since_boot(get_absolute_time());
    int plen = snprintf(prefix, sizeof(prefix), "%5lu.%03lu | ", ms / 1000, ms % 1000);

    uint32_t save = save_and_disable_interrupts();
    write_ring(prefix, plen);
    write_ring(buf, len);
    restore_interrupts(save);
}

static stdio_driver_t log_driver = {
    .out_chars = log_out_chars,
};

void log_buffer_init(void) {
    stdio_set_driver_enabled(&log_driver, true);
}

void log_buffer_snapshot(char *dst, size_t *out_len) {
    uint32_t save = save_and_disable_interrupts();
    size_t n = used;
    uint16_t tail = (used < LOG_BUFFER_SIZE) ? 0 : head;
    size_t first = LOG_BUFFER_SIZE - tail;
    if (first >= n) {
        memcpy(dst, ring + tail, n);
    } else {
        memcpy(dst, ring + tail, first);
        memcpy(dst + first, ring, n - first);
    }
    restore_interrupts(save);
    *out_len = n;
}
