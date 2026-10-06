/**
 * =====================================================================================
 *  HeadServo.h — 2-trục PAN/TILT controller cho 2 Servo SG90
 *  - Cập nhật 50Hz trên ServoTask (Core 1)
 *  - Speed limit + smoothstep easing
 *  - Clamp hard limits
 *  - NVS offset (calibration)
 *  - Auto-detach sau SERVO_IDLE_DETACH_MS
 *  - HOLD_TORQUE_TILT option
 * =====================================================================================
 */
#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

class HeadServo {
public:
  HeadServo();

  // Lifecycle
  void   begin();           // attach servo, load NVS offset, center từ từ
  void   update(float dt);  // gọi mỗi 20ms trong ServoTask

  // Commands (thread-safe: chỉ set target + timestamp, motion chạy trong update)
  void   setTarget(float panDeg, float tiltDeg);
  void   setTargetPan(float panDeg);
  void   setTargetTilt(float tiltDeg);
  void   nudge(float dpan, float dtilt);
  void   center();
  void   stop();            // freeze tại chỗ
  void   panic();           // tắt PWM ngay (detach)

  // Calibration (lưu NVS)
  void   saveCalibration(int8_t panOffset, int8_t tiltOffset);

  // Query (gọi từ main/Brain/Web)
  float  getCurrentPan()  const { return currentPan;  }
  float  getCurrentTilt() const { return currentTilt; }
  float  getTargetPan()   const { return targetPan;   }
  float  getTargetTilt()  const { return targetTilt;  }
  int8_t getOffsetPan()   const { return offsetPan;   }
  int8_t getOffsetTilt()  const { return offsetTilt;  }
  bool   isMoving()       const;
  bool   isAttached()     const { return attached; }
  bool   holdTorqueTilt() const { return _holdTilt; }
  void   setHoldTorqueTilt(bool en) { _holdTilt = en; }

private:
  Servo  panServo, tiltServo;
  float  currentPan, currentTilt;
  float  targetPan,  targetTilt;
  float  velPan,     velTilt;          // cho easing
  int8_t offsetPan,  offsetTilt;
  bool   attached;
  bool   _holdTilt;                    // giữ PWM TILT khi idle
  unsigned long lastMoveMs;
  unsigned long lastUpdateMs;

  void   attachHardware();
  void   detachHardware();
  void   writeImmediate(float pan, float tilt);
  void   loadCalibration();
  float  smoothstep(float t);
  float  clampPan(float d)  const;
  float  clampTilt(float d) const;
};
