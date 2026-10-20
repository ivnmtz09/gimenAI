#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

bool geminiReply(const String& message, const String& devices, const String& network, JsonDocument& reply, String& error);
bool geminiSpeech(const String& text, uint32_t rate, void (*output)(const uint8_t*, size_t), String& error, int volume);
// Voz para la respuesta: Google Cloud TTS si hay GOOGLE_TTS_API_KEY; si no, Gemini TTS.
bool sintetizarVoz(const String& text, uint32_t rate, void (*output)(const uint8_t*, size_t), String& error, int volume);
void clearGeminiMemory();
// Mantiene abiertas las conexiones HTTPS con Gemini y Cloud TTS (llamar desde loop() en reposo).
void precalentarConexiones();
// Cierra conexiones persistentes si no queda un bloque contiguo de ese tamaño.
void liberarMemoriaTls(size_t bloqueNecesario);
void serviceStandalone();
