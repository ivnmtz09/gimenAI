#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Extrae el audio base64 de un campo JSON sin guardar el JSON ni el audio completos.
// Fuentes:
//   - Gemini TTS: campo "data", PCM 16 bit little endian a 24 kHz, sin cabecera.
//   - Cloud TTS:  campo "audioContent", WAV con muestras µ-law de 8 bit (cabecera RIFF).
// Salida: PCM 16 bit a la frecuencia pedida (16 / 24 / 44,1 kHz), con interpolación lineal.
class AudioStreamDecoder {
public:
    enum Formato { PCM16, MULAW_WAV };
    using Output = void (*)(const uint8_t*, size_t);

    AudioStreamDecoder(uint32_t outputRate, int volume, Output output,
                       const char* key = "data", uint32_t inputRate = 24000, Formato formato = PCM16)
        : rate(outputRate), inRate(inputRate), gain(volume), emit(output), fmt(formato) {
        keyLen = strlen(key);
        if (keyLen > sizeof(clave)) keyLen = sizeof(clave);
        memcpy(clave, key, keyLen);
        headerDone = (fmt == PCM16);
    }

    void write(uint8_t c) {
        if (inString) {
            if (escaped) { escaped = false; consume(c); return; }
            if (c == '\\') { escaped = true; return; }
            if (c == '"') {
                inString = false;
                if (audioString) { audioString = false; bits = 0; word = 0; }
                else { keyReady = tokenSize == keyLen && memcmp(token, clave, keyLen) == 0; }
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
    bool valid() const { return samples > 0 && !bad && !audioString && !haveLowByte && headerDone; }
    uint32_t sampleCount() const { return samples; }

private:
    uint32_t rate, inRate;
    int gain;
    Output emit;
    Formato fmt;
    char clave[16];
    size_t keyLen = 0;
    bool inString = false, escaped = false, audioString = false;
    bool keyReady = false, valueReady = false, bad = false, haveLowByte = false;
    char token[16];
    size_t tokenSize = 0;
    uint32_t word = 0, inputIndex = 0, samples = 0;
    int bits = 0;
    uint8_t lowByte = 0;
    int16_t previous = 0, buffer[256];
    size_t buffered = 0;
    uint64_t nextPosition = 0;
    // Cabecera WAV: se acumula hasta encontrar el bloque "data"
    bool headerDone = true;
    uint8_t header[160];
    size_t headerLen = 0;

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
            byteDecodificado((uint8_t)(word >> bits));
        }
    }

    void byteDecodificado(uint8_t b) {
        if (!headerDone) { cabecera(b); return; }
        if (fmt == MULAW_WAV) { sample(mulaw(b)); return; }
        if (!haveLowByte) { lowByte = b; haveLowByte = true; }
        else { sample((int16_t)((uint16_t)b << 8 | lowByte)); haveLowByte = false; }
    }

    // Recorre los bloques RIFF hasta "data"; lo que venga después ya son muestras
    void cabecera(uint8_t b) {
        if (headerLen >= sizeof(header)) { bad = true; headerDone = true; return; }
        header[headerLen++] = b;
        if (headerLen < 12) return;
        if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) { bad = true; headerDone = true; return; }
        size_t pos = 12;
        while (pos + 8 <= headerLen) {
            uint32_t tam = header[pos + 4] | header[pos + 5] << 8 | header[pos + 6] << 16 | (uint32_t)header[pos + 7] << 24;
            if (memcmp(header + pos, "data", 4) == 0) {
                headerDone = true;
                for (size_t i = pos + 8; i < headerLen; i++) byteDecodificado(header[i]);
                return;
            }
            pos += 8 + tam + (tam & 1);
        }
    }

    static int16_t mulaw(uint8_t u) {
        u = ~u;
        int t = ((u & 0x0F) << 3) + 0x84;
        t <<= (u & 0x70) >> 4;
        return (int16_t)((u & 0x80) ? (0x84 - t) : (t - 0x84));
    }

    void sample(int16_t current) {
        if (inputIndex == 0) {
            output(current);
            nextPosition = inRate;
        } else {
            const uint64_t end = (uint64_t)inputIndex * rate;
            const uint64_t begin = (uint64_t)(inputIndex - 1) * rate;
            while (nextPosition <= end) {
                int64_t delta = (int64_t)(current - previous) * (int64_t)(nextPosition - begin);
                int32_t value = previous + delta / (int64_t)rate;
                output((int16_t)value);
                nextPosition += inRate;
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
