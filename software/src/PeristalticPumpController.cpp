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

  setTargetPumpSpeedInternal(mTargetSpeedPercentage);
  // If we're not ramping, then we've now already hit the target speed
  if (!mRampEnabled) mPumpTargetMode = PumpTargetMode::None;
}

void PeristalticPumpController::setTargetPumpFlowRate(float targetFlowRate) {
  // Constrain flow rate between 0 and maximum
  mTargetFlowRate = constrain(targetFlowRate, 0.0f, mMaxFlowRateMlMin);
  mPumpTargetMode = PumpTargetMode::FlowRate;
  // Convert flow rate to speed percentage
  setTargetPumpSpeedInternal((mTargetFlowRate / mMaxFlowRateMlMin) * 100.0f);
  // If we're not ramping, then we've now already hit the target flow rate
  if (!mRampEnabled) mPumpTargetMode = PumpTargetMode::None;
}

void PeristalticPumpController::pumpTargetVolume(float targetVolume) {
  if (targetVolume > 0) {
    mTargetVolume = targetVolume;
    mVolumeLastCalcTime = millis();
    mPumpedVolume = 0;
    mPumpTargetMode = PumpTargetMode::Volume;
    // Start at full speed
    setTargetPumpSpeedInternal(100.0f);
  }
}

void PeristalticPumpController::controlLoop() {
  unsigned long currentTime = millis();
  float lastIterationFlowRate = mFlowRate;

  // Handle speed ramping
  if (mRamping) {
    unsigned long elapsedTime = safeTimeDifference(mRampStartTime, currentTime);
    if (elapsedTime < RAMP_TIME_MS) {
      // Still ramping
      float rampProgress = (float)(currentTime - mRampStartTime) / RAMP_TIME_MS;
      float speedPercentage =
          mRampStartSpeed +
          (mTargetSpeedPercentage - mRampStartSpeed) * rampProgress;
      setPumpSpeed(speedPercentage);
    } else {
      // Finished ramping
      setPumpSpeed(mTargetSpeedPercentage);
      mRamping = false;
      // Clear mode when the ramp was the target, or if we're in volume mode then once we've finished pumping only
      if (mPumpTargetMode != PumpTargetMode::Volume || mTargetSpeedPercentage == 0.0f) {
        mPumpTargetMode = PumpTargetMode::None;
      }
    }
  }

  // Handle volume-based pumping
  if (mPumpTargetMode == PumpTargetMode::Volume) {
    // Calculate additional pumped volume since last iteration based on time and flow rate and accumulate
    unsigned long pumpTime = safeTimeDifference(mVolumeLastCalcTime, currentTime);
    mPumpedVolume += lastIterationFlowRate * (pumpTime / (60.0f * 1000.0f));
    mVolumeLastCalcTime = currentTime; // Reset start time for next iteration

    if (mRampEnabled && !mRamping) {
      // Calculate volume that will be pumped during ramp down
      // Area under linear ramp = (current_flow_rate * ramp_time) / 2
      float rampDownVolume = (mFlowRate * RAMP_TIME_MS) / (2.0f * 60.0f * 1000.0f);
      
      // Start ramping down early to account for ramp down volume
      if (mPumpedVolume + rampDownVolume >= mTargetVolume) {
        setTargetPumpSpeedInternal(0);
      }
    } else if (!mRampEnabled) {
      // No ramping - stop immediately at target volume
      if (mPumpedVolume >= mTargetVolume) {
        setTargetPumpSpeedInternal(0);
        mPumpTargetMode = PumpTargetMode::None;
      }
    }
  }
}

void PeristalticPumpController::setPumpSpeed(float speedPercentage) {
  // Immediately set the pump speed
  mSpeedPercentage = speedPercentage;
  // Update flow rate based on current speed
  mFlowRate = (mSpeedPercentage / 100.0f) * mMaxFlowRateMlMin;
  // Make it true!
  analogWrite(mPeristalticPumpControlPin,
              map(mSpeedPercentage, 0, 100, 0, 255));
  // If we're running at 0 speed then the pump is off, otherwise it's on
  mPumpOn = mSpeedPercentage > 0;
}

void PeristalticPumpController::setTargetPumpSpeedInternal(float targetSpeedPercentage) {
  // Don't start new ramp if already ramping to same target
  if (mRamping && targetSpeedPercentage == mTargetSpeedPercentage) {
    return;
  }

  mTargetSpeedPercentage = targetSpeedPercentage;
  if (!mRampEnabled) {
    // Go straight to the target speed without ramping
    setPumpSpeed(mTargetSpeedPercentage);
  } else {
    mRampStartTime = millis();
    mRampStartSpeed = mSpeedPercentage;
    mRamping = true;
  }
}
namespace PPC {

  unsigned long safeTimeDifference(unsigned long startTime, unsigned long endTime) {
    if (endTime >= startTime) {
      return endTime - startTime;
    } else {
      // Handle wrap-around case
      return (0xFFFFFFFF - startTime + endTime);
    }
  }

}