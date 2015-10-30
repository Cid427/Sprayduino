/*
  Sprayduino Nitrous Controller - October 2015
  Turns on or off nitrous control relay based on TPS position.

  Reads voltage from Throttle Position Sensor on analog pin 2,
  Turns on or off a nitrous activation solenoid on digital pin 7
*/


// USER DEFINED VALUES,   --- hope to move to a setup menu in the future
int ActivePercent = 95; // percentage of throttle opening at which to activate nitrous
int tpsMIN = 0; // TPS voltage at closed throttle - future to be programmable
int tpsMAX = 5; // TPS volage at WOT - future to be programmable
// int LowVoltProtect; // not used yet, but for a low voltage protection to shut down nitrous system

// Constants won't change:
const int TPSpin = A2; //TPS Pin connected to Analog 2
const int NitrousRelay = 7; //Relay connected to Digital 7

// Variables will change:
int TPSCurrentStatus = 0;
int TPSLastStatus = 0;
int tpsADC = 0;

void setup() {
  Serial.begin(9600); //for debug only, comment out for realtime use

  //Setup Pins
  pinMode(TPSpin, INPUT);
  pinMode(NitrousRelay, OUTPUT);

  //Turn OFF any power to the relay
  digitalWrite(NitrousRelay, LOW);
  delay(500); //half second delay
  Serial.println("Nitrous system is armed!"); // for debug purposes only
}

void loop() {
  
}
