#pragma once
// Picks the pin/platform header for whichever board this is compiled for.
// The Arduino IDE / PlatformIO define one of the ARDUINO_* macros below
// automatically based on the selected board - you don't set these yourself.
//
// To port to a new board:
//   1. Copy pins_uno.h to a new file (e.g. pins_stm32.h) and edit its pins,
//      ADC_MAX, and storage #defines for the new hardware.
//   2. Add a new #elif branch below that includes it.

#if defined(ARDUINO_AVR_UNO)
  #include "pins/arduino_uno.h"
#elif defined(ARDUINO_AVR_MEGA2560)
  #error "No pins_mega.h yet - copy pins_uno.h, adjust it for the Mega, and include it here"
#elif defined(ARDUINO_ARCH_STM32)
  #error "No pins_stm32.h yet - copy pins_uno.h, adjust it for STM32 (12-bit ADC, no native EEPROM), and include it here"
#else
  #error "Unrecognized board - create a pinsXXX.h for it and add a branch here"
#endif
