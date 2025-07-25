#ifndef __PERISTALTICPPUMPCONTROLLER_H_INCLUDED__
#define __PERISTALTICPPUMPCONTROLLER_H_INCLUDED__

#include <Arduino.h>
#include <SPI.h>

namespace PPC {

  enum class PumpTargetMode {
    None,          // No target mode set
    Speed,         // Target speed in percentage of maximum speed
    FlowRate,      // Target flow rate in ml/min
    Volume         // Target volume to pump in ml
  };
  
}

class PeristalticPumpController {

public:

  /*******************************
   * Constants
   *******************************/
  // The time period over which the pump speed is ramped up or down
  static const unsigned long RAMP_TIME_MS = 100;

  /*******************************
   * Constructors
   *******************************/
  // Ramping should be used to avoid the generation of back EMF from the pump
  PeristalticPumpController(
      uint8_t peristalticPumpControlPin,
      bool ramp,
      float maxFlowRateMlMin);

  /*******************************
   * Getters / Setters
   *******************************/
  // True when the pump is running
  bool isPumpOn() const;
  // Get the current pump speed as a percentage of the maximum speed
  float getPumpSpeedPercentage() const;
  // Get the theoretical current pumping volume / ml per minute
  float getPumpFlowRate() const;
  // Get the current pump target mode - useful for determining whether the pump has reached the end an operation
  PPC::PumpTargetMode getPumpTargetMode() const;

  /*******************************
   * Actions
   *******************************/
  // Control loop to be called in the main loop
  void controlLoop();
  // Set the pump to run at a specific speed (percentage of maximum speed)
  void setTargetPumpSpeed(float targetSpeedPercentage);
  // Set the pump to run at a specific flow rate
  void setTargetPumpFlowRate(float targetFlowRate);
  // Pump a specific volume of liquid as quickly as possible
  void pumpTargetVolume(float targetVolume);

private:

  /*******************************
   * Member variables
   *******************************/
  // The microcontroller PWM enabled pin that controls the pump
  uint8_t mPeristalticPumpControlPin;

  // True when the pump is on
  bool mPumpOn = false;
  // The current flow rate in ml/min
  float mFlowRate = 0.0f;
  // The current pump speed as a percentage of the maximum speed
  float mSpeedPercentage = 0.0f;

  // The target mode for the pump
  PPC::PumpTargetMode mPumpTargetMode = PPC::PumpTargetMode::None;

  // The target flow rate in ml/min
  float mTargetFlowRate = 0.0f;
  // The target pump speed as a percentage of the maximum speed
  float mTargetSpeedPercentage = 0.0f;
  // The target volume to pump in ml
  float mTargetVolume = 0.0f;

  // Ramping control
  bool mRampEnabled;
  // The time ramping started
  unsigned long mRampStartTime;
  // The speed at which the ramp started
  float mRampStartSpeed;

  // The start time for pumping a target volume
  unsigned long mVolumeStartTime;
  // The pumped volume so far in mL
  float mPumpedVolume;

  // Maximum flow rate in ml/min
  float mMaxFlowRateMlMin;

  /*******************************
   * Actions
   *******************************/
  // Immediately set the pump speed
  void setPumpSpeed(float speedPercentage);

};

#endif // __PERISTALTICPPUMPCONTROLLER_H_INCLUDED__
