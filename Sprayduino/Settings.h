#pragma once
#include "boards.h"

// Thin wrapper around whatever persistent storage this board has. The core
// logic only ever calls SettingsLoad()/SettingsSave() - it never touches
// EEPROM directly - so swapping AVR's native EEPROM for a flash-emulation
// library on a different board only ever means editing this one file.

#if defined(BOARD_HAS_NATIVE_EEPROM)
  #include <EEPROM.h>
  //#include <EEPROMAnything.h>
#endif

// Still stubbed out - fill these in when the settings menu (buttons/LCD) is
// built. Until then the sketch runs entirely on the hardcoded defaults in
// the main .ino, same as before.

void SettingsLoad() {
#if defined(BOARD_HAS_NATIVE_EEPROM)
  // e.g. EEPROM_readAnything(0, settingsStruct);
#else
  // Non-AVR boards: read from flash-based emulated storage here instead.
#endif
}

void SettingsSave() {
#if defined(BOARD_HAS_NATIVE_EEPROM)
  // e.g. EEPROM_writeAnything(0, settingsStruct);
#else
  // Non-AVR boards: write to flash-based emulated storage here instead.
#endif
}
