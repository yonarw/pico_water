#pragma once

#include <stddef.h>

// Register the log capture driver with pico stdio. Call after stdio_init_all().
void log_buffer_init(void);

// Copy current ring buffer contents (oldest → newest) into dst.
// *out_len is set to the number of bytes written (at most LOG_BUFFER_SIZE).
void log_buffer_snapshot(char *dst, size_t *out_len);
