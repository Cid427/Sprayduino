/*
  Sprayduino Nitrous Controller - portable version
  Turns on or off nitrous control relay based on TPS position.

  Reads voltage from Throttle Position Sensor on analog pin 2, or
  a microswitch on Digital pin 16(A2 used as digital).
  Read Trans Brake input on digital pin 7 to inhibit nitrous activation.
  Turns on or off nitrous activation relays on digital pin 9 and 10
  Transbrake input on digital pin 7 to inhibit activation while transbrake is engaged.
  Nitrous activation delay timer- started by release of the transbrake.

  Future plans:
    Add an arming input, remove the nitrous active led and use that pin. Add that led in hardware instead
    Nitrous relay safety timeout - shut off after n# seconds
    Menu system to make settings user configurable - will need an LCD and buttons,rotary,etc.
      (LoadConfig/SaveConfig/CheckButtons are stubbed out below for this)
    Battery reference voltage -  low voltage shutoff. -done
    Low voltage warning LED
    RPM Smoothing -done
    Second nitrous stage on NitrousRelay2 (currently wired but unused)
    Port to STM32 - add pins_stm32.h (see board_config.h)
    Fuel pressure safety
    Oil pressure safety
    Master Arm/kill switch
    Watchdog timer

  Original by Troy Bernakevitch, Nov 21 2015
  Patched: bug fixes to RPM handling, transbrake edge case, throttle edge
  case, dead config-detection code, and transbrake debounce (see "PATCH:"
  comments). Then split into a board-portable layout: pin numbers and ADC
  resolution now live in board_config.h / pins_uno.h instead of this file,
  and EEPROM specifics are isolated in Settings.h. Porting to a new board
  should never require editing this file - see board_config.h.
*/

#include "Boards.h"       // pin numbers, ADC_MAX, and storage capability for this board
#include "Settings.h"     // SettingsLoad()/SettingsSave() - board-specific storage lives here
#include "Watchdog.h"     // WatchdogEnable()/WatchdogPet()/WatchdogDisable() - board-specific watchdog lives here
#include "Sensors.h"      // ReadFuelPressurePSI()/ReadOilPressurePSI() - sensor-specific conversion math lives here

#define VERSION " Sprayduino V0.6"
#define DEBUG (1)


//********** DEFAULT VALUES **********//
// After Setup the user values stored in the EEPROM will override these defaults.

int tpsMIN = 1; // TPS at closed throttle - future to be programmable
int tpsMAX = ADC_MAX; // TPS at WOT - future to be programmable. Was hardcoded 1023; now follows this board's ADC resolution.
int ThrottleType = 0; // Type 0 for a TPS or 1 for microswtich.
byte ActivePercent = 95; // percentage of throttle opening at which to activate nitrous
byte ThrottleHysteresis = 5; // percentage points below ActivePercent before nitrous cuts out.
bool UseTransBrake = true; // whether a transbrake input is actually wired and used in this setup.
unsigned long Delay1Time = 1000; // The amount of time to delay nitrous activation on release of transbrake
bool UseDelay = true; // whether to require Delay1Time to elapse after brake release before nitrous is in use.
bool UseNitrousOnBrake = false; // allow nitrous to be active when transbrake is on or not
bool SafetyTimeoutFromBrakeRelease = true; // true: safety timer starts counting when the transbrake releases.
unsigned long SafetyTimeoutDuration = 10000; // milliseconds nitrous may stay continuously active before being activated
bool UseLowVoltProtect = true; // user-configurable: whether the low-voltage cutoff is enforced at all
float LowVoltProtect = 11.0; // battery voltage below which nitrous is inhibited, only enforced if UseLowVoltProtect is true
unsigned long LowVoltTimeoutMS = 500; // how long voltage may stay below LowVoltProtect before nitrous cut
int RPMmin = 350; // Minimum RPM for nitrous activation divided by 10 RPM only needs to read in multiples of 10
int RPMmax = 750; // Maximum RPM for nitrous activation divided by 10
byte PPR = 4; // Pulses Per Revolution, typical distibutor applications 4 for 8 cyl, 3 for 6 cyl, and 2 for 4 cyl.
byte RPMSmoothingFactor = 25; // 0-100: how much each new RPM reading counts vs the running average.
                               // Lower = smoother but slower to react, higher = more responsive but jitterier. 100 = no smoothing.
