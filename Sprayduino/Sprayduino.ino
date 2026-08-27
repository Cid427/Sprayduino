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
    Battery reference voltage -  low voltage shutoff.
    Low voltage warning LED
    RPM Smoothing
    Second nitrous stage on NitrousRelay2 (currently wired but unused)
    Port to STM32 - add pins_stm32.h (see board_config.h)

  Original by Troy Bernakevitch, Nov 21 2015
  Patched: bug fixes to RPM handling, transbrake edge case, throttle edge
  case, dead config-detection code, and transbrake debounce (see "PATCH:"
  comments). Then split into a board-portable layout: pin numbers and ADC
  resolution now live in board_config.h / pins_uno.h instead of this file,
  and EEPROM specifics are isolated in Settings.h. Porting to a new board
  should never require editing this file - see board_config.h.
*/

#include "boards.h" // pin numbers, ADC_MAX, and storage capability for this board
#include "Settings.h"     // SettingsLoad()/SettingsSave() - board-specific storage lives here

#define VERSION " Sprayduino V0.5"
#define DEBUG (1)


//********** DEFAULT VALUES **********//
// After Setup the user values stored in the EEPROM will override these defaults.

int tpsMIN = 1; // TPS at closed throttle - future to be programmable
int tpsMAX = ADC_MAX; // TPS at WOT - future to be programmable. Was hardcoded 1023; now follows this board's ADC resolution.
int ThrottleType = 0; // Type 0 for a TPS or 1 for microswtich.
byte ActivePercent = 95; // percentage of throttle opening at which to activate nitrous
unsigned long Delay1Time = 1000; // The amount of time to delay nitrous activation on release of transbrake
bool UseNitrousOnBrake = false; // allow nitrous to be active when transbrake is on or not
byte LowVoltProtect = 11; // for low voltage protection to shut down nitrous system
int RPMmin = 350; // Minimum RPM for nitrous activation divided by 10 RPM only needs to read in multiples of 10
int RPMmax = 750; // Maximum RPM for nitrous activation divided by 10
byte PPR = 4; // Pulses Per Revolution, typical distibutor applications 4 for 8 cyl, 3 for 6 cyl, and 2 for 4 cyl.


//********** CONSTANTS **********//
// Pin numbers now come from boards.h (pins_uno.h) rather than being
// hardcoded here, so this file doesn't need to change when porting boards.
const byte Voltpin = PIN_VOLTAGE;
const byte RPMpin = PIN_RPM;
const byte TransBrakepin = PIN_TRANSBRAKE;
const byte NitrousActiveled = PIN_NITROUS_LED;
const byte NitrousRelay1 = PIN_RELAY1;
const byte NitrousRelay2 = PIN_RELAY2; // reserved for a future 2nd nitrous stage - not driven yet
const byte ControllerStatusled = PIN_STATUS_LED;
const unsigned long RPMTimeout = 500000UL; // 0.5 second
const unsigned long TransBrakeDebounceDelay = 25; // milliseconds


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

//** for RPM **//
bool AllowNitrousRPM = false;
volatile unsigned long LastPulseTime = 0;
volatile unsigned long PulseInterval = 0;
long RPMPPR = 0;       // set once in setup(), doesn't need volatile
volatile long RPM = 0;


//********** SETUP **********//

void setup() {

  Serial.begin(9600);

  LoadConfig();

  CheckConfig();

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
  pinMode(NitrousRelay1, OUTPUT);
  pinMode(NitrousRelay2, OUTPUT);
  pinMode(NitrousActiveled, OUTPUT);
  pinMode(ControllerStatusled, OUTPUT);

  //Turn OFF any power to the relay
  digitalWrite(NitrousRelay1, HIGH);
  digitalWrite(NitrousRelay2, HIGH); // reserved for future 2nd stage, kept off
  digitalWrite(NitrousActiveled, LOW);

  attachInterrupt(digitalPinToInterrupt(RPMpin), GetRPM, RISING); // Interrupt on the tach input pin
  RPMPPR = long(60e5 / PPR); // microseconds in a minute(60e6) / pulse per revolution. Using 60e5 to round down. ex. 7500RPM wil read 750.

  digitalWrite(ControllerStatusled, HIGH);
#if defined(DEBUG)
  Serial.println("Nitrous system is armed!");
  Serial.println(BOARD_NAME);
#endif
}


//********** MAIN LOOP **********//

