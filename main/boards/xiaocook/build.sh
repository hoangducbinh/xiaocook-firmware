#!/bin/sh
# Repeatable XiaoCook community preset; ESP-IDF must already be exported.
set -eu
if [ -z "${IDF_PATH:-}" ]; then
    echo "Activate ESP-IDF first: source /Users/macos/esp/esp-idf/export.sh" >&2
    exit 1
fi
cd "$(dirname "$0")/../../.."
exec python3 scripts/build.py xiaocook --name xiaocook-st7789-240x240 --language vi-VN --wake-word disabled "$@"
