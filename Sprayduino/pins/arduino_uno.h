#pragma once
// Pin and platform definitions for Arduino Uno.
// To port this project to a new board: copy this file, rename it
// (e.g. pins_stm32.h), update the values below for the new board, then add
// a branch for it in board_config.h.

#define BOARD_NAME "Arduino Uno"

// ---- Pins ----
#define PIN_VOLTAGE       A4  // Battery voltage sense
#define PIN_RPM           2   // Tachometer input - must support attachInterrupt on this board
#define PIN_TRANSBRAKE    7   // Transbrake switch input
#define PIN_NITROUS_LED   8   // Nitrous-active indicator LED
#define PIN_RELAY1        9   // Nitrous stage 1 relay
#define PIN_RELAY2        10  // Nitrous stage 2 relay (reserved, unused for now)
#define PIN_STATUS_LED    13  // Controller status LED
#define PIN_THROTTLE_TPS  A2  // Throttle position sensor, analog input
#define PIN_THROTTLE_SW   16  // Throttle microswitch (Uno quirk: A2 doubles as digital pin 16)

// ---- ADC ----
// Uno's ADC is 10-bit: analogRead() returns 0-1023. Other boards (many STM32,
// ESP32) default to 12-bit (0-4095) - always read this constant instead of
// assuming 1023 anywhere in the core logic.
#define ADC_MAX 1023

// ---- Persistent storage ----
// Uno has real onboard EEPROM. Boards without native EEPROM (most STM32)
// should leave this undefined and implement flash-based storage in
// Settings.h instead.
#define BOARD_HAS_NATIVE_EEPROM 1