byte RPMHysteresis = 20; // Same /10 RPM units as RPMmin/RPMmax (default 20 = 200 actual RPM)
bool UseFuelPressureCutoff = false; // whether to enforce a minimum fuel pressure at all
bool FuelPressureIsSwitch = false; // false = analog sensor(ReadFuelPressurePSI()) true for pressure switch
float FuelPressureMinPSI = 4.0; // set to your fuel system's actual minimum safe pressure
unsigned long FuelPressureTimeoutMS = 500; // how long pressure may stay below FuelPressureMinPSI before nitrous cuts
bool UseOilPressureCutoff = false; // whether to enforce a minimum oil pressure at all
bool OilPressureIsSwitch = false; // same idea as FuelPressureIsSwitch, for oil pressure
float OilPressureMinPSI = 20.0; // PLACEHOLDER - set to your engine's actual minimum safe hot oil pressure
unsigned long OilPressureTimeoutMS = 500; // same idea as FuelPressureTimeoutMS, for oil pressure


//********** CONSTANTS **********//
// Pin numbers now come from boards.h (pins_uno.h) rather than being
// hardcoded here, so this file doesn't need to change when porting boards.
const byte Voltpin = PIN_VOLTAGE;
const byte RPMpin = PIN_RPM;
const byte TransBrakepin = PIN_TRANSBRAKE;
const byte NitrousActiveled = PIN_NITROUS_LED;
const byte NitrousRelay1 = PIN_RELAY1;
const byte NitrousRelay2 = PIN_RELAY2; // reserved for a future 2nd nitrous stage - not driven yet
const byte NitrousRelay3 = PIN_RELAY3; // reserved for a future 3rd nitrous stage - not driven yet
const byte ControllerStatusled = PIN_STATUS_LED;
const unsigned long RPMTimeout = 500000UL; // 0.5 second
const unsigned long TransBrakeDebounceDelay = 25; // milliseconds
const unsigned long ThrottleSwitchDebounceDelay = 25; // milliseconds - only used when ThrottleType is 1 (microswitch
const unsigned long VoltageCheckInterval = 250; // milliseconds - no need to sample voltage every single loop

//********** VARIABLES **********//
//** For the LED's **//
bool FlashLED1 = false;
int led1State = LOW;
unsigned long previousled1Millis = 0;
const long led1Interval = 1000;

//** for TPS **//
byte Throttlepin = PIN_THROTTLE_TPS; //Throttle Pin, default connected to the board's TPS pin
int ThrottleCurrentStatus = 0;
int ThrottleLastStatus = 0;
bool AllowNitrousThrottle = false;
// Only used when ThrottleType is 1 (microswitch) - rest state (LOW) assumes
// the switch is open at closed throttle, matching the existing
// digitalRead(Throttlepin) == HIGH check in CheckThrottle().
bool ThrottleSwitchLastRawReading = LOW;
unsigned long ThrottleSwitchLastChangeMillis = 0;
bool ThrottleSwitchDebouncedState = false;

//** for Trans Brake **//
bool AllowNitrousTransBrake = false;
bool TransBrakeState = 0;         
bool LastTransBrakeState = 0;
bool TransBrakeRawReading = HIGH;
bool TransBrakeLastRawReading = HIGH;
unsigned long TransBrakeLastChangeMillis = 0;

bool NitrousActive = false;

//** for Nitrous Delay **//
bool AllowNitrousDelay1 = false;
bool NitrousDelay1Active = false;
unsigned long PreviousDelay1Millis;

//** for Safety Timeout **//
bool AllowNitrousSafetyTimeout = false;
bool SafetyTimeoutActive = false;
unsigned long PreviousSafetyTimeoutMillis;

//** for Voltage **//
bool AllowNitrousVoltage = true; // starts permissive; CheckVoltage() will correct this on the first sample
float BatteryVoltage = 0;
unsigned long PreviousVoltageMillis = 0;
bool VoltageBelowMin = false;
unsigned long VoltageBelowMinSinceMillis = 0;

