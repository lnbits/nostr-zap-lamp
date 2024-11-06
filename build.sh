#!/bin/sh
command -v arduino-cli >/dev/null 2>&1 || { echo >&2 "arduino-cli not found. Aborting."; exit 1; }
arduino-cli config --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json init
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli upgrade
# uBitcoin is broken on esp32 3.x.x
arduino-cli core install esp32:esp32@2.0.17
arduino-cli lib install uBitcoin WebSockets ArduinoJson base64 Button WiFiManager Nostr # QRCode ESP32Ping
arduino-cli compile --build-path build --fqbn esp32:esp32:esp32 --build-property "build.partitions=min_spiffs" --build-property "upload.maximum_size=1966080" nostrZapLamp
