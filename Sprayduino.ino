/*
  Sprayduino Nitrous Controller - October 2015
  Turns on or off nitrous control relay based on TPS position.

  Reads voltage from Throttle Position Sensor on analog pin 2,
  Turns on or off a nitrous activation solenoid on digital pin 7
  
  Last modified Oct 31 2015
  by Troy Bernakevitch
*/


// USER DEFINED VALUES,   --- hope to move to a setup menu in the future
int ActivePercent = 95; // percentage of throttle opening at which to activate nitrous
int tpsMIN = 0; // TPS at closed throttle - future to be programmable
int tpsMAX = 1023; // TPS at WOT - future to be programmable
int LowVoltProtect; // not used yet, but for a low voltage protection to shut down nitrous system

// Constants won't change:
const int TPSpin = A2; //TPS Pin connected to Analog 2
const int NitrousRelay = 7; //Relay connected to Digital 7

// Variables will change:
int TPSCurrentStatus = 0;
int TPSLastStatus = 0;
bool NitrousActive = false;

void setup() {
  Serial.begin(9600); //for debug only, comment out for realtime use

  //Setup Pins
  pinMode(TPSpin, INPUT);
  pinMode(NitrousRelay, OUTPUT);

  //Turn OFF any power to the relay
  digitalWrite(NitrousRelay, HIGH); //seems my relay board works backwards?
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
      digitalWrite(NitrousRelay, LOW);
      NitrousActive = true;
    } else if (TPSCurrentStatus < ActivePercent) {
      digitalWrite(NitrousRelay, HIGH);
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
