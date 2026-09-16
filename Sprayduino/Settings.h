#pragma once
#include <EEPROM.h>

// These are defined in the main sketch file (further down, in the DEFAULT
// VALUES section) - declaring them here as extern lets this file reference
// them even though Settings.h is #included before that point in the sketch.
extern int tpsMIN;
extern int tpsMAX;
extern byte ThrottleType;
extern byte ActivePercent;
extern byte ThrottleHysteresis;
extern bool UseTransBrake;
extern unsigned long Delay1Time;
extern bool UseDelay;
extern bool UseNitrousOnBrake;
extern bool SafetyTimeoutFromBrakeRelease;
extern unsigned long SafetyTimeoutDuration;
extern bool UseLowVoltProtect;
extern float LowVoltProtect;
extern int RPMmin;
extern int RPMmax;
extern byte PPR;
extern byte RPMSmoothingFactor;
extern byte RPMHysteresis;

// Bump this whenever the struct layout below changes, so old/mismatched
// EEPROM data gets detected and ignored instead of loaded incorrectly.
#define SETTINGS_VERSION 1
#define SETTINGS_EEPROM_ADDR 0

struct NitrousSettings {
  uint8_t version;
  int tpsMIN;
  int tpsMAX;
  byte ThrottleType;
  byte ActivePercent;
  byte ThrottleHysteresis;
  bool UseTransBrake;
  unsigned long Delay1Time;
  bool UseDelay;
  bool UseNitrousOnBrake;
  bool SafetyTimeoutFromBrakeRelease;
  unsigned long SafetyTimeoutDuration;
  bool UseLowVoltProtect;
  float LowVoltProtect;
  int RPMmin;
  int RPMmax;
  byte PPR;
  byte RPMSmoothingFactor;
  byte RPMHysteresis;
};

#if defined(BOARD_HAS_NATIVE_EEPROM)

void SettingsLoad() {
  NitrousSettings loaded;
  EEPROM.get(SETTINGS_EEPROM_ADDR, loaded);

  if (loaded.version != SETTINGS_VERSION) {
#if defined(DEBUG)
    Serial.println("EEPROM settings missing or outdated - using defaults.");
#endif
    return; // hardcoded defaults from the top of the sketch stay in place
  }

  tpsMIN = loaded.tpsMIN;
  tpsMAX = loaded.tpsMAX;
  ThrottleType = loaded.ThrottleType;
  ActivePercent = loaded.ActivePercent;
  ThrottleHysteresis = loaded.ThrottleHysteresis;
  UseTransBrake = loaded.UseTransBrake;
  Delay1Time = loaded.Delay1Time;
  UseDelay = loaded.UseDelay;
  UseNitrousOnBrake = loaded.UseNitrousOnBrake;
  SafetyTimeoutFromBrakeRelease = loaded.SafetyTimeoutFromBrakeRelease;
  SafetyTimeoutDuration = loaded.SafetyTimeoutDuration;
  UseLowVoltProtect = loaded.UseLowVoltProtect;
  LowVoltProtect = loaded.LowVoltProtect;
  RPMmin = loaded.RPMmin;
  RPMmax = loaded.RPMmax;
  PPR = loaded.PPR;
  RPMSmoothingFactor = loaded.RPMSmoothingFactor;
  RPMHysteresis = loaded.RPMHysteresis;

#if defined(DEBUG)
  Serial.println("Settings loaded from EEPROM.");
#endif
}

void SettingsSave() {
  NitrousSettings toSave;
  toSave.version = SETTINGS_VERSION;
  toSave.tpsMIN = tpsMIN;
  toSave.tpsMAX = tpsMAX;
  toSave.ThrottleType = ThrottleType;
  toSave.ActivePercent = ActivePercent;
  toSave.ThrottleHysteresis = ThrottleHysteresis;
  toSave.UseTransBrake = UseTransBrake;
  toSave.Delay1Time = Delay1Time;
  toSave.UseDelay = UseDelay;
  toSave.UseNitrousOnBrake = UseNitrousOnBrake;
  toSave.SafetyTimeoutFromBrakeRelease = SafetyTimeoutFromBrakeRelease;
  toSave.SafetyTimeoutDuration = SafetyTimeoutDuration;
  toSave.UseLowVoltProtect = UseLowVoltProtect;
  toSave.LowVoltProtect = LowVoltProtect;
  toSave.RPMmin = RPMmin;
  toSave.RPMmax = RPMmax;
  toSave.PPR = PPR;
  toSave.RPMSmoothingFactor = RPMSmoothingFactor;
  toSave.RPMHysteresis = RPMHysteresis;

  EEPROM.put(SETTINGS_EEPROM_ADDR, toSave);

#if defined(DEBUG)
  Serial.println("Settings saved to EEPROM.");
#endif
}

#else
  // Boards without native EEPROM (most STM32) need a flash-based
  // implementation here instead - not needed yet for the Uno/328P work.
#endif
