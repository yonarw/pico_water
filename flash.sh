#!/usr/bin/env bash
set -euo pipefail

UF2="build/pico_water.uf2"
MOUNT="/mnt/pico"
LABEL="RPI-RP2"

if [[ ! -f "$UF2" ]]; then
    echo "error: $UF2 not found — run the build first" >&2
    exit 1
fi

SERIAL=$(ls /dev/ttyACM* 2>/dev/null | head -1)
if [[ -z "$SERIAL" ]]; then
    echo "error: no USB serial device found (is the Pico connected?)" >&2
    exit 1
fi

echo "Sending BOOTSEL trigger to $SERIAL..."
echo "BOOTSEL" > "$SERIAL"

echo "Waiting for device to appear..."
for i in $(seq 1 10); do
    DEVICE=$(blkid -L "$LABEL" 2>/dev/null || true)
    [[ -n "$DEVICE" ]] && break
    sleep 1
done

if [[ -z "$DEVICE" ]]; then
    echo "error: no device with label '$LABEL' found after 10 seconds" >&2
    exit 1
fi

echo "Found $DEVICE, mounting..."
mkdir -p "$MOUNT"
mount "$DEVICE" "$MOUNT"

echo "Copying firmware..."
cp "$UF2" "$MOUNT/"

# The Pico reboots itself as soon as the UF2 is received — umount may race with that
umount "$MOUNT" 2>/dev/null || true
echo "Done — Pico is rebooting with new firmware"
