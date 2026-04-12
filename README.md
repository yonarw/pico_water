# pico_water

Garden irrigation controller for the Raspberry Pi Pico W. Connects to an MQTT broker and integrates with Home Assistant via MQTT auto-discovery.

## Hardware

Valve count, GPIO pins, and names are freely configurable in `config.h`:

```c
#define VALVE_COUNT 4
#define VALVE_PINS  2, 3, 4, 5
#define VALVE_NAMES "rasen_1", "rasen_2", "beete_1", "beete_2"
```

GP23/24/25/29 are reserved by the CYW43 WiFi chip and must not be used. Up to the number of usable GP pins is supported.

`MAX_ACTIVE_VALVES` limits how many valves may be open simultaneously (water pressure / PSU current). A command that would exceed this limit is **rejected silently** — the requesting valve is not turned on, already-active valves are not affected.

WiFi credentials, MQTT broker settings, and LAN hostname are set in `config.h` (not tracked by git — see setup below).

## Safety

Each valve has a maximum runtime (`MAX_VALVE_ACTIVE_SECONDS`, default 10 min). Enforcement runs in a hardware timer IRQ — independent of the network stack, LED logic, and the main loop. A frozen WiFi connection or stalled MQTT publish cannot prevent a valve from being cut off. When a valve is auto-closed, its state is published to MQTT so Home Assistant stays in sync.

A hardware watchdog resets the device if the main loop stalls for longer than `WATCHDOG_TIMEOUT_MS`. On reboot, the device reconnects to WiFi and MQTT automatically.

## LED

- **Slow single pulse** (100 ms on / 900 ms off) — heartbeat, device is running normally
- **Double-blink** — triggered on every outbound MQTT publish (valve state change or status)

## MQTT

The device connects outbound to a Mosquitto (or compatible) broker. No port is opened on the Pico.

### Topic layout

| Direction     | Topic                                | Payload / notes                         |
|---------------|--------------------------------------|-----------------------------------------|
| HA → Pico     | `{prefix}/valve/<name>/set`          | `on` / `off`                            |
| Pico → HA     | `{prefix}/valve/<name>/state`        | `on` / `off`; sent on change and auto-close |
| Pico → HA     | `{prefix}/status`                    | JSON: version, uptime_s, rssi_dbm, temp_c |
| Pico → broker | `{prefix}/availability`              | LWT `offline` / `online`               |
| Pico → HA     | `homeassistant/switch/.../config`    | Discovery payload, once at boot (retained) |
| Pico → HA     | `homeassistant/sensor/.../config`    | Sensor discovery for status fields (retained) |

`{prefix}` = `MQTT_TOPIC_PREFIX` in `config.h` (default `pico`).

Status is published on every valve state change and periodically every `MQTT_STATUS_INTERVAL_MS`.

### Home Assistant auto-discovery

The device publishes retained discovery payloads at boot. Home Assistant picks them up automatically — no manual YAML configuration needed. The following entities are created:

- One **switch** entity per valve (e.g. `switch.rasen_1`)
- **sensor** entities for `version`, `uptime_s`, `rssi_dbm`, `temp_c` (diagnostic category)
- A **binary_sensor** for device availability via the LWT topic

Entity IDs are stable as long as `VALVE_NAMES` and `MQTT_CLIENT_ID` don't change. Renaming a valve in `config.h` creates a new entity and orphans the old one.

### Status JSON

```json
{"version":"1.0.0","uptime_s":3600,"rssi_dbm":-52,"temp_c":28.4,"active_valves":1}
```

### Security

Mosquitto username/password authentication (`MQTT_USERNAME` / `MQTT_PASSWORD`). Credentials travel in plaintext over LAN; TLS is not implemented. Stale retained commands (published before the device last connected) are discarded on receipt.

## Debug log

`GET http://<pico-hostname>/log` returns the in-memory log ring buffer as plain text. Useful for diagnosing issues without a USB connection. This is the only HTTP endpoint; all valve control goes through MQTT.

## Build

### Prerequisites

[Nix](https://nixos.org/) with flakes enabled. No other dependencies needed — the flake provides the ARM toolchain, CMake, Ninja, picotool, and the pico-sdk.

To enable flakes if not already active, add to `~/.config/nix/nix.conf`:
```
experimental-features = nix-command flakes
```

### Setup

1. Copy the example config and fill in your WiFi credentials, GPIO pins, and MQTT broker settings:
   ```sh
   cp config.h.example config.h
   # edit config.h — set WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER_HOST, MQTT_USERNAME, MQTT_PASSWORD
   ```
   `config.h` is listed in `.gitignore` and will never be committed.

2. Enter the dev shell (downloads pico-sdk on first run, ~200 MB, cached afterwards):
   ```sh
   nix develop
   ```

3. Build:
   ```sh
   nix develop --command cmake -B build -G Ninja
   nix develop --command ninja -C build
   ```

   The firmware will be at `build/pico_water.uf2`.

### Flashing

#### Initial flash (no firmware)

Hold the **BOOTSEL** button on the Pico W, plug it in via USB, then copy the `.uf2` file to the mounted drive:

```sh
cp build/pico_water.uf2 /run/media/$USER/RPI-RP2/
```

The board reboots automatically and connects to WiFi.

#### USB update (firmware already running)

`picotool` (available in the dev shell) reboots the running device into BOOTSEL mode over USB, flashes the firmware, and reboots into it — all in one step:

```sh
sudo ./flash.sh
```

To see serial output during startup, enable `ENABLE_USB_DEBUG` in `config.h` before flashing — this adds a 2 s delay so the host can enumerate the USB serial device before the first log lines are printed.
