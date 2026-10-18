#pragma once

bool hayVoz() {
    return audioListo || (btConectado() && btStream);
}

void salidaVoz(const uint8_t* data, size_t size) {
    if (!usandoBT && !audioListo) iniciarAudio();
    salidaAudio(data, (int)size);
    serviceStandalone();
}

bool vozNavegador = false;      // la consulta actual pidió la voz en el navegador
bool atendiendoPeticionLarga = false;    // un handler largo (voz, escaneo) está activo: no reentrar en handleClient()
bool vozWebCabeceras = false;

// Atiende peticiones y anima la cara entre bloques descargados de Gemini.
void serviceStandalone() {
    static bool servicing = false;
    static unsigned long lastWeb = 0;
    if (servicing) return;
    servicing = true;
    actualizarPantalla();
    if (!atendiendoPeticionLarga && millis() - lastWeb >= 25) {
        lastWeb = millis();
        localServer.handleClient();
    }
    servicing = false;
    delay(0);
}

void consultarGimena(const String& message) {
    chatRunning = true;
    bool wantsVoice = !vozNavegador && hayVoz();   // en modo navegador la voz la pide la página
    liberarI2S();  // reserva espacio para el pico de memoria del handshake TLS
    renderOLED("thinking", "Consultando Gemini directamente");
    String states = String("luz_sala (LED GPIO 2): ") + (estadoLuzSala ? "ON" : "OFF") +
        "; luz_cocina (salida GPIO 4): " + (estadoLuzCocina ? "ON" : "OFF") +
        "; ventilador (salida GPIO 5): " + (estadoVentilador ? "ON" : "OFF");
    String red;
    if (WiFi.status() == WL_CONNECTED && preguntaPorRed(message)) {
        asegurarEscaneoRed();   // ~4 s la primera vez; luego se reutiliza unos minutos
        red = resumenRed();
    }
    JsonDocument response;
    if (WiFi.status() != WL_CONNECTED) jobError = "El ESP32 no tiene conexion WiFi.";
    else if (geminiReply(message, states, red, response, jobError)) {
        jobReply = response["reply"].as<String>();
        ultimaRespuesta = jobReply;
        caraActual = response["face"] | "neutral";
        if (caraActual != "neutral" && caraActual != "happy" && caraActual != "love" &&
            caraActual != "surprised" && caraActual != "sad" && caraActual != "thinking" && caraActual != "sleepy") caraActual = "neutral";
        for (JsonObject action : response["device_actions"].as<JsonArray>()) {
            if (!action["state"].is<bool>()) continue;
            String id = action["device"] | "";
            bool state = action["state"];
            if (id == "luz_sala") estadoLuzSala = state;
            else if (id == "luz_cocina") estadoLuzCocina = state;
            else if (id == "ventilador") estadoVentilador = state;
        }
        actualizarHardware();
        renderOLED(caraActual, ultimaRespuesta);
        Serial.printf("[Gimena directa] %s\n", ultimaRespuesta.c_str());
        voicePending = wantsVoice;
        voiceReadyAt = millis() + 700;
    }
    if (jobError.length()) {
        if (!btConectado()) restaurarI2S();
        renderOLED("sad", jobError);
        Serial.printf("[Gemini] %s\n", jobError.c_str());
    }
    chatRunning = false;
}

void hablarDirectamente() {
    voicePending = false;
    usandoBT = btConectado() && btStream;
    audioReproduciendo = true;
    uint32_t rate = usandoBT ? 44100 : AUDIO_SAMPLE_RATE;
    geminiSpeech(jobReply, rate, salidaVoz, voiceError, AUDIO_VOLUMEN);
    if (usandoBT) {
        unsigned long start = millis();
        while (btStream && xStreamBufferBytesAvailable(btStream) && btConectado() && millis() - start < 2000) {
            serviceStandalone();
            delay(5);
        }
    } else silencioI2S(AUDIO_SAMPLE_RATE / 10);
    audioReproduciendo = false;
    if (voiceError.length()) Serial.printf("[Voz] %s\n", voiceError.c_str());
    if (!btConectado()) restaurarI2S();
}

bool chatOcupado() {
    return chatPending || chatRunning || voicePending || audioReproduciendo;
}

bool encolarChat(const String& message, bool voz_navegador = false) {
    if (chatOcupado()) return false;
    vozNavegador = voz_navegador;
    pendingMessage = message;
    jobReply = jobError = voiceError = "";
    chatJob++;
    chatPending = true;
    if (btEscaneando && !btObjetivoValido && !btConectado()) {
        btEscaneando = false;
        a2dp.cancel_discovery();
    }
    return true;
}
