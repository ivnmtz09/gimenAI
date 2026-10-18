#pragma once
#include <Arduino.h>
#include "driver/i2s.h"
#include "config.h"

class AudioController {
private:
    bool isRecording;
    uint8_t* pcmBuffer;
    size_t bufferSize;
    size_t recordedBytes;

public:
    AudioController();
    ~AudioController();

    bool begin();
    void startRecording();
    size_t readAudioChunk(uint8_t* buffer, size_t maxLen);
    size_t recordWavSample(uint32_t durationMs);
    void playPcmChunk(const uint8_t* data, size_t len);
    void playBeep(int frequencyHz, int durationMs);

    uint8_t* getBuffer() { return pcmBuffer; }
    size_t getRecordedBytes() const { return recordedBytes; }
};
