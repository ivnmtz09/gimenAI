#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Extrae PCM base64 de inlineData.data sin guardar el JSON ni el audio completos.
// Entrada: PCM mono little endian 24 kHz de Gemini TTS. Salida: 16 o 44,1 kHz.
class AudioStreamDecoder {
public:
    using Output = void (*)(const uint8_t*, size_t);
    AudioStreamDecoder(uint32_t outputRate, int volume, Output output)
        : rate(outputRate), gain(volume), emit(output) {}

    void write(uint8_t c) {
        if (inString) {
            if (escaped) { escaped = false; consume(c); return; }
            if (c == '\\') { escaped = true; return; }
            if (c == '"') {
                inString = false;
                if (audioString) { audioString = false; bits = 0; word = 0; }
                else { keyReady = tokenSize == 4 && memcmp(token, "data", 4) == 0; }
                return;
            }
            consume(c);
            return;
        }
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') return;
        if (c == ':' && keyReady) { valueReady = true; keyReady = false; return; }
        if (c == '"') {
            inString = true;
            audioString = valueReady;
            valueReady = false;
            keyReady = false;
            tokenSize = 0;
            return;
        }
        keyReady = valueReady = false;
    }

    void finish() { flush(); }
    bool valid() const { return samples > 0 && !bad && !audioString && !haveLowByte; }
    uint32_t sampleCount() const { return samples; }

private:
    uint32_t rate;
    int gain;
    Output emit;
    bool inString = false, escaped = false, audioString = false;
    bool keyReady = false, valueReady = false, bad = false, haveLowByte = false;
    char token[4];
    size_t tokenSize = 0;
    uint32_t word = 0, inputIndex = 0, samples = 0;
    int bits = 0;
    uint8_t lowByte = 0;
    int16_t previous = 0, buffer[256];
    size_t buffered = 0;
    uint64_t nextPosition = 0;

    void consume(uint8_t c) {
        if (!audioString) {
            if (tokenSize < sizeof(token)) token[tokenSize] = (char)c;
            tokenSize++;
            return;
        }
        if (c == '=') return;
        int v = c >= 'A' && c <= 'Z' ? c - 'A' :
                c >= 'a' && c <= 'z' ? c - 'a' + 26 :
                c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' ? 62 : c == '/' ? 63 : -1;
        if (v < 0) { bad = true; return; }
        word = (word << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            uint8_t b = (uint8_t)(word >> bits);
            if (!haveLowByte) { lowByte = b; haveLowByte = true; }
            else { sample((int16_t)((uint16_t)b << 8 | lowByte)); haveLowByte = false; }
        }
    }

    void sample(int16_t current) {
        if (inputIndex == 0) {
            output(current);
            nextPosition = 24000;
        } else {
            const uint64_t end = (uint64_t)inputIndex * rate;
            const uint64_t begin = (uint64_t)(inputIndex - 1) * rate;
            while (nextPosition <= end) {
                int64_t delta = (int64_t)(current - previous) * (int64_t)(nextPosition - begin);
                int32_t value = previous + delta / (int64_t)rate;
                output((int16_t)value);
                nextPosition += 24000;
            }
        }
        previous = current;
        inputIndex++;
    }

    void output(int16_t s) {
        buffer[buffered++] = (int16_t)((int32_t)s * gain / 100);
        samples++;
        if (buffered == sizeof(buffer) / sizeof(buffer[0])) flush();
    }

    void flush() {
        if (buffered && emit) emit((const uint8_t*)buffer, buffered * sizeof(int16_t));
        buffered = 0;
    }
};
