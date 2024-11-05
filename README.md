# esp-now Button

A lightweight, energy-efficient portable button.

## Features

- Long-lasting battery life: lasts more than a year on 4 x AAA batteries.
- ESP-Now Protocol: reliable communication without the need for Wi-Fi.
- Supports any ESP32-based controllers (including the ESP32-C3).

## Installation

### Manual Build / OTA

1. Install [PlatformIO](https://platformio.org/install).

**Note:** This repository contains a submodule. Use the `--recursive` option when cloning.

```bash
git clone --recursive https://github.com/DrA1ex/esp_now_button.git
cd esp_now_button

# Specify the platform: esp32-c3 or esp8266
PLATFORM=esp32-c3

# Set the environment: debug, release, or ota
ENV=release

# For OTA: set your ESP's address
ADDRESS=esp_now_button.local

# Additional settings if OTA is enabled
if [ "$ENV" = "ota" ]; then OTA=1; else OTA=0; ADDRESS=""; fi

pio run -t upload -e $PLATFORM-$ENV --upload-port "$ADDRESS"
```

## Usage

- Turn on device, it will discover HUB automatically.
- Configure your HUB to handle signals and perform desired actions.

## Roadmap

- [ ] Release HUB firmware
- [ ] Add support for long pressing
- [ ] Add customization options
- [ ] Expand compatibility