//** for Fuel/Oil Pressure **//
bool AllowNitrousFuelPressure = true;
float FuelPressurePSI = 0;
bool FuelPressureBelowMin = false;
unsigned long FuelPressureBelowMinSinceMillis = 0;

bool AllowNitrousOilPressure = true;
float OilPressurePSI = 0;
bool OilPressureBelowMin = false;
unsigned long OilPressureBelowMinSinceMillis = 0;

//** for RPM **//
bool AllowNitrousRPM = false;
volatile unsigned long LastPulseTime = 0;
volatile unsigned long PulseInterval = 0;
long RPMPPR = 0;       // set once in setup(), doesn't need volatile
volatile long RPM = 0;

#if defined(DEBUG)
// Temporary bench-testing instrumentation: tracks loop() iteration time and
// prints a min/avg/max summary once a second, rather than every iteration
// (printing every iteration would itself dominate the timing and give
// meaningless numbers). Compiles out entirely when DEBUG is off.
unsigned long LoopTimingWindowStartMillis = 0;
unsigned long LoopTimingMinMicros = 0xFFFFFFFF;
unsigned long LoopTimingMaxMicros = 0;
unsigned long LoopTimingSumMicros = 0;
unsigned long LoopTimingCount = 0;
const unsigned long LoopTimingReportIntervalMillis = 1000;
#endif

//********** SETUP **********//

void EnforceSettingDependencies() {
  // If there's no transbrake, there's no release event to time the safety
  // timeout from - force it to start from first activation instead, so the
  // feature keeps working rather than silently going dead.
  // NOTE: once the settings menu (buttons/screen) exists, call this again
  // any time UseTransBrake is changed at runtime, not just here at boot -
  // this only runs once, at startup.
  if (!UseTransBrake) {
    SafetyTimeoutFromBrakeRelease = false;
  }
}

void setup() {

  WatchdogDisable();

  Serial.begin(9600);

  LoadConfig();

  CheckConfig();

  EnforceSettingDependencies();

  //Setup Pins
  switch (ThrottleType) {
    case 0: Throttlepin = PIN_THROTTLE_TPS; // TPS input
      break;
    case 1: Throttlepin = PIN_THROTTLE_SW; // microswitch input
      break;
  }
  pinMode(Throttlepin, INPUT);
  pinMode(Voltpin, INPUT);
  pinMode(RPMpin, INPUT);
  pinMode(TransBrakepin, INPUT);
    if (FuelPressureIsSwitch) {
    pinMode(PIN_FUEL_PRESSURE, INPUT_PULLUP);
  }
  if (OilPressureIsSwitch) {
    pinMode(PIN_OIL_PRESSURE, INPUT_PULLUP);
  }
  pinMode(NitrousRelay1, OUTPUT);
  pinMode(NitrousRelay2, OUTPUT);
  pinMode(NitrousRelay3, OUTPUT);
  pinMode(NitrousActiveled, OUTPUT);
  pinMode(ControllerStatusled, OUTPUT);

  //Turn OFF any power to the relay
  digitalWrite(NitrousRelay1, HIGH);
  digitalWrite(NitrousRelay2, HIGH); // reserved for future 2nd stage, kept off
  digitalWrite(NitrousRelay3, HIGH); // reserved for future 3rd stage, kept off
  digitalWrite(NitrousActiveled, LOW);

  attachInterrupt(digitalPinToInterrupt(RPMpin), GetRPM, RISING); // Interrupt on the tach input pin
  RPMPPR = long(60e5 / PPR); // microseconds in a minute(60e6) / pulse per revolution. Using 60e5 to round down. ex. 7500RPM wil read 750.

  digitalWrite(ControllerStatusled, HIGH);

#if defined(DEBUG)
  Serial.println("Nitrous system is armed!");
  Serial.println(BOARD_NAME);
  if ((RPMmin + RPMHysteresis) >= (RPMmax - RPMHysteresis)) {
    Serial.println("WARNING: RPMHysteresis leaves no valid RPM window - nitrous can never activate!");
  }
  if (ThrottleHysteresis >= ActivePercent) {
    Serial.println("WARNING: ThrottleHysteresis >= ActivePercent - nitrous will only cut out via other conditions, never via throttle position.");
  }
  if (!UseTransBrake && SafetyTimeoutFromBrakeRelease) {
    // Should never actually print - EnforceSettingDependencies() corrects this
    // combination above. Left as a backstop in case that ever gets bypassed
    // (e.g. a future settings-menu bug, or a value poked in some other way).
    Serial.println("WARNING: UseTransBrake is false but SafetyTimeoutFromBrakeRelease is true - there's no release event, so the safety timer will never start. Set SafetyTimeoutFromBrakeRelease to false.");
  }
#endif

  WatchdogEnable();

#if defined(DEBUG)
  LoopTimingWindowStartMillis = millis();
#endif

}


