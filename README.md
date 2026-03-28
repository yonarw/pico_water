# pico_water

Garden irrigation controller for the Raspberry Pi Pico W. Exposes a minimal HTTP REST API compatible with Home Assistant's [REST switch](https://www.home-assistant.io/integrations/switch.rest/) integration.

## Hardware

| Valve     | GPIO pin |
|-----------|----------|
| `rasen_1` | GP2      |
| `rasen_2` | GP3      |
| `beete_1` | GP4      |
| `beete_2` | GP5      |

Pins and WiFi credentials are configured in `config.h` (not tracked by git — see setup below). GP23/24/25/29 are reserved by the CYW43 WiFi chip and must not be used.

Each valve is protected by a 10-minute maximum runtime (`MAX_RUNTIME_SECONDS`) enforced in hardware regardless of API state.

The onboard LED signals activity:
- **Slow single pulse** (100 ms on / 900 ms off) — heartbeat, device is running
- **Double-blink** — triggered on every incoming HTTP request

## API

| Method | Path                     | Body        | Response       |
|--------|--------------------------|-------------|----------------|
| POST   | `/switch/<name>`         | `on` / `off`| `ok`           |
| GET    | `/switch/state/<name>`   | —           | `on` / `off`   |

### Home Assistant config example

```yaml
switch:
  - platform: rest
    name: Rasen 1
    resource: http://<pico-ip>/switch/rasen_1
    state_resource: http://<pico-ip>/switch/state/rasen_1
    body_on: "on"
    body_off: "off"
```

## Build

### Prerequisites

[Nix](https://nixos.org/) with flakes enabled. No other dependencies needed — the flake provides the ARM toolchain, CMake, Ninja, and the pico-sdk.

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
   mkdir -p build && cd build
   cmake -G Ninja ..
   ninja
   ```

   The firmware will be at `build/pico_water.uf2`.

### Flashing

Hold the **BOOTSEL** button on the Pico W, plug it in via USB, then copy the `.uf2` file to the mounted drive:

```sh
cp build/pico_water.uf2 /run/media/$USER/RPI-RP2/
```

The board reboots automatically and connects to WiFi. Serial output (USB) shows connection status.

   ninja
   ```

   The firmware will be at `build/pico_water.uf2`.

### Flashing

Hold the **BOOTSEL** button on the Pico W, plug it in via USB, then copy the `.uf2` file to the mounted drive:

```sh
cp build/pico_water.uf2 /run/media/$USER/RPI-RP2/
```

The board reboots automatically and connects to WiFi. Serial output (USB) shows the assigned IP.
