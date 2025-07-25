#include "PeristalticPumpController.hpp"

using namespace PPC;

/*******************************
 * Constructors
 *******************************/

PeristalticPumpController::PeristalticPumpController(
    uint8_t peristalticPumpControlPin, 
    bool ramp,
    float maxFlowRateMlMin)
      : mPeristalticPumpControlPin(peristalticPumpControlPin), 
        mRampEnabled(ramp),
        mRampStartTime(0), 
        mRampStartSpeed(0.0f),
        mMaxFlowRateMlMin(maxFlowRateMlMin) {
  pinMode(mPeristalticPumpControlPin, OUTPUT);
  analogWrite(mPeristalticPumpControlPin, 0);
}

/*******************************
 * Getters / Setters
 *******************************/

bool PeristalticPumpController::isPumpOn() const { 
  return mPumpOn;
}

float PeristalticPumpController::getPumpSpeedPercentage() const {
  return mSpeedPercentage;
}

float PeristalticPumpController::getPumpFlowRate() const {
  return mFlowRate;
}

// Get the current pump target mode - useful for determining whether the pump has reached the end an operation
PPC::PumpTargetMode PeristalticPumpController::getPumpTargetMode() const {
  return mPumpTargetMode;
}

/*******************************
 * Actions
 *******************************/

void PeristalticPumpController::setTargetPumpSpeed(
    float targetSpeedPercentage) {
  // Constrain the target speed between 0 and 100%
  mTargetSpeedPercentage = constrain(targetSpeedPercentage, 0.0f, 100.0f);
  mPumpTargetMode = PumpTargetMode::Speed;

  if (!mRampEnabled) {
    setPumpSpeed(mTargetSpeedPercentage);
  } else {
    mRampStartTime = millis();
    mRampStartSpeed = mSpeedPercentage;
  }

  mPumpOn = mTargetSpeedPercentage > 0;
}

void PeristalticPumpController::setTargetPumpFlowRate(float targetFlowRate) {
  // Constrain flow rate between 0 and maximum
  mTargetFlowRate = constrain(targetFlowRate, 0.0f, mMaxFlowRateMlMin);
  mPumpTargetMode = PumpTargetMode::FlowRate;
  // Convert flow rate to speed percentage
  setTargetPumpSpeed((mTargetFlowRate / mMaxFlowRateMlMin) * 100.0f);
}

void PeristalticPumpController::pumpTargetVolume(float targetVolume) {
  if (targetVolume > 0) {
    mTargetVolume = targetVolume;
    mVolumeStartTime = millis();
    mPumpedVolume = 0;
    mPumpTargetMode = PumpTargetMode::Volume;
    // Start at full speed
    setTargetPumpSpeed(100.0f);
  }
}

void PeristalticPumpController::controlLoop() {
  unsigned long currentTime = millis();

  // Handle speed ramping
  if (mRampEnabled && mPumpTargetMode != PumpTargetMode::None) {
    if (currentTime - mRampStartTime < RAMP_TIME_MS) {
      // Still ramping
      float rampProgress = (float)(currentTime - mRampStartTime) / RAMP_TIME_MS;
      float speedPercentage =
          mRampStartSpeed +
          (mTargetSpeedPercentage - mRampStartSpeed) * rampProgress;
      setPumpSpeed(speedPercentage);
    } else {
      // Finished ramping
      setPumpSpeed(mTargetSpeedPercentage);
      if (mPumpTargetMode == PumpTargetMode::Volume) {
        // If we're looking to pump a volume, we only stop after we've finished ramping down
        if (mTargetSpeedPercentage == 0.0f) mPumpTargetMode = PumpTargetMode::None;
      } else {
        // In all other modes, we're done once we've ramped
        mPumpTargetMode = PumpTargetMode::None;
      }
    }
  }

  // Update flow rate based on current speed
  mFlowRate = (mSpeedPercentage / 100.0f) * mMaxFlowRateMlMin;

  // Handle volume-based pumping
  if (mPumpTargetMode == PumpTargetMode::Volume) {
    // Calculate pumped volume based on time and flow rate
    unsigned long pumpTime = currentTime - mVolumeStartTime;
    mPumpedVolume = (mFlowRate * pumpTime) / (60.0f * 1000.0f); // Convert to ml

    if (mRampEnabled) {
      // Calculate volume that will be pumped during ramp down
      // Area under linear ramp = (current_flow_rate * ramp_time) / 2
      float rampDownVolume = (mFlowRate * RAMP_TIME_MS) / (2.0f * 60.0f * 1000.0f);
      
      // Start ramping down early to account for ramp down volume
      if (mPumpedVolume + rampDownVolume >= mTargetVolume) {
        setTargetPumpSpeed(0);
      }
    } else {
      // No ramping - stop immediately at target volume
      if (mPumpedVolume >= mTargetVolume) {
        setTargetPumpSpeed(0);
      }
    }
  }
}

void PeristalticPumpController::setPumpSpeed(float speedPercentage) {
  // Immediately set the pump speed
  mSpeedPercentage = speedPercentage;
  analogWrite(mPeristalticPumpControlPin,
              map(mSpeedPercentage, 0, 100, 0, 255));
}