//********** MAIN LOOP **********//

void loop() {

#if defined(DEBUG)
  unsigned long loopStartMicros = micros();
#endif

  WatchdogPet(); // must run every iteration - see Watchdog.h

  if (FlashLED1 == true) {
    FlashControllerLED();
  }

  CheckTransBrake();

  if (NitrousDelay1Active == true) {
    NitrousDelay1();
  }

  CheckVoltage();

  ReadPressureSensors();

  CheckFuelPressure();

  CheckOilPressure();

  CheckThrottle();

  UpdateRPM();

  CheckRPM();

  CheckSafetyTimeout();

  NitrousOnOff();

  UpdateDisplay();

#if defined(DEBUG)
  unsigned long loopElapsedMicros = micros() - loopStartMicros;
  if (loopElapsedMicros < LoopTimingMinMicros) LoopTimingMinMicros = loopElapsedMicros;
  if (loopElapsedMicros > LoopTimingMaxMicros) LoopTimingMaxMicros = loopElapsedMicros;
  LoopTimingSumMicros += loopElapsedMicros;
  LoopTimingCount++;

  if (millis() - LoopTimingWindowStartMillis >= LoopTimingReportIntervalMillis) {
    Serial.print("Loop timing (us) min/avg/max: ");
    Serial.print(LoopTimingMinMicros);
    Serial.print("/");
    Serial.print(LoopTimingSumMicros / LoopTimingCount);
    Serial.print("/");
    Serial.print(LoopTimingMaxMicros);
    Serial.print("  iterations/sec: ");
    Serial.println(LoopTimingCount);

    LoopTimingMinMicros = 0xFFFFFFFF;
    LoopTimingMaxMicros = 0;
    LoopTimingSumMicros = 0;
    LoopTimingCount = 0;
    LoopTimingWindowStartMillis = millis();
  }
#endif

}

/********** FUNCTIONS **********/

void FlashControllerLED() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousled1Millis >= led1Interval) {
    previousled1Millis = currentMillis;
    if (led1State == LOW) {
      led1State = HIGH;
    } else {
      led1State = LOW;
    }
    digitalWrite(ControllerStatusled, led1State);
  }
}

// Generic switch debounce, reused for the transbrake and the throttle
// microswitch (and any future digital switch input). Only updates
// debouncedState once the raw pin reading has held steady for debounceDelay
// milliseconds, filtering out mechanical/electrical bounce rather than
// reacting to every raw transition.
void DebouncePin(byte pin, unsigned long debounceDelay, bool &lastRawReading, unsigned long &lastChangeMillis, bool &debouncedState) {
  bool reading = digitalRead(pin);

  if (reading != lastRawReading) {
    lastChangeMillis = millis();
  }

  if ((millis() - lastChangeMillis) >= debounceDelay) {
    debouncedState = reading;
  }

  lastRawReading = reading;
}

void DebounceTransBrake() {
  DebouncePin(TransBrakepin, TransBrakeDebounceDelay, TransBrakeLastRawReading, TransBrakeLastChangeMillis, TransBrakeState);
}


