#pragma once

void handleWebRoot() {
    localServer.sendHeader("Cache-Control", "no-store");
    localServer.send_P(200, "text/html; charset=utf-8", WEB_PANEL);
}

void handleDevices() {
    JsonDocument doc;
    const char* ids[] = {"luz_sala", "luz_cocina", "ventilador"};
    const char* names[] = {"LED integrado", "Salida cocina", "Salida ventilador"};
    const int pins[] = {PIN_DEVICE_LUZ_SALA, PIN_DEVICE_LUZ_COCINA, PIN_DEVICE_VENTILADOR};
    const bool states[] = {estadoLuzSala, estadoLuzCocina, estadoVentilador};
    for (int i = 0; i < 3; i++) {
        JsonObject d = doc["devices"][ids[i]].to<JsonObject>();
        d["name"] = names[i]; d["target"] = "esp32_hardware";
        d["pin"] = pins[i]; d["state"] = states[i];
    }
    doc["esp32_nodes"]["local"]["ip"] = WiFi.localIP().toString();
    doc["standalone"] = true;
    enviarJson(doc);
}

void handleToggle() {
    String id = localServer.pathArg(0);
    if (id == "luz_sala") estadoLuzSala = !estadoLuzSala;
    else if (id == "luz_cocina") estadoLuzCocina = !estadoLuzCocina;
    else if (id == "ventilador") estadoVentilador = !estadoVentilador;
    else { localServer.send(404, "application/json", "{\"error\":\"Salida desconocida\"}"); return; }
    actualizarHardware();
    localServer.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleChat() {
    JsonDocument body;
    if (localServer.arg("plain").length() > 1024 || deserializeJson(body, localServer.arg("plain")) || !body["message"].is<const char*>()) {
        localServer.send(400, "application/json", "{\"error\":\"Mensaje invalido\"}"); return;
    }
    String message = body["message"].as<String>(); message.trim();
    if (!message.length() || message.length() > 500) {
        localServer.send(400, "application/json", "{\"error\":\"Escribe entre 1 y 500 caracteres\"}"); return;
    }
    bool vozWeb = body["voice"] == "browser";
    if (btObjetivoValido || !encolarChat(message, vozWeb)) {
        localServer.send(409, "application/json", "{\"error\":\"Gimena esta ocupada; espera a que termine\"}"); return;
    }
    JsonDocument result; result["job"] = chatJob;
    enviarJson(result, 202);
}

void handleChatStatus() {
    if (localServer.hasArg("job") && localServer.arg("job").toInt() != chatJob) {
        localServer.send(409, "application/json", "{\"error\":\"La consulta ya no es la actual\"}"); return;
    }
    JsonDocument doc;
    doc["busy"] = chatOcupado(); doc["reply"] = jobReply; doc["face"] = caraActual;
    doc["error"] = jobError; doc["audio_error"] = voiceError;
    enviarJson(doc);
}

// Voz en el navegador: PCM 16 bit mono a 24 kHz enviado por bloques (chunked)
// a medida que llega de Gemini, sin guardar el audio completo en RAM.
// Se agrupa en bloques de 4 KB y sin Nagle: con trozos de 512 bytes la
// transferencia caía a ~9 KB/s (cada trozo esperaba el ACK retardado).
static uint8_t vozWebBuffer[4096];
static size_t vozWebLlenos = 0;

void vaciarVozWeb() {
    if (!vozWebLlenos) return;
    localServer.sendContent((const char*)vozWebBuffer, vozWebLlenos);
    vozWebLlenos = 0;
}

void salidaVozWeb(const uint8_t* data, size_t size) {
    if (!vozWebCabeceras) {
        vozWebCabeceras = true;
        localServer.client().setNoDelay(true);
        localServer.sendHeader("Cache-Control", "no-store");
        localServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
        localServer.send(200, "application/octet-stream", "");
    }
    while (size) {
        size_t n = min(size, sizeof(vozWebBuffer) - vozWebLlenos);
        memcpy(vozWebBuffer + vozWebLlenos, data, n);
        vozWebLlenos += n; data += n; size -= n;
        if (vozWebLlenos == sizeof(vozWebBuffer)) vaciarVozWeb();
    }
}

void hablarEnNavegador() {
    atendiendoPeticionLarga = true;
    vozWebCabeceras = false;
    vozWebLlenos = 0;
    audioReproduciendo = true;   // la boca de la OLED se mueve mientras se genera la voz
    String error;
    bool ok = geminiSpeech(jobReply, 24000, salidaVozWeb, error, 100);
    audioReproduciendo = false;
    atendiendoPeticionLarga = false;
    if (vozWebCabeceras) {
        vaciarVozWeb();
        localServer.sendContent("");   // fin de la respuesta por bloques
    } else {
        JsonDocument doc;
        doc["error"] = error.length() ? error : String("Gemini no devolvio audio.");
        enviarJson(doc, 502);
    }
    if (!ok) Serial.printf("[Voz web] %s\n", error.c_str());
}

void handleChatVoice() {
    if (localServer.arg("job").toInt() != chatJob || !jobReply.length() || !vozNavegador) {
        localServer.send(409, "application/json", "{\"error\":\"No hay una respuesta para leer\"}"); return;
    }
    if (chatOcupado()) {
        localServer.send(409, "application/json", "{\"error\":\"Gimena esta ocupada; espera a que termine\"}"); return;
    }
    hablarEnNavegador();
}

// Escaneo IoT bajo demanda (?nuevo=1 ignora la caché)
void handleNetworkScan() {
    if (chatOcupado()) { localServer.send(409, "application/json", "{\"error\":\"Espera a que termine la consulta\"}"); return; }
    atendiendoPeticionLarga = true;
    if (localServer.hasArg("nuevo")) escanearRed(); else asegurarEscaneoRed();
    atendiendoPeticionLarga = false;
    JsonDocument doc;
    doc["age_s"] = (millis() - redEscaneadoEn) / 1000;
    JsonArray lista = doc["devices"].to<JsonArray>();
    for (int i = 0; i < redCantidad; i++) {
        JsonObject d = lista.add<JsonObject>();
        d["ip"] = redDispositivos[i].ip.toString();
        d["name"] = redDispositivos[i].nombre;
        d["type"] = redDispositivos[i].tipo;
        d["manufacturer"] = redDispositivos[i].fabricante;
    }
    enviarJson(doc);
}

void handleClearHistory() {
    if (chatOcupado()) { localServer.send(409, "application/json", "{\"error\":\"Espera a que termine la consulta\"}"); return; }
    clearGeminiMemory();
    localServer.send(200, "application/json", "{\"status\":\"cleared\"}");
}

void handleNetworkCheck() {
    if (chatOcupado()) { localServer.send(409, "application/json", "{\"error\":\"Espera a que termine la consulta\"}"); return; }
    JsonDocument doc;
    IPAddress address;
    doc["dns"] = WiFi.dnsIP().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    bool resolved = WiFi.hostByName("generativelanguage.googleapis.com", address);
    doc["gemini_dns_ok"] = resolved;
    doc["gemini_ip"] = address.toString();
    WiFiClient probe;
    doc["gemini_tcp_ok"] = resolved && probe.connect(address, 443, 3000);
    probe.stop();
    doc["internet_tcp_ok"] = (bool)probe.connect(IPAddress(1, 1, 1, 1), 443, 3000);
    probe.stop();
    doc["time"] = (long)time(nullptr);
    doc["heap"] = ESP.getFreeHeap();
    doc["largest_block"] = ESP.getMaxAllocHeap();
    enviarJson(doc);
}
