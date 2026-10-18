#pragma once
#include <Arduino.h>

// ==============================================================================
// GimenAI - Modo de Test (ESP32 DevKit V1 Type-C + OLED GM009605 + Smart Home)
// ==============================================================================

// ------------------------------------------------------------------------------
// Configuración de Red WiFi y Servidor Local GimenAI
// ------------------------------------------------------------------------------
#define TEST_WIFI_SSID       "A54"
#define TEST_WIFI_PASSWORD   "12345678"

// IP de la computadora donde corre 'python3 main.py' (tu IP actual detectada: 10.82.50.223)
#define SERVER_HOST          "10.82.50.223"
#define SERVER_PORT          8000


// ------------------------------------------------------------------------------
// Clave API de Google Gemini (Opcional, el ESP32 usa el servidor Python)
// ------------------------------------------------------------------------------
#define GEMINI_API_KEY       "TU_GOOGLE_GEMINI_API_KEY"

// Modelo a usar (gemini-1.5-flash es rápido, económico y excelente para IoT)
#define GEMINI_MODEL         "gemini-1.5-flash"

// ------------------------------------------------------------------------------
// Pantalla OLED GM009605v4.3 (Controlador SSD1306 I2C 128x64)
// Pines I2C estándar del ESP32 DevKit V1
// ------------------------------------------------------------------------------
#define OLED_PIN_SDA         21
#define OLED_PIN_SCL         22
#define OLED_I2C_ADDR        0x3C   // Dirección habitual: 0x3C (o 0x3D)
#define SCREEN_WIDTH         128
#define SCREEN_HEIGHT        64

// ------------------------------------------------------------------------------
// Dispositivos IoT Simulados / Reales (Bombillos y Actuadores)
// ------------------------------------------------------------------------------
// Bombillo 1: LED integrado en la placa ESP32 DevKit V1 (generalmente GPIO 2)
#define PIN_DEVICE_LUZ_SALA     2   

// Bombillo 2: Puedes conectar un LED externo con resistencia de 220 o 330 ohm
#define PIN_DEVICE_LUZ_COCINA   4   

// Dispositivo 3: Enchufe / Ventilador / Relé
#define PIN_DEVICE_VENTILADOR   5   
