#pragma once

#include "config.h"

_Static_assert(WIFI_CONNECT_TIMEOUT_MS < WATCHDOG_TIMEOUT_MS,
    "WIFI_CONNECT_TIMEOUT_MS must be less than WATCHDOG_TIMEOUT_MS");
_Static_assert(WIFI_RECONNECT_INTERVAL_MS > 0,
    "WIFI_RECONNECT_INTERVAL_MS must be positive");
_Static_assert(WATCHDOG_TIMEOUT_MS >= 1000,
    "WATCHDOG_TIMEOUT_MS must be at least 1000 ms");
