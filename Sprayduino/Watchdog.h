#pragma once

// Thin wrapper around this board's hardware watchdog. The core logic only
// ever calls WatchdogEnable()/WatchdogPet()/WatchdogDisable() - it never
// touches the platform's watchdog API directly - so porting to a board with
// a different watchdog peripheral only ever means editing this one file.
//
// The watchdog forces a full hardware reset if WatchdogPet() isn't called
// often enough - protecting against the (rare, but possible) case of the
// firmware hanging with a relay stuck active. A reset re-runs setup(), which
// drives every relay back to its safe OFF state.

#if defined(__AVR__)
  #include <avr/wdt.h>

  // Time allowed between WatchdogPet() calls before a reset is forced.
  // loop() has no blocking delays and normally completes in well under a
  // millisecond, so 1 second gives huge margin without being slow to
  // recover from a genuine hang.
  #define WATCHDOG_TIMEOUT WDTO_1S

  void WatchdogDisable() {
    wdt_disable();
  }

  void WatchdogEnable() {
    wdt_enable(WATCHDOG_TIMEOUT);
  }

  void WatchdogPet() {
    wdt_reset();
  }

#else
  #warning "No watchdog implementation for this board yet - WatchdogEnable()/WatchdogPet()/WatchdogDisable() are no-ops. Add one here (e.g. STM32's IWatchdog library) before relying on this board for real."

  void WatchdogDisable() {}
  void WatchdogEnable() {}
  void WatchdogPet() {}