void CheckTransBrake() {
  if (!UseTransBrake) {
    // No transbrake in this setup - never gates nitrous, and there's no
    // release event for other features to hook into (see the startup
    // warning if SafetyTimeoutFromBrakeRelease is left true with this off).
    AllowNitrousTransBrake = true;
    AllowNitrousDelay1 = true;
    return;
  }

  DebounceTransBrake();

  // Gate nitrous while the brake is actively held, unless nitrous-on-brake
  // is explicitly allowed. Re-evaluated every loop, not just on transitions.
  AllowNitrousTransBrake = !(TransBrakeState == HIGH && UseNitrousOnBrake == false);

  // Detect the release/reapply edges exactly once per transition - this is
  // also where the post-release delay and the safety timeout hook in, and it
  // now runs the same way regardless of whether UseDelay is on, so neither
  // feature silently stops working depending on Delay1Time's value.
  if (TransBrakeState != LastTransBrakeState) {
    if (TransBrakeState == LOW) {
#if defined(DEBUG)
      Serial.println("TransBrake Released");
#endif
      if (UseDelay) {
        NitrousDelay1Active = true;
        PreviousDelay1Millis = millis();
#if defined(DEBUG)
        Serial.println("Delay Started");
#endif
        NitrousDelay1(); // will set AllowNitrousDelay1 false immediately, true once Delay1Time elapses
      } else {
        AllowNitrousDelay1 = true; // no post-release delay configured - allow right away
      }

      if (SafetyTimeoutFromBrakeRelease) {
        SafetyTimeoutActive = true;
        PreviousSafetyTimeoutMillis = millis();
      }
    } else {
      // Reapplied (re-staged) - re-arm the safety timeout for the next run,
      // and require a fresh release (+ delay, if used) before the next one.
      AllowNitrousSafetyTimeout = true;
      SafetyTimeoutActive = false;
      if (!UseNitrousOnBrake) {
        AllowNitrousDelay1 = false;
      }
    }
    LastTransBrakeState = TransBrakeState;
  }

  // Power-on edge case: if UseNitrousOnBrake is true and the brake is
  // already held at boot, there's no release transition to catch - permit
  // directly instead of waiting forever for an edge that will never come.
  if (TransBrakeState == HIGH && UseNitrousOnBrake == true && !AllowNitrousDelay1) {
    AllowNitrousDelay1 = true;
  }
}

void NitrousDelay1() {
  AllowNitrousDelay1 = false;
  unsigned long CurrentMillis;

  CurrentMillis = millis();
  if (CurrentMillis - PreviousDelay1Millis >= Delay1Time) {
    AllowNitrousDelay1 = true;
#if defined(DEBUG)
    Serial.println("Delay Ended");
#endif
    NitrousDelay1Active = false;
  }
}


void GetRPM() {
  unsigned long PulseTime = micros();
  unsigned long interval = PulseTime - LastPulseTime;
  if (interval > 0) {
    PulseInterval = interval;
  }
  LastPulseTime = PulseTime;
}


void UpdateRPM() {
  noInterrupts();
  unsigned long lastPulse = LastPulseTime;
  unsigned long interval = PulseInterval;
  interrupts();

  if (micros() - lastPulse > RPMTimeout) {
    RPM = 0; // stall detected - snap straight to 0, no smoothing delay here
    return;
  }

  if (interval == 0) {
    return; // no new pulse data since last check
  }

  long rawRPM = RPMPPR / interval;

  if (RPM > 0 && (rawRPM > RPM * 2 || rawRPM < RPM / 2)) {
    return; // ignore this sample, keep the previous smoothed RPM
  }

  RPM = ((long)RPMSmoothingFactor * rawRPM + (100 - RPMSmoothingFactor) * RPM) / 100;
}


void CheckSafetyTimeout() {
  if (SafetyTimeoutActive && (millis() - PreviousSafetyTimeoutMillis >= SafetyTimeoutDuration)) {
    AllowNitrousSafetyTimeout = false; // tripped - stays latched off until the transbrake is reapplied
    SafetyTimeoutActive = false;       // stop counting, the trip itself is now what's holding nitrous off
#if defined(DEBUG)
    Serial.println("Safety timeout reached - nitrous cut off");
#endif
  }
}

// Shared by voltage, fuel pressure, and oil pressure: allows a value to dip
// below minValue for up to timeoutMS before reporting unsafe, but reports
// safe again the instant it recovers - no equivalent grace period on the way
// back up, since there's no reason to stay cautious once it's actually fine.
bool CheckMinThreshold(float currentValue, float minValue, unsigned long timeoutMS, bool &belowMin, unsigned long &belowMinSinceMillis) {
  if (currentValue < minValue) {
    if (!belowMin) {
      belowMin = true;
      belowMinSinceMillis = millis();
    }
    if (millis() - belowMinSinceMillis >= timeoutMS) {
      return false; // sustained below minimum - cut nitrous
    }
    return true; // still within the grace period
  }

  belowMin = false; // recovered - clear immediately, no delay on the way back up
  return true;
}


