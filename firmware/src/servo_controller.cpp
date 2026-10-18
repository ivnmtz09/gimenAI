#include "servo_controller.h"

ServoController::ServoController() 
    : currentHeadPos(90), targetHeadPos(90),
      currentArmPos(10), targetArmPos(10),
      lastStepTime(0) {}

void ServoController::begin() {
    // Permitir asignación de temporizadores en ESP32
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    
    servoHead.setPeriodHertz(50); // Frecuencia estándar para servos analógicos (50Hz)
    servoArm.setPeriodHertz(50);
    
    // Rango típico de pulsos para MG90S: 500us a 2400us
    servoHead.attach(PIN_SERVO_HEAD, 500, 2400);
    servoArm.attach(PIN_SERVO_ARM, 500, 2400);

    servoHead.write(currentHeadPos);
    servoArm.write(currentArmPos);
}

void ServoController::setTargets(int headAngle, int armAngle) {
    targetHeadPos = constrain(headAngle, 30, 150);
    targetArmPos = constrain(armAngle, 0, 160);
}

void ServoController::moveToDefault() {
    setTargets(90, 10);
}

void ServoController::performGesture(const char* gesture) {
    if (strcmp(gesture, "greet") == 0) {
        // Saludo con el brazo
        setTargets(75, 120);
    } else if (strcmp(gesture, "nod") == 0) {
        // Asentimiento con la cabeza
        setTargets(110, 10);
    } else if (strcmp(gesture, "think") == 0) {
        // Inclinación curiosa de cabeza
        setTargets(65, 40);
    } else {
        moveToDefault();
    }
}

void ServoController::update() {
    unsigned long now = millis();
    // Actualizar paso de interpolación cada 15ms para movimiento suave
    if (now - lastStepTime >= 15) {
        lastStepTime = now;

        if (currentHeadPos < targetHeadPos) currentHeadPos++;
        else if (currentHeadPos > targetHeadPos) currentHeadPos--;
        servoHead.write(currentHeadPos);

        if (currentArmPos < targetArmPos) currentArmPos++;
        else if (currentArmPos > targetArmPos) currentArmPos--;
        servoArm.write(currentArmPos);
    }
}
