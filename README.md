# ESP32 Bicycle Computer

A custom, fully functional bicycle head unit built around the ESP32 microcontroller. Written in C++ (Arduino IDE), designed as a low-cost, open-source alternative to commercial head units like Garmin or Wahoo.

## Features

* **Navigation & Map:** Live rendering of a planned route on a TFT screen using a parsed `route.gpx` file.
* **Track Recording:** Generates custom GPX tracks saved to the SD card, logging GPS coordinates, elevation, and cadence data.
* **BLE Integration:** Wireless data fetching from external Bluetooth Low Energy cycling sensors (e.g., Cycplus cadence sensor).
* **Built-in Wi-Fi Server:** Allows downloading rides (GPX files) and uploading new routes via a local web browser interface to a phone or PC, eliminating the need to physically remove the SD card.
* **Precision Timer:** Custom Delta Time algorithm for tracking moving and total time, immune to UI rendering delays or SD card write blocking.
* **Stats Memory:** Persistent storage of total mileage (ODO) on the SD card.

## Hardware

* Board: **ESP32**
* Display: **TFT 240x320** with **XPT2046** touch panel (SPI)
* Location: **GPS Module** (e.g., NEO-6M / M8N) via UART (RX/TX)
* Storage: **MicroSD** card reader module (SPI)

## Dependencies / Libraries

To compile this code in Arduino IDE, you need the following libraries installed:
* `TFT_eSPI` (graphics rendering, requires proper `User_Setup.h` configuration for your specific screen)
* `XPT2046_Touchscreen` (touch input handling)
* `TinyGPSPlus` (NMEA data parsing)
* Built-in ESP32 libraries: `WiFi`, `WebServer`, `SD`, `BLEDevice`

## ⚠️ Important: Setup & Compilation

For security reasons, the Wi-Fi credentials file is excluded from this repository. To compile the code, you must manually create a `secrets.h` file in the root project directory and paste the following template:

```cpp
#pragma once

// Home Wi-Fi Network
#define WIFI_SSID_HOME "Your_Home_Network_Name"
#define WIFI_PASS_HOME "Your_Home_Password"

// Mobile Hotspot (for on-the-go syncing)
#define WIFI_SSID_MOBILE "Your_Hotspot_Name"
#define WIFI_PASS_MOBILE "Your_Hotspot_Password"