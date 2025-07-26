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

    if (mRampEnabled) {
      // Calculate volume pumped during ramp up/down at full speed
      // For constant acceleration, ramp time = 100 / RAMP_SPEED_PERCENTAGE_PER_MS
      float rampTime = 100.0f / RAMP_SPEED_PERCENTAGE_PER_MS;
      float rampVolume = (rampTime * mMaxFlowRateMlMin) / (2.0f * 60.0f * 1000.0f); // ml per ramp
      float totalRampVolume = 2 * rampVolume; // Both up and down ramps
      
      if (targetVolume <= totalRampVolume) {
        // For small volumes, calculate required speed to achieve target volume
        // targetVolume = (rampTime * maxFlowRate * speedPercent) / (60000)
        float speedPercent = (targetVolume * 60000.0f) / (rampTime * mMaxFlowRateMlMin);
        setTargetPumpSpeedInternal(speedPercent);
      } else {
        // Normal volume - start at full speed
        setTargetPumpSpeedInternal(100.0f);
      }
    } else {
      // No ramping - start at full speed
      setTargetPumpSpeedInternal(100.0f);
    }
  }
}

void PeristalticPumpController::controlLoop() {
  unsigned long currentTime = millis();
  float lastIterationFlowRate = mFlowRate;

  // Handle speed ramping
  if (mRamping) {
    unsigned long elapsedTime = PPC::safeTimeDifference(mRampStartTime, currentTime);
    float speedDelta = RAMP_SPEED_PERCENTAGE_PER_MS * elapsedTime;
    
    // Calculate new speed based on direction of ramp
    float newSpeed;
    if (mTargetSpeedPercentage > mRampStartSpeed) {
      // Ramping up
      newSpeed = mRampStartSpeed + speedDelta;
      if (newSpeed >= mTargetSpeedPercentage) {
        newSpeed = mTargetSpeedPercentage;
        mRamping = false;
      }
    } else {
      // Ramping down
      newSpeed = mRampStartSpeed - speedDelta;
      if (newSpeed <= mTargetSpeedPercentage) {
        newSpeed = mTargetSpeedPercentage;
        mRamping = false;
      }
    }
    
    setPumpSpeed(newSpeed);
    
    // Clear mode when appropriate
    if (!mRamping && (mPumpTargetMode != PumpTargetMode::Volume || mTargetSpeedPercentage == 0.0f)) {
      mPumpTargetMode = PumpTargetMode::None;
    }
  }

  // Handle volume-based pumping
  if (mPumpTargetMode == PumpTargetMode::Volume) {
    unsigned long pumpTime = PPC::safeTimeDifference(mVolumeLastCalcTime, currentTime);
    mPumpedVolume += lastIterationFlowRate * (pumpTime / (60.0f * 1000.0f));
    mVolumeLastCalcTime = currentTime;

    if (mRampEnabled && !mRamping) {
      // Calculate volume that will be pumped during ramp down
      // For constant acceleration, ramp down time = current_speed / RAMP_SPEED_PERCENTAGE_PER_MS
      float rampDownTime = mSpeedPercentage / RAMP_SPEED_PERCENTAGE_PER_MS;
      float rampDownVolume = (mFlowRate * rampDownTime) / (2.0f * 60.0f * 1000.0f);
      
      if (mPumpedVolume + rampDownVolume >= mTargetVolume) {
        setTargetPumpSpeedInternal(0);
      }
    } else if (!mRampEnabled) {
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