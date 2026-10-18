#include "standalone_cloud.h"
#include "audio_stream_decoder.h"
#include "test_config.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");

namespace {
String historyUser[3], historyModel[3];
size_t historyCount = 0;

class BoundedJson : public Stream {
public:
    String text;
    bool overflow = false;
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* bytes, size_t size) override {
        if (text.length() + size > 8192) { overflow = true; return 0; }
        text.concat((const char*)bytes, size);
        serviceStandalone();
        return size;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};

class AudioJson : public Stream {
public:
    AudioStreamDecoder decoder;
    AudioJson(uint32_t rate, int volume, void (*output)(const uint8_t*, size_t)) : decoder(rate, volume, output) {}
    size_t write(uint8_t c) override { decoder.write(c); return 1; }
    size_t write(const uint8_t* bytes, size_t size) override {
        for (size_t i = 0; i < size; i++) decoder.write(bytes[i]);
        serviceStandalone();
        return size;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};

bool beginCloud(HTTPClient& http, WiFiClientSecure& tls, const char* model, String& error) {
    if (strlen(GEMINI_API_KEY) < 20 || String(GEMINI_API_KEY).startsWith("TU_")) {
        error = "Configura GEMINI_API_KEY en secrets.h y vuelve a cargar el firmware.";
        return false;
    }
    if (time(nullptr) < 1700000000) {
        error = "Esperando sincronizacion de hora para conectar con Gemini de forma segura.";
        return false;
    }
    IPAddress address;
    if (!WiFi.hostByName("generativelanguage.googleapis.com", address)) {
        error = "El ESP32 no pudo resolver el nombre de Gemini. Revisa el DNS de la red WiFi.";
        return false;
    }
    Serial.printf("[Cloud] DNS=%s hora=%ld heap=%u max=%u\n", address.toString().c_str(), (long)time(nullptr), ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    // El almacén del SDK valida cadenas RSA/ECDSA sin analizar todas las raíces en RAM.
    tls.setCACertBundle(x509_crt_bundle_start);
    tls.setHandshakeTimeout(15);
    String url = String("https://generativelanguage.googleapis.com/v1beta/models/") + model + ":generateContent";
    if (!http.begin(tls, url)) { error = "No se pudo preparar la conexion con Gemini."; return false; }
    http.setConnectTimeout(10000);
    http.setTimeout(45000);
    http.setReuse(false);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-goog-api-key", GEMINI_API_KEY);
    return true;
}

// El handshake TLS necesita ~40 KB contiguos; con la pila Bluetooth activa no caben.
bool sinMemoriaTls(int code, WiFiClientSecure& tls, String& error) {
    if (code >= 0) return false;
    char detail[160] = {};
    int tlsError = tls.lastError(detail, sizeof(detail));
    Serial.printf("[Cloud] HTTP=%d TLS=%d %s heap=%u max=%u\n", code, tlsError, detail, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    if (tlsError != -32512) return false;   // MBEDTLS_ERR_SSL_ALLOC_FAILED
    error = "Sin memoria para conectar con Gemini: el Bluetooth ocupa la RAM. Reinicia la placa para liberarla.";
    return true;
}

bool checkResponse(int code, String& error) {
    if (code == 200) return true;
    if (code == 429) error = "Se alcanzo la cuota de Gemini. Espera antes de volver a intentarlo.";
    else if (code == 401 || code == 403) error = "Gemini rechazo la clave API o sus permisos.";
    else if (code < 0) error = "No se pudo conectar con Gemini por Internet. Revisa WiFi y vuelve a intentarlo.";
    else error = String("Gemini respondio HTTP ") + code + ". Revisa el modelo y la cuenta.";
    return false;
}
}

void clearGeminiMemory() {
    for (size_t i = 0; i < 3; i++) { historyUser[i] = ""; historyModel[i] = ""; }
    historyCount = 0;
}

bool geminiReply(const String& message, const String& devices, const String& network, JsonDocument& reply, String& error) {
    JsonDocument request;
    String red = network.length()
        ? ". Escaneo de la red WiFi recien hecho (SSDP y mDNS), solo informativo, no los controlas: " + network +
          ". Si preguntan que dispositivos hay en la red, nombralos con su tipo de forma breve"
        : String("");
    request["systemInstruction"]["parts"][0]["text"] =
        String("Eres Gimena, asistente de hogar de la Universidad de La Guajira. Responde en espanol, sin emojis, ") +
        "con un maximo de 45 palabras. Estas ejecutandote directamente en un ESP32. Solo controla estas salidas: " + devices +
        red + ". Devuelve JSON estricto {\"reply\":\"respuesta\",\"face\":\"happy\",\"device_actions\":[]}. " +
        "face debe ser neutral, happy, love, surprised, sad, thinking o sleepy. " +
        "Si piden encender o apagar una salida, usa device_actions:[{\"device\":\"id\",\"state\":true}]. " +
        "No inventes dispositivos conectados. Recuerda los mensajes anteriores.";
    JsonArray contents = request["contents"].to<JsonArray>();
    for (size_t i = 0; i < historyCount; i++) {
        JsonObject user = contents.add<JsonObject>();
        user["role"] = "user"; user["parts"][0]["text"] = historyUser[i];
        JsonObject model = contents.add<JsonObject>();
        model["role"] = "model"; model["parts"][0]["text"] = historyModel[i];
    }
    JsonObject user = contents.add<JsonObject>();
    user["role"] = "user"; user["parts"][0]["text"] = message;
    request["generationConfig"]["responseMimeType"] = "application/json";
    request["generationConfig"]["maxOutputTokens"] = 512;
    String payload;
    serializeJson(request, payload);
    request.clear();

    WiFiClientSecure tls;
    HTTPClient http;
    if (!beginCloud(http, tls, GEMINI_MODEL, error)) return false;
    int code = http.POST(payload);
    payload = "";
    if (sinMemoriaTls(code, tls, error) || !checkResponse(code, error)) { http.end(); return false; }
    BoundedJson body;
    int transferred = http.writeToStream(&body);
    http.end();
    tls.stop();
    if (transferred < 0 || body.overflow) { error = "La respuesta de Gemini no se pudo leer completa."; return false; }
    JsonDocument envelope;
    if (deserializeJson(envelope, body.text)) { error = "Gemini devolvio una respuesta invalida."; return false; }
    body.text = "";
    String answer;
    for (JsonObject part : envelope["candidates"][0]["content"]["parts"].as<JsonArray>()) {
        if (!(part["thought"] | false)) answer += part["text"] | "";
    }
    envelope.clear();
    if (deserializeJson(reply, answer) || !reply["reply"].is<const char*>() ||
        !reply["device_actions"].is<JsonArray>() || strlen(reply["reply"] | "") > 1000) {
        error = "Gemini no devolvio una respuesta valida. Vuelve a intentarlo.";
        return false;
    }
    if (historyCount == 3) {
        for (size_t i = 1; i < 3; i++) { historyUser[i-1] = historyUser[i]; historyModel[i-1] = historyModel[i]; }
        historyCount--;
    }
    historyUser[historyCount] = message;
    historyModel[historyCount++] = answer;
    return true;
}

bool geminiSpeech(const String& text, uint32_t rate, void (*output)(const uint8_t*, size_t), String& error, int volume) {
    JsonDocument request;
    request["contents"][0]["parts"][0]["text"] = "Lee en espanol colombiano, con voz femenina calida, exactamente este texto: " + text;
    request["generationConfig"]["responseModalities"][0] = "AUDIO";
    request["generationConfig"]["speechConfig"]["voiceConfig"]["prebuiltVoiceConfig"]["voiceName"] = GEMINI_TTS_VOICE;
    String payload;
    serializeJson(request, payload);
    request.clear();
    WiFiClientSecure tls;
    HTTPClient http;
    if (!beginCloud(http, tls, GEMINI_TTS_MODEL, error)) return false;
    int code = http.POST(payload);
    payload = "";
    if (sinMemoriaTls(code, tls, error) || !checkResponse(code, error)) { http.end(); return false; }
    AudioJson sink(rate, volume, output);
    int transferred = http.writeToStream(&sink);
    sink.decoder.finish();
    http.end();
    tls.stop();
    if (transferred < 0 || !sink.decoder.valid()) { error = "No se pudo completar la voz de Gemini."; return false; }
    Serial.printf("[Voz directa] %u muestras reproducidas a %u Hz\n", sink.decoder.sampleCount(), rate);
    return true;
}
