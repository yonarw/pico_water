# pico_water

Garden irrigation controller for the Raspberry Pi Pico W. Exposes a minimal HTTP REST API compatible with Home Assistant's [REST switch](https://www.home-assistant.io/integrations/switch.rest/) integration.

## Hardware

Valve count, GPIO pins, and names are freely configurable in `config.h`:

```c
#define VALVE_COUNT 4
#define VALVE_PINS  2, 3, 4, 5
#define VALVE_NAMES "rasen_1", "rasen_2", "beete_1", "beete_2"
```

GP23/24/25/29 are reserved by the CYW43 WiFi chip and must not be used. Up to the number of usable GP pins is supported.

`MAX_ACTIVE_VALVES` limits how many valves may be open simultaneously (water pressure / PSU current). A `POST on` that would exceed this limit is **rejected with HTTP 503** — already-active valves are not affected.

WiFi credentials and LAN hostname are also set in `config.h` (not tracked by git — see setup below).

## Safety

Each valve has a maximum runtime (`MAX_VALVE_ACTIVE_SECONDS`, default 10 min). Enforcement runs in a hardware timer IRQ — independent of the network stack, LED logic, and the main loop. A frozen WiFi connection or hung HTTP request cannot prevent a valve from being cut off.

A hardware watchdog resets the device if the main loop stalls for longer than `WATCHDOG_TIMEOUT_MS`. On reboot, the device reconnects to WiFi automatically.

## LED

- **Slow single pulse** (100 ms on / 900 ms off) — heartbeat, device is running
- **Double-blink** — triggered on every incoming HTTP request

## API

### Switch control

| Method | Path                     | Body        | Response       |
|--------|--------------------------|-------------|----------------|
| POST   | `/switch/<name>`         | `on` / `off`| `ok`           |
| GET    | `/switch/state/<name>`   | —           | `on` / `off`   |

### Status

`GET /status` returns a JSON object:

```json
{"version":"1.0.0","uptime_s":3600,"rssi_dbm":-52,"temp_c":28.4,"active_valves":1}
```

| Field           | Description                              |
|-----------------|------------------------------------------|
| `version`       | Firmware version (`git describe`)        |
| `uptime_s`      | Seconds since last boot                  |
| `rssi_dbm`      | WiFi signal strength                     |
| `temp_c`        | RP2040 internal temperature (±5 °C)      |
| `active_valves` | Number of currently open valves          |

### Debug log

`GET /log` returns the last 512 bytes of serial output as plain text. Useful for diagnosing issues without a USB connection.

### Home Assistant config example

```yaml
switch:
  - platform: rest
    name: Rasen 1
    resource: http://<pico-ip>/switch/rasen_1
    state_resource: http://<pico-ip>/switch/state/rasen_1
    body_on: "on"
    body_off: "off"

# One HTTP poll, multiple typed sensors via template
sensor:
  - platform: rest
    resource: http://<pico-ip>/status
    name: Pico Water Status
    value_template: "ok"
    json_attributes:
      - version
      - uptime_s
      - rssi_dbm
      - temp_c
      - active_valves

template:
  - sensor:
      - name: Pico Water Uptime
        state: "{{ state_attr('sensor.pico_water_status', 'uptime_s') }}"
        unit_of_measurement: "s"
        device_class: duration
      - name: Pico Water RSSI
        state: "{{ state_attr('sensor.pico_water_status', 'rssi_dbm') }}"
        unit_of_measurement: "dBm"
      - name: Pico Water Temperature
        state: "{{ state_attr('sensor.pico_water_status', 'temp_c') }}"
        unit_of_measurement: "°C"
        device_class: temperature
```

## Build

### Prerequisites

[Nix](https://nixos.org/) with flakes enabled. No other dependencies needed — the flake provides the ARM toolchain, CMake, Ninja, picotool, and the pico-sdk.

To enable flakes if not already active, add to `~/.config/nix/nix.conf`:
```
experimental-features = nix-command flakes
```

### Setup

1. Copy the example config and fill in your WiFi credentials and GPIO pins:
   ```sh
   cp config.h.example config.h
   # edit config.h
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
