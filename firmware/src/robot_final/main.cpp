#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "config.h"
#include "expressions.h"
#include "display_controller.h"
#include "servo_controller.h"
#include "audio_controller.h"

// Instancias de periféricos
DisplayController display;
ServoController servos;
AudioController audio;

RobotState currentState = STATE_IDLE;
unsigned long stateTimer = 0;

void connectWiFi() {
    Serial.printf("Conectando a WiFi: %s ...\n", WIFI_SSID);
    display.showMessage("Conectando WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi conectado! IP: %s\n", WiFi.localIP().toString().c_str());
        display.showMessage("WiFi Conectado!");
        delay(1000);
        display.setExpression(EXPR_HAPPY);
        servos.performGesture("greet");
        delay(1200);
        servos.moveToDefault();
        display.setExpression(EXPR_NEUTRAL);
    } else {
        Serial.println("\nNo se pudo conectar a WiFi. Operando en modo local.");
        display.showMessage("Error WiFi: Modo Offline");
        delay(1500);
        display.setExpression(EXPR_SAD);
    }
}

// Envía el audio grabado al servidor backend (FastAPI / Gemini Bridge)
bool sendAudioAndProcessResponse() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi no conectado. No se puede procesar audio.");
        return false;
    }

    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + SERVER_AUDIO_PATH;
    Serial.printf("Enviando audio a %s (%d bytes)...\n", url.c_str(), audio.getRecordedBytes());

    http.begin(url);
    http.addHeader("Content-Type", "audio/wav");
    http.setTimeout(10000); // 10 segundos timeout

    int httpCode = http.POST(audio.getBuffer(), audio.getRecordedBytes());

    if (httpCode == HTTP_CODE_OK) {
        // Obtenemos respuesta JSON con texto, emoción y gesto
        String payload = http.getString();
        Serial.printf("Respuesta recibida: %s\n", payload.c_str());

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            const char* emotion = doc["emotion"] | "happy";
            const char* gesture = doc["gesture"] | "nod";
            const char* replyText = doc["text"] | "¡Hola!";

            Serial.printf("Gimena dice: %s (Emoción: %s, Gesto: %s)\n", replyText, emotion, gesture);

            // Cambiar expresión
            if (strcmp(emotion, "happy") == 0) display.setExpression(EXPR_HAPPY);
            else if (strcmp(emotion, "surprised") == 0) display.setExpression(EXPR_SURPRISED);
            else if (strcmp(emotion, "sad") == 0) display.setExpression(EXPR_SAD);
            else display.setExpression(EXPR_TALKING);

            // Ejecutar movimiento
            servos.performGesture(gesture);

            // Simular respuesta por audio o reproducir síntesis si el servidor la envía
            audio.playBeep(880, 150);
            delay(100);
            audio.playBeep(1200, 200);

            http.end();
            return true;
        } else {
            Serial.println("Error al deserializar JSON de respuesta.");
        }
    } else {
        Serial.printf("Error HTTP: %d\n", httpCode);
    }

    http.end();
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- Iniciando GimenAI Robot ---");

    // Configuración de botón trigger (BOOT en GPIO 0)
    pinMode(PIN_TRIGGER_BTN, INPUT_PULLUP);

    // Inicializar periféricos
    servos.begin();
    display.begin();

    if (!audio.begin()) {
        Serial.println("Fallo crítico en subsistema de audio I2S.");
        display.showMessage("Error: Audio I2S");
    }

    // Conectar WiFi
    connectWiFi();
    currentState = STATE_IDLE;
}

void loop() {
    // 1. Actualizaciones de animación y servos no bloqueantes
    display.updateAnimation();
    servos.update();

    // 2. Máquina de Estados Finitos (FSM)
    switch (currentState) {
        case STATE_IDLE: {
            // Activar por botón BOOT (GPIO 0 presionado en LOW)
            if (digitalRead(PIN_TRIGGER_BTN) == LOW) {
                delay(50); // Antirrebote
                if (digitalRead(PIN_TRIGGER_BTN) == LOW) {
                    Serial.println("\n[Trigger] Activado. Pasando a STATE_LISTENING...");
                    currentState = STATE_LISTENING;
                    display.setExpression(EXPR_LISTENING);
                    servos.performGesture("think");
                    audio.playBeep(1000, 100); // Beep de inicio de grabación
                    stateTimer = millis();
                }
            }
            break;
        }

        case STATE_LISTENING: {
            Serial.printf("Grabando audio (%d segundos)...\n", AUDIO_RECORD_SECONDS);
            size_t bytes = audio.recordWavSample(AUDIO_RECORD_SECONDS * 1000);
            Serial.printf("Grabación completada: %d bytes.\n", bytes);

            audio.playBeep(600, 80); // Beep de fin de grabación
            currentState = STATE_PROCESSING;
            display.setExpression(EXPR_THINKING);
            break;
        }

        case STATE_PROCESSING: {
            Serial.println("Procesando con Gimena (Google Gemini)...");
            bool success = sendAudioAndProcessResponse();

            if (success) {
                currentState = STATE_RESPONDING;
                stateTimer = millis();
            } else {
                display.setExpression(EXPR_SAD);
                audio.playBeep(300, 300);
                delay(1500);
                servos.moveToDefault();
                display.setExpression(EXPR_NEUTRAL);
                currentState = STATE_IDLE;
            }
            break;
        }

        case STATE_RESPONDING: {
            // Mantener estado de habla y gestos durante unos segundos
            if (millis() - stateTimer > 3500) {
                Serial.println("Fin de respuesta. Volviendo a STATE_IDLE.");
                servos.moveToDefault();
                display.setExpression(EXPR_NEUTRAL);
                currentState = STATE_IDLE;
            }
            break;
        }

        case STATE_ERROR:
        default:
            delay(100);
            break;
    }
}
