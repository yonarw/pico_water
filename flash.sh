#!/usr/bin/env bash
set -euo pipefail

UF2="build/pico_water.uf2"

if [[ ! -f "$UF2" ]]; then
    echo "error: $UF2 not found — run the build first" >&2
    exit 1
fi

echo "Rebooting device into BOOTSEL mode..."
sudo picotool reboot -u -f

echo "Waiting for device to enumerate..."
for i in $(seq 1 10); do
    sudo picotool info &>/dev/null && break
    sleep 1
done

echo "Loading firmware..."
sudo picotool load -f "$UF2"

echo "Rebooting into new firmware..."
sudo picotool reboot
echo "Done"
