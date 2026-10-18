#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

bool geminiReply(const String& message, const String& devices, const String& network, JsonDocument& reply, String& error);
bool geminiSpeech(const String& text, uint32_t rate, void (*output)(const uint8_t*, size_t), String& error, int volume);
void clearGeminiMemory();
void serviceStandalone();
