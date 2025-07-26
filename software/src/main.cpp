#include <Arduino.h>

#include "PeristalticPumpController.hpp"

// Note: Arduino Nano supports PWM on pins 3, 5, 6, 9, 10, and 11
// Pin that the pump is connected to
static const uint8_t PUMP_CTL_PIN = 3;
// The pump controller instance
PeristalticPumpController* gPPC;

// The test identifier
int state = -2; // Set this to a higher value to skip earlier tests
// How long to wait during a given test
unsigned long stateWaitTime = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  // Wait for initialisation of the serial interface
  while(!Serial);
  // Set up the pump controller
  gPPC = new PeristalticPumpController(PUMP_CTL_PIN, true, 70.0f);

  Serial.println("Starting pump control test in 10 seconds...");
  delay(10000);
}

void loop() {
  static unsigned long lastActionTime = 0;
  gPPC->controlLoop();

  switch (state) {
    case -2:
      lastActionTime = millis();
      Serial.print("Running at full speed for 3 mins to check output flow rate...");
      gPPC->setTargetPumpSpeed(100.0f);
      stateWaitTime = 1000UL * 60UL * 3UL; // wait 3 minutes so we have time to check the output flow rate
      state++;
      break;
    case 0:
      lastActionTime = millis();
      Serial.print("Ramping pump up to run at half speed for 3 mins...");
      gPPC->setTargetPumpSpeed(50.0f); // Set pump speed to 50% of max speed
      stateWaitTime = 1000UL * 60UL * 3UL; // wait 3 minutes so we have time to check the output flow rate
      state++;
      break;
    case 2:
      lastActionTime = millis();
      Serial.print("Ramping pump down to 35 ml/min for 3 mins...");
      gPPC->setTargetPumpFlowRate(35.0f);
      stateWaitTime = 1000UL * 60UL * 3UL; // wait 3 minutes so we have time to check the output flow rate
      state++;
      break;
    case 4:
      lastActionTime = millis();
      Serial.print("Stopping pump...");
      gPPC->setTargetPumpSpeed(0);
      stateWaitTime = 0; // no need to wait - should be obvious when the pump stops
      state++;
      break;
    case 6:
      lastActionTime = millis();
      Serial.print("Pumping 80 ml...");
      gPPC->pumpTargetVolume(80.0f); // Pump 80 ml of liquid
      stateWaitTime = 0; // no need to wait - we should have an absolte measure of volume
      state++;
      break;
    default:
      // Wait for as long as required for the test
      if (millis() - lastActionTime >= stateWaitTime) {
        // Only do anything if we've finished executing the last action
        if (gPPC->getPumpTargetMode() == PPC::PumpTargetMode::None) {
          unsigned long elapsedTime = millis() - lastActionTime;
          Serial.println("Done in " + String(elapsedTime) + " ms.");
          // Wait 5 seconds before the next action
          delay(5000);
          // Move on to the next action
          state++;
          if (state > 6) {
            state = 0; // Reset state to loop through actions again
            delay(60000); // Wait for a minute before repeating the tests
          }
        }
      }
  }
}

