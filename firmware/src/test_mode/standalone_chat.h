#pragma once

bool hayVoz() {
    return audioListo;
}

void salidaVoz(const uint8_t* data, size_t size) {
    if (!audioListo) iniciarAudio();
    salidaAudio(data, (int)size);
    serviceStandalone();
}

bool vozNavegador = false;      // la consulta actual pidió la voz en el navegador
bool atendiendoPeticionLarga = false;    // un handler largo (voz, escaneo) está activo: no reentrar en handleClient()
bool vozWebCabeceras = false;
String jobAnimacion;            // animación de la última respuesta (para el panel)

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
    String minus = message; minus.toLowerCase();
    if (minus.indexOf("tengo sed") >= 0) {   // comando secreto: no pasa por Gemini
        jobReply = ultimaRespuesta = "Comando secreto activado. Un Old Parr doce anos con hielo. Salud, y con moderacion.";
        caraActual = "love";
        renderOLED(caraActual, ultimaRespuesta);
        iniciarAnimacion("whisky");
        jobAnimacion = "whisky";
        Serial.println("[Gimena directa] (whisky) comando secreto");
        voicePending = wantsVoice;
        voiceReadyAt = millis() + 150;   // deja al panel leer la respuesta antes de hablar
        chatRunning = false;
        return;
    }
    liberarI2S();  // reserva espacio para el pico de memoria del handshake TLS
    renderOLED("thinking", "Consultando Gemini directamente");
    String states = String("luz_sala (LED GPIO 2): ") + (estadoLuzSala ? "ON" : "OFF") +
        "; luz_cocina (salida GPIO 4): " + (estadoLuzCocina ? "ON" : "OFF") +
        "; ventilador (salida GPIO 5): " + (estadoVentilador ? "ON" : "OFF");
    if (WiFi.status() == WL_CONNECTED && tuyaConfigurado()) {
        String errorTuya;   // sin Tuya Gimena sigue respondiendo con las salidas GPIO
        if (!tuyaActualizar(errorTuya, 600000)) Serial.printf("[Tuya] %s\n", errorTuya.c_str());
        if (enchufesTuya()) states += "; " + tuyaResumen();
    }
    String red;
    if (WiFi.status() == WL_CONNECTED && preguntaPorRed(message)) {
        iniciarAnimacion("radar", 15000);   // se corta al llegar la respuesta
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
        String animacion = response["animation"] | "";
        bool animacionDispositivo = false;
        for (JsonObject action : response["device_actions"].as<JsonArray>()) {
            if (!action["state"].is<bool>()) continue;
            String id = action["device"] | "";
            bool state = action["state"];
            if (!animacionDispositivo) {   // la primera acción define la animación
                String nombre;
                for (size_t i = 0; i < enchufesTuya(); i++)
                    if (enchufeTuya(i).alias == id) nombre = enchufeTuya(i).nombre;
                animacion = animacionDeDispositivo(id, state, nombre);
                animacionDispositivo = true;
            }
            if (id == "luz_sala") estadoLuzSala = state;
            else if (id == "luz_cocina") estadoLuzCocina = state;
            else if (id == "ventilador") estadoVentilador = state;
            else if (id.startsWith("tuya_")) {
                String errorTuya;
                if (!tuyaComando(id, state, errorTuya)) {
                    jobReply = "No pude cambiar ese enchufe. " + errorTuya;
                    ultimaRespuesta = jobReply;
                    caraActual = "sad";
                    animacion = "";
                }
            }
        }
        actualizarHardware();
        renderOLED(caraActual, ultimaRespuesta);
        iniciarAnimacion(animacion);   // ignora nombres desconocidos o vacíos
        jobAnimacion = animacionValida(animacion) ? animacion : "";
        Serial.printf("[Gimena directa] (%s) %s\n", jobAnimacion.c_str(), ultimaRespuesta.c_str());
        voicePending = wantsVoice;
        voiceReadyAt = millis() + 150;   // deja al panel leer la respuesta antes de hablar
    }
    if (jobError.length()) {
        restaurarI2S();
        renderOLED("sad", jobError);
        Serial.printf("[Gemini] %s\n", jobError.c_str());
    }
    chatRunning = false;
}

void hablarDirectamente() {
    voicePending = false;
    audioReproduciendo = true;
    sintetizarVoz(jobReply, AUDIO_SAMPLE_RATE, salidaVoz, voiceError, AUDIO_VOLUMEN);
    silencioI2S(AUDIO_SAMPLE_RATE / 10);
    audioReproduciendo = false;
    if (voiceError.length()) Serial.printf("[Voz] %s\n", voiceError.c_str());
    restaurarI2S();
}

bool chatOcupado() {
    return chatPending || chatRunning || voicePending || audioReproduciendo;
}

bool encolarChat(const String& message, bool voz_navegador = false) {
    if (chatOcupado()) return false;
    vozNavegador = voz_navegador;
    pendingMessage = message;
    jobReply = jobError = voiceError = jobAnimacion = "";
    chatJob++;
    chatPending = true;
    return true;
}