void loop() {

  if (FlashLED1 == true) {
    FlashControllerLED();
  }

  CheckTransBrake();

  if (NitrousDelay1Active == true) {
    NitrousDelay1();
  }

  CheckVoltage(); //This really should not run every loop, 4 times a second at most would do.

  CheckThrottle();

  UpdateRPM(); // PATCH: compute RPM from ISR data here, outside the interrupt

  CheckRPM();

  NitrousOnOff();

  UpdateDisplay();

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


void DebounceTransBrake() {
  bool reading = digitalRead(TransBrakepin);

  if (reading != TransBrakeLastRawReading) {
    TransBrakeLastChangeMillis = millis();
  }

  if ((millis() - TransBrakeLastChangeMillis) >= TransBrakeDebounceDelay) {
    TransBrakeState = reading;
  }

  TransBrakeLastRawReading = reading;
}

void CheckTransBrake() {
  DebounceTransBrake(); 

  if (!Delay1Time) { // if there is no delay time
    if (TransBrakeState == HIGH && UseNitrousOnBrake == false) {
      AllowNitrousTransBrake = false;
    } else  {
      AllowNitrousTransBrake = true;
      AllowNitrousDelay1 = true;
    }
  } else { // if there is a delay
    if (TransBrakeState == HIGH && UseNitrousOnBrake == false) {
      AllowNitrousTransBrake = false;
    } else if (TransBrakeState != LastTransBrakeState) {
      if (TransBrakeState == LOW) {
        AllowNitrousTransBrake = true;
        NitrousDelay1Active = true;
#if defined(DEBUG)
        Serial.println("TransBrake Released");
        Serial.println("Delay Started");
#endif
        PreviousDelay1Millis = millis();
        NitrousDelay1();
      }
    }
    else if (TransBrakeState == HIGH && UseNitrousOnBrake == true && !AllowNitrousTransBrake) {
      AllowNitrousTransBrake = true;
      AllowNitrousDelay1 = true;
    }
    LastTransBrakeState = TransBrakeState;
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
    RPM = 0;
  } else if (interval > 0) {
    RPM = RPMPPR / interval;
  }
}

void CheckVoltage() {
  // Stubbed out - LowVoltProtect is defined but not yet enforced.
  // Future plan: read Voltpin, convert to actual battery voltage, and force
  // AllowNitrousThrottle/AllowNitrousRPM false (or a dedicated
  // AllowNitrousVoltage flag) if voltage drops below LowVoltProtect.
}

void CheckThrottle() {
  switch (ThrottleType) {
    case 0: //read throttle postion sensor pin and convert to a percentage of throttle opening
      ThrottleCurrentStatus = map(analogRead(Throttlepin), tpsMIN, tpsMAX, 0, 100);

      if (ThrottleCurrentStatus >= ActivePercent) {
        AllowNitrousThrottle = true;
      } else {
        AllowNitrousThrottle = false;
      }
      break;
    case 1:
      ThrottleCurrentStatus = digitalRead(Throttlepin);
      if (ThrottleCurrentStatus == HIGH) {
        AllowNitrousThrottle = true;
      } else {
        AllowNitrousThrottle = false;
      }
      break;
  }
}

void CheckRPM() {
  switch (NitrousActive) {
    case true:
      if (RPM < RPMmin || RPM > RPMmax) {
        AllowNitrousRPM = false;
      }
      break;
    case false:
      if (RPM > RPMmin && RPM < RPMmax) {
        AllowNitrousRPM = true;
      } else {
        AllowNitrousRPM = false;
      }
      break;
  }
}

void NitrousOnOff() {
  if (AllowNitrousThrottle == true && AllowNitrousTransBrake == true && AllowNitrousDelay1 == true && AllowNitrousRPM == true) {
    digitalWrite(NitrousRelay1, LOW);
    digitalWrite(NitrousActiveled, HIGH);
    NitrousActive = true;
  } else {
    digitalWrite(NitrousRelay1, HIGH);
    digitalWrite(NitrousActiveled, LOW);
    NitrousActive = false;
  }
  // NitrousRelay2 intentionally not driven here yet - reserved for a future
  // second nitrous stage.
}

void UpdateDisplay() {
#if defined(DEBUG)
  if (ThrottleCurrentStatus != ThrottleLastStatus) {
    Serial.print(ThrottleCurrentStatus);
    Serial.print("% ");
    Serial.print(RPM * 10);
    Serial.print(" ");
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
