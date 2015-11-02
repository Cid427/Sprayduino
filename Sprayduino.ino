/*
  Sprayduino Nitrous Controller - October 2015
  Turns on or off nitrous control relay based on TPS position.

  Reads voltage from Throttle Position Sensor on analog pin 2,
  Turns on or off nitrous activation relays on digital pin 7 and 8
  
  Last modified Nov 01 2015
  by Troy Bernakevitch
*/

// USER DEFINED VALUES,   --- hope to move to a setup menu in the future
int tpsMIN = 0; // TPS at closed throttle - future to be programmable
int tpsMAX = 1023; // TPS at WOT - future to be programmable
byte ActivePercent = 95; // percentage of throttle opening at which to activate nitrous
byte LowVoltProtect; // for low voltage protection to shut down nitrous system

// Constants won't change:
const byte TPSpin = A2; //TPS Pin connected to Analog 2
const byte Voltpin = A4; //Input to monitor Battery Voltage on analog 4
const byte TransBrakepin = 9; //Input for Transbrake connected to Digital 9
const byte NitrousRelay1 = 7; //Relay 1 connected to Digital 7
const byte NitrousRelay2 = 8; //Relay 2 connected to Digital 8, either as 2nd stage or together with relay 1

// Variables will change:
int TPSCurrentStatus = 0;
int TPSLastStatus = 0;
bool NitrousActive = false;

void setup() {
  Serial.begin(9600); //for debug only, comment out for realtime use

  //Setup Pins
  pinMode(TPSpin, INPUT);
  pinMode(Voltpin, INPUT);
  pinMode(TransBrakepin, INPUT);
  pinMode(NitrousRelay1, OUTPUT);
  pinMode(NitrousRelay2, OUTPUT); 

  //Turn OFF any power to the relay
  digitalWrite(NitrousRelay1, HIGH); //seems my relay board works backwards?
  digitalWrite(NitrousRelay2, HIGH);
  delay(500); //half second delay
  Serial.println("Nitrous system is armed!"); // for debug purposes only
}

void loop() {
  
  //read throttle postion sensor pin and convert to a percentage of throttle opening
  TPSCurrentStatus = map(analogRead(TPSpin), tpsMIN, tpsMAX, 0, 100);
  
  if (TPSCurrentStatus != TPSLastStatus) {
    Serial.print(TPSCurrentStatus);
    Serial.print("% ");
    // turn on nitrous realy if TPS is above the 'Active Percent' set
    if (TPSCurrentStatus > ActivePercent) {
      digitalWrite(NitrousRelay1, LOW);
      NitrousActive = true;
    } else if (TPSCurrentStatus < ActivePercent) {
      digitalWrite(NitrousRelay1, HIGH);
      NitrousActive = false;
    }
    if (NitrousActive == true) {
      Serial.print(" ");
      Serial.print("Nitrous Active");
    }
    Serial.println();
    TPSLastStatus = TPSCurrentStatus;
  }
}

void CheckButtons() {
  
}

void GetRPM() {
  
}

void NitrousOnOff() {
  
}

void UpdateDisplay() {
  
}

