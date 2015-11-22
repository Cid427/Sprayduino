/*
  Sprayduino Nitrous Controller - October 2015
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
    Battery reference voltage -  low voltage shutoff.

  Last modified Nov 21 2015
  by Troy Bernakevitch
*/

#include <EEPROM.h>
#include <EEPROMAnything.h>

#define VERSION " Sprayduino V0.3"
#define DEBUG (1)


//********** DEFAULT VALUES **********//
// After Setup the user values stored in the EEPROM will override these defaults.

int tpsMIN = 1; // TPS at closed throttle - future to be programmable
int tpsMAX = 1023; // TPS at WOT - future to be programmable
int ThrottleType = 0; // Type 0 for a TPS or 1 for microswtich.
byte ActivePercent = 95; // percentage of throttle opening at which to activate nitrous
unsigned long Delay1Time = 1000; // The amount of time to delay nitrous activation on release of transbrake
bool UseNitrousOnBrake = false; // allow nitrous to be active when transbrake is on or not
byte LowVoltProtect = 11; // for low voltage protection to shut down nitrous system
int RPMmin = 350; // Minimum RPM for nitrous activation divided by 10 RPM only needs to read in multiples of 10
int RPMmax = 750; // Maximum RPM for nitrous activation divided by 10
byte PPR = 4; // Pulses Per Revolution, typical distibutor applications 4 for 8 cyl, 3 for 6 cyl, and 2 for 4 cyl.


//********** CONSTANTS **********//
const byte Voltpin = A4; //Input to monitor Battery Voltage on analog 4
const byte RPMpin = 2; //Input for tachometer in on digital 2
const byte TransBrakepin = 7; //Input for Transbrake connected to Digital 7
const byte NitrousActiveled = 8; //LED to indicate nitrous is Active on digital 8
const byte NitrousRelay1 = 9; //Relay 1 connected to Digital 9. Nitrous relays on 9,10 for PWM if ever needed
const byte NitrousRelay2 = 10; //Relay 2 connected to Digital 10, either as 2nd stage or together with relay 1
const byte ControllerStatusled = 13; //LED to indicate the controller status on digital 13


//********** VARIABLES **********//
//** For the LED's **//
bool FlashLED1 = false;
int led1State = LOW;
unsigned long previousled1Millis = 0;
const long led1Interval = 1000;

//** for TPS **//
byte Throttlepin = A2; //Throttle Pin, default connected to Analog 2
int ThrottleCurrentStatus = 0;
int ThrottleLastStatus = 0;
bool AllowNitrousThrottle = false;

//** for Trans Brake **//
bool AllowNitrousTransBrake = false;
bool TransBrakeState = 0;
bool LastTransBrakeState = 0;

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
unsigned long LastPulseTime;
unsigned long PulseInterval = 0;
long RPMPPR = 0;
long RPM = 0;


//********** SETUP **********//

void setup() {

  Serial.begin(9600);

  LoadConfig();

  CheckConfig();

  //Setup Pins
  switch (ThrottleType) {
    case 0: Throttlepin = A2; // analog pin 2 for TPS
      break;
    case 1: Throttlepin = 16; // use analog pin 2 as digital pin for microswitch
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
  digitalWrite(NitrousRelay2, HIGH);
  digitalWrite(NitrousActiveled, LOW);

  attachInterrupt(digitalPinToInterrupt(RPMpin), GetRPM, RISING); // Interrupt on digital pin 2 for RPM input
  RPMPPR = long(60e5 / PPR); // microseconds in a minute(60e6) / pulse per revolution. Using 60e5 to round down. ex. 7500RPM wil read 750.

  digitalWrite(ControllerStatusled, HIGH);
#if defined(DEBUG)
  Serial.println("Nitrous system is armed!");
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

void CheckTransBrake() {
  TransBrakeState = digitalRead(TransBrakepin);

  if (!Delay1Time) { // if there is no delay time
    if (TransBrakeState == HIGH && UseNitrousOnBrake == false) {
      AllowNitrousTransBrake = false;
    } else  {
      AllowNitrousTransBrake = true;
      AllowNitrousDelay1 = true;
    }
  } else { // if there is a delay
    if (TransBrakeState == HIGH && UseNitrousOnBrake == false) { //this does'nt make sense if 'usenitrousonbrake is false then there would be no way to start the timer anyway
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
  PulseInterval = PulseTime - LastPulseTime;
  LastPulseTime = PulseTime;
  RPM = RPMPPR / PulseInterval;
}

void CheckVoltage() {

}

void CheckThrottle() {
  switch (ThrottleType) {
    case 0: //read throttle postion sensor pin and convert to a percentage of throttle opening
      ThrottleCurrentStatus = map(analogRead(Throttlepin), tpsMIN, tpsMAX, 0, 100);
      if (ThrottleCurrentStatus > ActivePercent) {  //Allow Nitrous to be active only if above the set active percentage
        AllowNitrousThrottle = true;
      } else if (ThrottleCurrentStatus < ActivePercent) {
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

}

void LoadConfig() {

}

void CheckConfig() {
  if (ThrottleType == 0 && tpsMIN == 0 && tpsMAX == 1023) { //if TPS has not been set assume either first run or config failed to load
#if defined(DEBUG)
    Serial.println("Defaults loaded! Setup Needed!");
#endif
    FlashLED1 = true;
    FlashControllerLED();
  }
}

void SaveConfig() {

}

