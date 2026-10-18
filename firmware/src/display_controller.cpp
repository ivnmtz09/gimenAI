#include "display_controller.h"

// Colores personalizados
#define COLOR_BG        0x0000 // Negro
#define COLOR_CYAN_EYE  0x07FF // Cian brillante
#define COLOR_WHITE     0xFFFF
#define COLOR_PINK      0xF81F

DisplayController::DisplayController() : currentExpression(EXPR_NEUTRAL), lastBlinkTime(0), isBlinking(false) {}

void DisplayController::begin() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_WHITE, COLOR_BG);
    tft.setTextSize(2);
    
    // Pantalla de bienvenida
    tft.setCursor(60, 90);
    tft.print("GimenAI");
    tft.setCursor(40, 130);
    tft.setTextSize(1);
    tft.print("Asistente Robotico");
    delay(1500);
    
    setExpression(EXPR_NEUTRAL);
}

void DisplayController::drawEye(int cx, int cy, int r, uint16_t color) {
    tft.fillCircle(cx, cy, r, color);
    // Brillo blanco en la pupila
    tft.fillCircle(cx - (r / 3), cy - (r / 3), r / 4, COLOR_WHITE);
}

void DisplayController::drawHappyEye(int cx, int cy, int r, uint16_t color) {
    // Dibujar arco hacia arriba (expresión sonriente ^)
    for (int t = 0; t < 5; t++) {
        tft.drawCircle(cx, cy + 10, r - t, color);
    }
    // Borrar la mitad inferior para dejar solo el arco superior
    tft.fillRect(cx - r - 2, cy + 10, (r * 2) + 4, r + 5, COLOR_BG);
}

void DisplayController::drawSurprisedEye(int cx, int cy, int r, uint16_t color) {
    tft.fillCircle(cx, cy, r + 8, color);
    tft.fillCircle(cx, cy, r - 5, COLOR_BG);
    tft.fillCircle(cx, cy, r / 3, COLOR_WHITE);
}

void DisplayController::drawSadEye(int cx, int cy, int r, uint16_t color) {
    tft.fillCircle(cx, cy, r, color);
    // Ceja inclinada triste
    tft.fillTriangle(cx - r, cy - r, cx + r, cy - r, cx + (cx < 120 ? -r : r), cy, COLOR_BG);
}

void DisplayController::setExpression(Expression expr) {
    currentExpression = expr;
    tft.fillScreen(COLOR_BG);

    const int leftX = 70;
    const int rightX = 170;
    const int eyeY = 110;
    const int eyeRadius = 32;

    switch (expr) {
        case EXPR_NEUTRAL:
            drawEye(leftX, eyeY, eyeRadius, COLOR_CYAN_EYE);
            drawEye(rightX, eyeY, eyeRadius, COLOR_CYAN_EYE);
            break;

        case EXPR_HAPPY:
            drawHappyEye(leftX, eyeY, eyeRadius, COLOR_CYAN_EYE);
            drawHappyEye(rightX, eyeY, eyeRadius, COLOR_CYAN_EYE);
            // Sonrisa pequeña
            tft.drawCircle(120, 160, 20, COLOR_PINK);
            tft.fillRect(95, 135, 50, 25, COLOR_BG);
            break;

        case EXPR_LISTENING:
            // Ojos más brillantes y abiertos con indicadores
            drawEye(leftX, eyeY, eyeRadius + 4, 0x07E0); // Verde brillante
            drawEye(rightX, eyeY, eyeRadius + 4, 0x07E0);
            tft.setCursor(65, 190);
            tft.setTextColor(0x07E0, COLOR_BG);
            tft.setTextSize(1);
            tft.print("[ Escuchando... ]");
            break;

        case EXPR_THINKING:
            // Ojos mirando arriba a la derecha
            drawEye(leftX + 10, eyeY - 12, eyeRadius - 4, 0xFFE0); // Amarillo
            drawEye(rightX + 10, eyeY - 12, eyeRadius - 4, 0xFFE0);
            tft.fillCircle(195, 50, 4, 0xFFE0);
            tft.fillCircle(208, 40, 7, 0xFFE0);
            tft.fillCircle(224, 25, 10, 0xFFE0);
            break;

        case EXPR_TALKING:
            drawEye(leftX, eyeY, eyeRadius, COLOR_CYAN_EYE);
            drawEye(rightX, eyeY, eyeRadius, COLOR_CYAN_EYE);
            // Boca abierta animada
            tft.fillEllipse(120, 175, 22, 14, COLOR_PINK);
            break;

        case EXPR_SURPRISED:
            drawSurprisedEye(leftX, eyeY, eyeRadius, COLOR_CYAN_EYE);
            drawSurprisedEye(rightX, eyeY, eyeRadius, COLOR_CYAN_EYE);
            tft.drawCircle(120, 175, 12, COLOR_WHITE);
            break;

        case EXPR_SAD:
            drawSadEye(leftX, eyeY, eyeRadius, 0x001F); // Azul
            drawSadEye(rightX, eyeY, eyeRadius, 0x001F);
            break;

        case EXPR_SLEEPING:
            tft.fillRect(leftX - 25, eyeY, 50, 6, COLOR_WHITE);
            tft.fillRect(rightX - 25, eyeY, 50, 6, COLOR_WHITE);
            break;
    }
}

void DisplayController::updateAnimation() {
    unsigned long now = millis();

    // Pestañeo natural en modo neutro cada 4 segundos
    if (currentExpression == EXPR_NEUTRAL) {
        if (!isBlinking && (now - lastBlinkTime > 3800)) {
            // Cerrar ojos
            tft.fillScreen(COLOR_BG);
            tft.fillRect(45, 110, 50, 6, COLOR_CYAN_EYE);
            tft.fillRect(145, 110, 50, 6, COLOR_CYAN_EYE);
            isBlinking = true;
            lastBlinkTime = now;
        } else if (isBlinking && (now - lastBlinkTime > 160)) {
            // Abrir ojos de nuevo
            isBlinking = false;
            lastBlinkTime = now;
            setExpression(EXPR_NEUTRAL);
        }
    } else if (currentExpression == EXPR_TALKING) {
        // Animación de boca hablando (abrir / cerrar cada 200ms)
        static unsigned long lastMouth = 0;
        static bool mouthOpen = false;
        if (now - lastMouth > 200) {
            lastMouth = now;
            mouthOpen = !mouthOpen;
            if (mouthOpen) {
                tft.fillEllipse(120, 175, 22, 14, COLOR_PINK);
            } else {
                tft.fillRect(95, 160, 50, 30, COLOR_BG);
                tft.drawLine(100, 175, 140, 175, COLOR_PINK);
            }
        }
    }
}

void DisplayController::showMessage(const char* text) {
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_WHITE, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(10, 110);
    tft.println(text);
}
