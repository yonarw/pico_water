#pragma once

#include "config.h"
#include "lwipopts.h"

_Static_assert(WIFI_CONNECT_TIMEOUT_MS < WATCHDOG_TIMEOUT_MS,
    "WIFI_CONNECT_TIMEOUT_MS must be less than WATCHDOG_TIMEOUT_MS");
_Static_assert(WIFI_RECONNECT_INTERVAL_MS > 0,
    "WIFI_RECONNECT_INTERVAL_MS must be positive");
_Static_assert(WATCHDOG_TIMEOUT_MS >= 1000,
    "WATCHDOG_TIMEOUT_MS must be at least 1000 ms");

_Static_assert(VALVE_COUNT > 0,
    "VALVE_COUNT must be at least 1");
_Static_assert(
    sizeof((unsigned[]){ VALVE_PINS }) / sizeof(unsigned) == VALVE_COUNT,
    "VALVE_PINS length must match VALVE_COUNT");
_Static_assert(
    sizeof((const char *[]){ VALVE_NAMES }) / sizeof(const char *) == VALVE_COUNT,
    "VALVE_NAMES length must match VALVE_COUNT");

_Static_assert(MAX_ACTIVE_VALVES > 0,
    "MAX_ACTIVE_VALVES must be at least 1");
_Static_assert(MAX_ACTIVE_VALVES <= VALVE_COUNT,
    "MAX_ACTIVE_VALVES cannot exceed VALVE_COUNT");

_Static_assert(MAX_VALVE_ACTIVE_SECONDS > 0,
    "MAX_VALVE_ACTIVE_SECONDS must be positive");

_Static_assert(LOG_BUFFER_SIZE > 0,
    "LOG_BUFFER_SIZE must be positive");
_Static_assert(LOG_BUFFER_SIZE <= TCP_SND_BUF,
    "LOG_BUFFER_SIZE must not exceed TCP_SND_BUF (see lwipopts.h)");

#define CONFIG_VALIDATED
