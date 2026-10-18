#pragma once
#include <Arduino.h>

// ==============================================================================
// GimenAI - Configuración General de Hardware y Conexiones
// ESP32-S3-DevKitC-1 N8R8 (Universidad de La Guajira)
// ==============================================================================

// ------------------------------------------------------------------------------
// Configuración de Red WiFi y Backend / Gemini API
// ------------------------------------------------------------------------------
#define WIFI_SSID           "A54"
#define WIFI_PASSWORD       "12345678"

// URL del servidor intermediario (Backend FastAPI de baja latencia) o Endpoint
// Ejemplo: "http://192.168.1.100:8000" si está en la red local
#define SERVER_HOST         "192.168.1.100"
#define SERVER_PORT         8000
#define SERVER_AUDIO_PATH   "/api/voice"

// Opcional: Google Gemini API Key si se usa consulta directa desde el ESP32
#define GEMINI_API_KEY      "TU_GOOGLE_GEMINI_API_KEY"

// ------------------------------------------------------------------------------
// Asignación de Pines GPIO (Evitar GPIO 33-37 reservados para PSRAM Octal)
// ------------------------------------------------------------------------------

// Micrófono Digital I2S (INMP441 - I2S0)
// Nota: Pin L/R del INMP441 a GND (canal izquierdo)
#define PIN_I2S_MIC_SCK     4   // BCLK
#define PIN_I2S_MIC_WS      5   // WS / LRCL
#define PIN_I2S_MIC_SD      6   // SD / DOUT

// Amplificador de Audio I2S (MAX98357A - I2S1)
// Nota: SD a 3V3, GAIN a GND
#define PIN_I2S_SPK_BCLK    7   // BCLK
#define PIN_I2S_SPK_LRC     8   // LRC
#define PIN_I2S_SPK_DIN     9   // DIN

// Pantalla TFT ST7789 1.54" 240x240 (SPI)
// Nota: BLK (Backlight) a 3V3
#define PIN_TFT_CS          10  // CS
#define PIN_TFT_MOSI        11  // MOSI / SDA
#define PIN_TFT_SCK         12  // SCK / SCL
#define PIN_TFT_DC          14  // DC
#define PIN_TFT_RST         21  // RST

// Servomotores MG90S (PWM)
// Alimentación obligatoria desde riel de 5V separado con GND común
#define PIN_SERVO_HEAD      15  // Servo de Cabeza (Pan/Tilt)
#define PIN_SERVO_ARM       16  // Servo de Brazo (Gesto)

// Botón de activación manual / Wakeup (Opcional, BOOT en GPIO 0)
#define PIN_TRIGGER_BTN     0

// ------------------------------------------------------------------------------
// Parámetros de Audio
// ------------------------------------------------------------------------------
#define AUDIO_SAMPLE_RATE   16000   // 16 kHz estándar para Gemini / Whisper
#define AUDIO_BITS_PER_SAMPLE 16    // 16 bits
#define AUDIO_RECORD_SECONDS 4      // Tiempo de escucha por comando (ajustable)
