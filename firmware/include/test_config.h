#pragma once
#include <Arduino.h>

// ==============================================================================
// GimenAI - Modo de Test (ESP32 DevKit V1 Type-C + OLED GM009605 + Smart Home)
// ==============================================================================

// ------------------------------------------------------------------------------
// Configuración de Red WiFi
// ------------------------------------------------------------------------------
// Configuración local opcional (secrets.h está excluido de Git).
#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef TEST_WIFI_SSID
#define TEST_WIFI_SSID       "A54"
#endif
#ifndef TEST_WIFI_PASSWORD
#define TEST_WIFI_PASSWORD   "12345678"
#endif

// Red propia (abierta) que crea Gimena cuando no logra conectarse (portal de configuración).
// Nombre corto: el QR de la OLED debe ser versión 1 (máx. 17 bytes) para verse grande.
#define PORTAL_AP_SSID       "Gimena"

// ------------------------------------------------------------------------------
// Gemini directo desde el ESP32. Define la clave real en secrets.h.
// ------------------------------------------------------------------------------
#ifndef GEMINI_API_KEY
#define GEMINI_API_KEY       "TU_GOOGLE_GEMINI_API_KEY"
#endif

#ifndef GEMINI_MODEL
#define GEMINI_MODEL         "gemini-3.5-flash-lite"
#endif
#define GEMINI_TTS_MODEL     "gemini-2.5-flash-preview-tts"
#define GEMINI_TTS_VOICE     "Kore"

// ------------------------------------------------------------------------------
// Voz rápida: Google Cloud Text-to-Speech (clave de Google Cloud "AIza...", con la
// API Text-to-Speech activada). Define GOOGLE_TTS_API_KEY en secrets.h; sin ella
// se usa Gemini TTS, que es más lento.
// ------------------------------------------------------------------------------
#ifndef GOOGLE_TTS_API_KEY
#define GOOGLE_TTS_API_KEY   ""
#endif
#ifndef GOOGLE_TTS_LANGUAGE
#define GOOGLE_TTS_LANGUAGE  "es-US"
#endif
#ifndef GOOGLE_TTS_VOICE
#define GOOGLE_TTS_VOICE     "es-US-Neural2-A"   // femenina, español latinoamericano
#endif
#define GOOGLE_TTS_RATE      16000   // µ-law 8 bit: ~21 KB/s en base64, por debajo del límite de la placa
#define GOOGLE_TTS_SPEED     1.05

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

// ------------------------------------------------------------------------------
// Parlante: amplificador I2S MAX98357A (voz de Gimena)
// ------------------------------------------------------------------------------
#define AUDIO_PIN_BCLK       26   // MAX98357A BCLK
#define AUDIO_PIN_LRC        25   // MAX98357A LRC (WS)
#define AUDIO_PIN_DIN        27   // MAX98357A DIN
#define AUDIO_SAMPLE_RATE    16000  // salida I2S; Gemini TTS se remuestrea desde 24 kHz
#define AUDIO_VOLUMEN        60   // 0-100 (si suena saturado o con ruido, bájalo)
