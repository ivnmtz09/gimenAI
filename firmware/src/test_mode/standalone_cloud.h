#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

bool geminiReply(const String& message, const String& devices, JsonDocument& reply, String& error);
bool geminiSpeech(const String& text, uint32_t rate, void (*output)(const uint8_t*, size_t), String& error);
void clearGeminiMemory();
void serviceStandalone();
