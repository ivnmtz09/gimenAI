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
    if (btObjetivoValido || !encolarChat(message)) {
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
