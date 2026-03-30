#include "log_buffer.h"

#include "pico/stdio.h"
#include "pico/stdio/driver.h"
#include "hardware/sync.h"

static char     ring[LOG_BUFFER_SIZE];
static uint16_t head = 0;
static uint16_t used = 0;

static void log_out_chars(const char *buf, int len) {
    uint32_t save = save_and_disable_interrupts();
    for (int i = 0; i < len; i++) {
        ring[head] = buf[i];
        head = (uint16_t)((head + 1) % LOG_BUFFER_SIZE);
        if (used < LOG_BUFFER_SIZE) used++;
    }
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
    // When buffer is not full, data starts at 0. When full, the oldest byte
    // is at head (the next write position, which wraps over the oldest data).
    uint16_t tail = (used < LOG_BUFFER_SIZE) ? 0 : head;
    for (size_t i = 0; i < n; i++) {
        dst[i] = ring[(tail + i) % LOG_BUFFER_SIZE];
    }
    restore_interrupts(save);
    *out_len = n;
}
