#pragma once
#include "Boards.h"
 
// Thin wrapper around the fuel/oil pressure sensors' conversion math. Pin
// numbers still come from boards.h like everything else, but the actual
// transfer function (how a raw analogRead() maps to PSI) depends on which
// sensor model you use, not which board - that's a different axis of
// variation than board portability, so it lives here instead of in
// boards.h or the main sketch.
//
// Not implemented yet - fill in ReadFuelPressurePSI()/ReadOilPressurePSI()
// once you've picked actual sensors. Until then both report 0, which the
// safety cutoffs in the main sketch treat as "below minimum" - fails safe
// (blocks nitrous) rather than silently passing if a cutoff gets enabled
// before this is wired up for real.
 
float ReadFuelPressurePSI() {
  // Example for a typical automotive 0.5-4.5V linear pressure transducer
  // rated 0-100psi (check your sensor's actual datasheet numbers before
  // using this - transfer functions vary by sensor):
  //
  //   float pinVolts = analogRead(PIN_FUEL_PRESSURE) * (ADC_REF_VOLTAGE / (float)ADC_MAX);
  //   float psi = (pinVolts - 0.5) * (100.0 / (4.5 - 0.5));
  //   return psi;
  return 0;
}
 
float ReadOilPressurePSI() {
  // Same idea as ReadFuelPressurePSI() - fill in once a sensor is chosen.
  // Oil pressure sensors commonly use a different voltage range/PSI rating
  // than fuel pressure sensors, so don't assume they share one formula.
  return 0;
}
 
float ReadFuelPressureSwitchPSI() {
  return (digitalRead(PIN_FUEL_PRESSURE) == LOW) ? 999.0 : 0.0;
}
 
float ReadOilPressureSwitchPSI() {
  return (digitalRead(PIN_OIL_PRESSURE) == LOW) ? 999.0 : 0.0;
}