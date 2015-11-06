/*
  Sprayduino Nitrous Controller - October 2015
  Turns on or off nitrous control relay based on TPS position.

  Reads voltage from Throttle Position Sensor on analog pin 2,
  Read Trans Brake input on digital pin 7 to inhibit nitrous activation
  Turns on or off nitrous activation relays on digital pin 9 and 10

  Future plans:
    Nitrous relay safety timeout - shut off after n# seconds
    Read engine RPM fron tach.Safety time out
    Minimum and maximum rpm for nitrous activation - window switch.
    Nitrous activation delay timer.
    Menu system to make settings user configurable - will need an LCD and buttons,rotary,etc.
    Battery reference voltage -  low voltage shutoff.
    Transbrake input - option to inhibit activation while transbrake is engaged.

  Last modified Nov 4 2015
  by Troy Bernakevitch
*/

//********** DEFAULT VALUES **********//
// After Setup the user values stored in the EEPROM will override these defaults.

int tpsMIN = 0; // TPS at closed throttle - future to be programmable
int tpsMAX = 1023; // TPS at WOT - future to be programmable
byte ActivePercent = 95; // percentage of throttle opening at which to activate nitrous
unsigned long DelayTime = 0; // The amount of time to delay nitrous activation on release of transbrake
byte LowVoltProtect = 11; // for low voltage protection to shut down nitrous system
byte PPR = 4;// Pulses Per Revolution, 4 for 8 cyl, 3 for 6 cyl, and 2 for 4 cyl.


//********** CONSTANTS **********//
const byte TPSpin = A2; //TPS Pin connected to Analog 2
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
int TPSCurrentStatus = 0;
int TPSLastStatus = 0;
bool AllowNitrousTPS = false;

//** for Trans Brake **//
bool AllowNitrousTransBrake = false;
bool TransBrakeState = false;

bool NitrousActive = false;

//** for RPM **//
unsigned long LastPulseTime;
unsigned long PulseInterval = 0;
int RPMPPR = long(60e6 / PPR); // micrseoonds in a minute adjusted for the Pulse Per Revolution to calculate RPM
int RPM = 0;


//********** SETUP **********//

void setup() {

  Serial.begin(9600); //for debug only, comment out for realtime use

  //Setup Pins
  pinMode(TPSpin, INPUT);
  pinMode(Voltpin, INPUT);
  pinMode(RPMpin, INPUT_PULLUP);
  pinMode(TransBrakepin, INPUT);
  pinMode(NitrousRelay1, OUTPUT);
  pinMode(NitrousRelay2, OUTPUT);
  pinMode(NitrousActiveled, OUTPUT);
  pinMode(ControllerStatusled, OUTPUT);

  //Turn OFF any power to the relay
  digitalWrite(NitrousRelay1, HIGH);
  digitalWrite(NitrousRelay2, HIGH);
  digitalWrite(NitrousActiveled, LOW);

  attachInterrupt(digitalPinToInterrupt(RPMpin), GetRPM, RISING); // Interrupt on pin 2 for RPM input

  LoadConfig();

  CheckConfig(); // check if configuration saved in EEPROM loaded

  digitalWrite(ControllerStatusled, HIGH);
  Serial.println("Nitrous system is armed!"); // for debug purposes only
}


//********** MAIN LOOP **********//

void loop() {

  if (FlashLED1 == true) {
    FlashControllerLED();
  }

  CheckTransBrake();
  GetRPM();
  CheckTPS();
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

void CheckButtons() {

}

  TransBrakeState = digitalRead(TransBrakepin);

  if (!DelayTime) { // if there is no delay time

    if (TransBrakeState == HIGH && NitrousOnBrake == false) {
      AllowNitrousTransBrake = false;
    }
    else  {
      AllowNitrousTransBrake = true;
    }
  }
}

void GetRPM() {
  unsigned long PulseTime = micros();
  PulseInterval = PulseTime - LastPulseTime;
  LastPulseTime = PulseTime;
}

void CheckTPS() {
  //read throttle postion sensor pin and convert to a percentage of throttle opening
  TPSCurrentStatus = map(analogRead(TPSpin), tpsMIN, tpsMAX, 0, 100);

  //Allow Nitrous to be active only if above the set Percentage of throttle opening
  if (TPSCurrentStatus > ActivePercent) {
    AllowNitrousTPS = true;
  } else if (TPSCurrentStatus < ActivePercent) {
    AllowNitrousTPS = false;
  }
}

void NitrousOnOff() {
  if (AllowNitrousTPS == true && AllowNitrousTransBrake == true) {
    digitalWrite(NitrousRelay1, LOW);
    digitalWrite(NitrousActiveled, HIGH);
    NitrousActive = true;
  }
  else {
    digitalWrite(NitrousRelay1, HIGH);
    digitalWrite(NitrousActiveled, LOW);
    NitrousActive = false;
  }
}

void UpdateDisplay() {
  if (TPSCurrentStatus != TPSLastStatus) {
    Serial.print(TPSCurrentStatus);
    Serial.print("% ");
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
  TPSLastStatus = TPSCurrentStatus;
}

void CheckConfig() {
  if (tpsMIN == 0 && tpsMAX == 1023) { //if TPS has not been set assume either first run or config failed to load
    Serial.println("Defaults loaded! Setup Needed!");
    FlashLED1 = true;
    FlashControllerLED();
  }
}

void SaveConfig() {

}