void CheckVoltage() {
  if (!UseLowVoltProtect) {
    AllowNitrousVoltage = true; // protection disabled by the user - never inhibit on voltage
    VoltageBelowMin = false;
    return;
  }

  // Only sample a few times a second (per VoltageCheckInterval) rather than
  // every loop - battery voltage doesn't change fast enough to need more,
  // and it keeps the ADC free for other reads.
  unsigned long currentMillis = millis();
  if (currentMillis - PreviousVoltageMillis < VoltageCheckInterval) {
    return;
  }
  PreviousVoltageMillis = currentMillis;

  // Board-agnostic conversion: ADC_REF_VOLTAGE, ADC_MAX, and
  // VOLTAGE_DIVIDER_RATIO all come from boards.h, so this line doesn't
  // change when porting to a board with a different ADC or reference voltage.
  BatteryVoltage = analogRead(Voltpin) * (ADC_REF_VOLTAGE / (float)ADC_MAX) * VOLTAGE_DIVIDER_RATIO;

  bool wasAllowed = AllowNitrousVoltage;
  AllowNitrousVoltage = CheckMinThreshold(BatteryVoltage, LowVoltProtect, LowVoltTimeoutMS, VoltageBelowMin, VoltageBelowMinSinceMillis);

#if defined(DEBUG)
  if (wasAllowed && !AllowNitrousVoltage) {
    Serial.print("Low voltage inhibit: ");
    Serial.println(BatteryVoltage);
  }
#endif
}

void ReadPressureSensors() {
  // Conversion math lives in Sensors.h, not here - see that file to fill in
  // the actual sensor-specific formulas once sensors are chosen, or to
  // adjust the switch polarity assumption if using a pressure switch instead.
  FuelPressurePSI = FuelPressureIsSwitch ? ReadFuelPressureSwitchPSI() : ReadFuelPressurePSI();
  OilPressurePSI = OilPressureIsSwitch ? ReadOilPressureSwitchPSI() : ReadOilPressurePSI();
}

void CheckFuelPressure() {
  if (!UseFuelPressureCutoff) {
    AllowNitrousFuelPressure = true;
    return;
  }
  bool wasAllowed = AllowNitrousFuelPressure;
  AllowNitrousFuelPressure = CheckMinThreshold(FuelPressurePSI, FuelPressureMinPSI, FuelPressureTimeoutMS, FuelPressureBelowMin, FuelPressureBelowMinSinceMillis);
#if defined(DEBUG)
  if (wasAllowed && !AllowNitrousFuelPressure) {
    Serial.print("Fuel pressure inhibit: ");
    Serial.println(FuelPressurePSI);
  }
#endif
}

void CheckOilPressure() {
  if (!UseOilPressureCutoff) {
    AllowNitrousOilPressure = true;
    return;
  }
  bool wasAllowed = AllowNitrousOilPressure;
  AllowNitrousOilPressure = CheckMinThreshold(OilPressurePSI, OilPressureMinPSI, OilPressureTimeoutMS, OilPressureBelowMin, OilPressureBelowMinSinceMillis);
#if defined(DEBUG)
  if (wasAllowed && !AllowNitrousOilPressure) {
    Serial.print("Oil pressure inhibit: ");
    Serial.println(OilPressurePSI);
  }
#endif
}

void CheckThrottle() {
  switch (ThrottleType) {
    case 0: //read throttle postion sensor pin and convert to a percentage of throttle opening
      ThrottleCurrentStatus = map(analogRead(Throttlepin), tpsMIN, tpsMAX, 0, 100);

      // int math + clamp avoids underflow if someone sets ThrottleHysteresis > ActivePercent
      int throttleCutoff = (int)ActivePercent - (int)ThrottleHysteresis;
      if (throttleCutoff < 0) throttleCutoff = 0;

      switch (AllowNitrousThrottle) {
        case false:
          if (ThrottleCurrentStatus >= ActivePercent) {
            AllowNitrousThrottle = true;
          }
          break;
        case true:
          if (ThrottleCurrentStatus < throttleCutoff) {
            AllowNitrousThrottle = false;
          }
          break;
      }
      break;
    case 1:
      DebouncePin(Throttlepin, ThrottleSwitchDebounceDelay, ThrottleSwitchLastRawReading, ThrottleSwitchLastChangeMillis, ThrottleSwitchDebouncedState);
      ThrottleCurrentStatus = ThrottleSwitchDebouncedState ? HIGH : LOW;
      AllowNitrousThrottle = ThrottleSwitchDebouncedState;
      break;
  }
}

