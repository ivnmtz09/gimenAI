#include "audio_controller.h"

AudioController::AudioController() 
    : isRecording(false), pcmBuffer(nullptr), bufferSize(0), recordedBytes(0) {}

AudioController::~AudioController() {
    if (pcmBuffer != nullptr) {
        free(pcmBuffer);
    }
    i2s_driver_uninstall(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_1);
}

bool AudioController::begin() {
    // 1. Configuración de I2S0 para Entrada de Micrófono (INMP441)
    i2s_config_t i2s_mic_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t i2s_mic_pins = {
        .bck_io_num = PIN_I2S_MIC_SCK,
        .ws_io_num = PIN_I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_MIC_SD
    };

    esp_err_t err_mic = i2s_driver_install(I2S_NUM_0, &i2s_mic_config, 0, NULL);
    if (err_mic != ESP_OK) {
        Serial.printf("Error al iniciar I2S0 Micrófono: %d\n", err_mic);
        return false;
    }
    i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);

    // 2. Configuración de I2S1 para Salida de Parlante (MAX98357A)
    i2s_config_t i2s_spk_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t i2s_spk_pins = {
        .bck_io_num = PIN_I2S_SPK_BCLK,
        .ws_io_num = PIN_I2S_SPK_LRC,
        .data_out_num = PIN_I2S_SPK_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t err_spk = i2s_driver_install(I2S_NUM_1, &i2s_spk_config, 0, NULL);
    if (err_spk != ESP_OK) {
        Serial.printf("Error al iniciar I2S1 Parlante: %d\n", err_spk);
        return false;
    }
    i2s_set_pin(I2S_NUM_1, &i2s_spk_pins);

    // 3. Reservar buffer de audio en PSRAM si está disponible
    bufferSize = (AUDIO_SAMPLE_RATE * (AUDIO_BITS_PER_SAMPLE / 8) * AUDIO_RECORD_SECONDS) + 44; // +44 para header WAV
    if (psramFound()) {
        pcmBuffer = (uint8_t*)ps_malloc(bufferSize);
        Serial.printf("Buffer de audio reservado en PSRAM: %d bytes\n", bufferSize);
    } else {
        pcmBuffer = (uint8_t*)malloc(bufferSize);
        Serial.printf("Buffer de audio reservado en RAM interna: %d bytes\n", bufferSize);
    }

    if (!pcmBuffer) {
        Serial.println("Error: No se pudo reservar memoria para el buffer de audio.");
        return false;
    }

    return true;
}

// Escribe la cabecera estándar RIFF/WAV (44 bytes)
static void writeWavHeader(uint8_t* header, uint32_t wavDataSize, uint32_t sampleRate, uint16_t channels, uint16_t bitsPerSample) {
    uint32_t fileSize = wavDataSize + 36;
    uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
    uint16_t blockAlign = channels * (bitsPerSample / 8);

    memcpy(header, "RIFF", 4);
    header[4] = (uint8_t)(fileSize & 0xFF);
    header[5] = (uint8_t)((fileSize >> 8) & 0xFF);
    header[6] = (uint8_t)((fileSize >> 16) & 0xFF);
    header[7] = (uint8_t)((fileSize >> 24) & 0xFF);
    memcpy(header + 8, "WAVEfmt ", 8);
    header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0; // Tamaño Subchunk1
    header[20] = 1; header[21] = 0; // PCM = 1
    header[22] = (uint8_t)(channels & 0xFF);
    header[23] = (uint8_t)((channels >> 8) & 0xFF);
    header[24] = (uint8_t)(sampleRate & 0xFF);
    header[25] = (uint8_t)((sampleRate >> 8) & 0xFF);
    header[26] = (uint8_t)((sampleRate >> 16) & 0xFF);
    header[27] = (uint8_t)((sampleRate >> 24) & 0xFF);
    header[28] = (uint8_t)(byteRate & 0xFF);
    header[29] = (uint8_t)((byteRate >> 8) & 0xFF);
    header[30] = (uint8_t)((byteRate >> 16) & 0xFF);
    header[31] = (uint8_t)((byteRate >> 24) & 0xFF);
    header[32] = (uint8_t)(blockAlign & 0xFF);
    header[33] = (uint8_t)((blockAlign >> 8) & 0xFF);
    header[34] = (uint8_t)(bitsPerSample & 0xFF);
    header[35] = (uint8_t)((bitsPerSample >> 8) & 0xFF);
    memcpy(header + 36, "data", 4);
    header[40] = (uint8_t)(wavDataSize & 0xFF);
    header[41] = (uint8_t)((wavDataSize >> 8) & 0xFF);
    header[42] = (uint8_t)((wavDataSize >> 16) & 0xFF);
    header[43] = (uint8_t)((wavDataSize >> 24) & 0xFF);
}

size_t AudioController::recordWavSample(uint32_t durationMs) {
    if (!pcmBuffer) return 0;

    size_t targetBytes = (AUDIO_SAMPLE_RATE * (AUDIO_BITS_PER_SAMPLE / 8) * durationMs) / 1000;
    if (targetBytes > bufferSize - 44) {
        targetBytes = bufferSize - 44;
    }

    uint8_t* dataPtr = pcmBuffer + 44;
    size_t totalBytesRead = 0;
    size_t bytesRead = 0;

    while (totalBytesRead < targetBytes) {
        size_t toRead = targetBytes - totalBytesRead;
        if (toRead > 1024) toRead = 1024;

        i2s_read(I2S_NUM_0, dataPtr + totalBytesRead, toRead, &bytesRead, portMAX_DELAY);
        totalBytesRead += bytesRead;
    }

    // Escribir cabecera WAV al inicio
    writeWavHeader(pcmBuffer, totalBytesRead, AUDIO_SAMPLE_RATE, 1, AUDIO_BITS_PER_SAMPLE);
    recordedBytes = totalBytesRead + 44;

    return recordedBytes;
}

void AudioController::playPcmChunk(const uint8_t* data, size_t len) {
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_1, data, len, &bytesWritten, portMAX_DELAY);
}

void AudioController::playBeep(int frequencyHz, int durationMs) {
    // Generar tono sinusoidal simple
    int samples = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
    int16_t sampleBuffer[128];
    float phase = 0.0;
    float phaseIncrement = 2.0 * PI * frequencyHz / AUDIO_SAMPLE_RATE;

    int remaining = samples;
    while (remaining > 0) {
        int chunk = remaining > 64 ? 64 : remaining;
        for (int i = 0; i < chunk; i++) {
            int16_t val = (int16_t)(sin(phase) * 6000.0); // Amplitud moderada
            sampleBuffer[i * 2] = val;     // Canal izquierdo
            sampleBuffer[i * 2 + 1] = val; // Canal derecho
            phase += phaseIncrement;
            if (phase >= 2.0 * PI) phase -= 2.0 * PI;
        }
        size_t written = 0;
        i2s_write(I2S_NUM_1, sampleBuffer, chunk * 2 * sizeof(int16_t), &written, portMAX_DELAY);
        remaining -= chunk;
    }
}
