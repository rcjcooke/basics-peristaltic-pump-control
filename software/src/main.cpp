#include <Arduino.h>

#include "PeristalticPumpController.hpp"

// Note: Arduino Nano supports PWM on pins 3, 5, 6, 9, 10, and 11
// Pin that the pump is connected to
static const uint8_t PUMP_CTL_PIN = 3;
// The pump controller instance
PeristalticPumpController* gPPC;

int state = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  // Wait for initialisation of the serial interface
  while(!Serial);
  // Set up the pump controller
  gPPC = new PeristalticPumpController(PUMP_CTL_PIN, true, 80.0f);

  Serial.println("Starting pump control test...");

}

void loop() {
  static unsigned long lastActionTime = 0;
  gPPC->controlLoop();

  switch (state) {
    case 0:
      lastActionTime = millis();
      Serial.print("Ramping pump up to run at half speed...");
      gPPC->setTargetPumpSpeed(50.0f); // Set pump speed to 50% of max speed
      state++;
      break;
    case 2:
      lastActionTime = millis();
      Serial.print("Ramping pump down to 20 ml/min...");
      gPPC->setTargetPumpFlowRate(20.0f); // Set pump flow rate to 20 ml/min
      state++;
      break;
    case 4:
      lastActionTime = millis();
      Serial.print("Stopping pump...");
      gPPC->setTargetPumpSpeed(0);
      state++;
      break;
    case 6:
      lastActionTime = millis();
      Serial.print("Pumping 80 ml...");
      gPPC->pumpTargetVolume(80.0f); // Pump 80 ml of liquid
      state++;
      break;
    default:
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
        }
      }
  }
}

