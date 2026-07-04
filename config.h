/**
 * @file config.h
 * @brief Hardware configuration, pinouts, and constants.
 */
#pragma once

// --- GPS Pins (CN1 Socket) ---
#define RX_PIN 22
#define TX_PIN 27

// --- SD Card Pins ---
#define SD_CS 5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18

// --- Touchscreen Pins ---
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// --- Application Limits ---
#define MAX_ROUTE_POINTS 1500