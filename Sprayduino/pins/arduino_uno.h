#pragma once
// Pin and platform definitions for Arduino Uno.
// To port this project to a new board: copy this file, rename it
// (e.g. pins_stm32.h), update the values below for the new board, then add
// a branch for it in boards.h.
 
#define BOARD_NAME "Arduino Uno"
 
// ---- Pins ----
#define PIN_VOLTAGE       A4  // Battery voltage sense
#define PIN_RPM           2   // Tachometer input - must support attachInterrupt on this board
#define PIN_TRANSBRAKE    7   // Transbrake switch input
#define PIN_NITROUS_LED   8   // Nitrous-active indicator LED
#define PIN_RELAY1        9   // Nitrous stage 1 relay - PWM-capable
#define PIN_RELAY2        10  // Nitrous stage 2 relay (reserved, unused for now) - PWM-capable
#define PIN_RELAY3        11  // Nitrous stage 3 relay (reserved, unused for now) - PWM-capable
#define PIN_STATUS_LED    13  // Controller status LED
#define PIN_THROTTLE_TPS  A2  // Throttle position sensor, analog input
#define PIN_THROTTLE_SW   16  // Throttle microswitch (Uno quirk: A2 doubles as digital pin 16)
#define PIN_FUEL_PRESSURE A0  // Reserved for a future fuel pressure sensor - not yet wired
#define PIN_OIL_PRESSURE  A1  // Reserved for a future oil pressure sensor - not yet wired
 
// ---- ADC ----
// Uno's ADC is 10-bit: analogRead() returns 0-1023. Other boards (many STM32,
// ESP32) default to 12-bit (0-4095) - always read this constant instead of
// assuming 1023 anywhere in the core logic.
#define ADC_MAX 1023
 
// ---- Voltage sensing ----
// ADC_REF_VOLTAGE is the voltage the ADC measures against - used together
// with ADC_MAX to convert a raw analogRead() into the actual voltage present
// at the pin. Uno's ADC reference is the 5V rail; many other boards (most
// STM32, ESP32) use 3.3V instead.
#define ADC_REF_VOLTAGE 5.0
 
// PLACEHOLDER - no voltage divider circuit is built yet. This ratio converts
// the voltage measured AT THE PIN back up to actual battery voltage:
//   batteryVolts = pinVolts * VOLTAGE_DIVIDER_RATIO
// Once you build the divider (R1 from battery+ down to the pin, R2 from the
// pin down to GND):
//   VOLTAGE_DIVIDER_RATIO = (R1 + R2) / R2
// Calibrate it by comparing a multimeter reading of the actual battery
// against the Serial-printed BatteryVoltage, then adjust this constant until
// they match. Also size R1/R2 so the pin voltage can never exceed
// ADC_REF_VOLTAGE even at your highest expected battery voltage (e.g. during
// charging) - this matters even more on 3.3V boards.
#define VOLTAGE_DIVIDER_RATIO 1.0
 
// ---- Persistent storage ----
// Uno has real onboard EEPROM. Boards without native EEPROM (most STM32)
// should leave this undefined and implement flash-based storage in
// Settings.h instead.
#define BOARD_HAS_NATIVE_EEPROM 1
 