#pragma once
#include <Arduino.h>

// Pins ESP32
#define PIN_ENABLE 26
#define PIN_DATA   27
#define PIN_CLOCK  14
#define PIN_LATCH  12
#define PIN_BUTTON 16

// Display
#define COLS 16
#define ROWS 16
constexpr uint8_t  MAX_BRIGHTNESS = 255;
constexpr uint16_t TOTAL_PIXELS   = ROWS * COLS;

// NTP / fuseau horaire (France = CET/CEST)
#define NTP_SERVER        "pool.ntp.org"
#define TZ_INFO           "CET-1CEST,M3.5.0,M10.5.0/3"
#define WIFI_MANAGER_SSID "IKEA-Clock"

enum SYSTEM_STATUS { NONE, WSBINARY, UPDATE, LOADING };
extern volatile SYSTEM_STATUS currentStatus;