void CheckRPM() {
  switch (NitrousActive) {
    case true:
      // Cut off immediately at the original safety bounds - no hysteresis on
      // the way out, this is the protective edge and shouldn't be delayed.
      if (RPM < RPMmin || RPM > RPMmax) {
        AllowNitrousRPM = false;
      }
      break;
    case false:
      // To re-activate, require RPM to sit solidly inside the window - past
      // RPMmin and short of RPMmax by RPMHysteresis - so hovering right at
      // either boundary can't cause rapid on/off cycling.
      if (RPM > (RPMmin + RPMHysteresis) && RPM < (RPMmax - RPMHysteresis)) {
        AllowNitrousRPM = true;
      } else {
        AllowNitrousRPM = false;
      }
      break;
  }
}


void NitrousOnOff() {
  if (AllowNitrousThrottle == true && AllowNitrousTransBrake == true && AllowNitrousDelay1 == true && AllowNitrousRPM == true && AllowNitrousVoltage == true && AllowNitrousSafetyTimeout == true && AllowNitrousFuelPressure == true && AllowNitrousOilPressure == true) {
    if (!NitrousActive && !SafetyTimeoutFromBrakeRelease && !SafetyTimeoutActive) {
      // First activation this run, and configured to start the safety
      // timeout clock here instead of at brake release.
      SafetyTimeoutActive = true;
      PreviousSafetyTimeoutMillis = millis();
    }
    digitalWrite(NitrousRelay1, LOW);
    digitalWrite(NitrousActiveled, HIGH);
    NitrousActive = true;
  } else {
    digitalWrite(NitrousRelay1, HIGH);
    digitalWrite(NitrousActiveled, LOW);
    NitrousActive = false;
  }
  // NitrousRelay2 and NitrousRelay3 intentionally not driven here yet -
  // reserved for future 2nd/3rd nitrous stages. Both are on PWM-capable
  // pins already, ready for progressive/duty-cycle control if that's ever
  // added - see pins/arduino_uno.h.
}


void UpdateDisplay() {
#if defined(DEBUG)
  if (ThrottleCurrentStatus != ThrottleLastStatus) {
    Serial.print(ThrottleCurrentStatus);
    Serial.print("% ");
    Serial.print(RPM * 10);
    Serial.print(" ");
    Serial.print(BatteryVoltage);    
    if (TransBrakeState == true) {
      Serial.print(" ");
      Serial.print("TransBrake On");
    }
    if (NitrousActive == true) {
      Serial.print(" ");
      Serial.print("Nitrous Active");
    }
    Serial.println();
  }
  ThrottleLastStatus = ThrottleCurrentStatus;
#endif
}

void CheckButtons() {
  // Stubbed out - future plan: buttons/rotary encoder + LCD menu for
  // configuring settings on-device instead of hardcoded defaults above.
}

void LoadConfig() {
  SettingsLoad(); // board-specific storage lives in Settings.h
}

void CheckConfig() {
  // PATCH: original condition checked "tpsMIN == 0" to detect a failed/first
  // config load, but the default value assigned to tpsMIN above is 1, not 0 -
  // so that check could never be true and this safety check was dead code.
  // Fixed to match the actual default, and now compares tpsMAX against
  // ADC_MAX instead of a hardcoded 1023 so it stays correct on any board.
  // Still a no-op in practice until SettingsLoad() actually reads from storage.
  if (ThrottleType == 0 && tpsMIN == 1 && tpsMAX == ADC_MAX) {
#if defined(DEBUG)
    Serial.println("Defaults loaded! Setup Needed!");
#endif
    FlashLED1 = true;
    FlashControllerLED();
  }
}

void SaveConfig() {
  SettingsSave(); // board-specific storage lives in Settings.h
}
