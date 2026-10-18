#pragma once
#include <ESP32Servo.h>
#include "config.h"

class ServoController {
private:
    Servo servoHead;
    Servo servoArm;
    int currentHeadPos;
    int targetHeadPos;
    int currentArmPos;
    int targetArmPos;
    unsigned long lastStepTime;

public:
    ServoController();
    void begin();
    void setTargets(int headAngle, int armAngle);
    void moveToDefault();
    void performGesture(const char* gesture);
    void update();
};
