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
    AudioJson(uint32_t rate, int volume, void (*output)(const uint8_t*, size_t),
              const char* key = "data", uint32_t inputRate = 24000,
              AudioStreamDecoder::Formato formato = AudioStreamDecoder::PCM16)
        : decoder(rate, volume, output, key, inputRate, formato) {}
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

bool claveValida(const char* key) {
    return strlen(key) >= 20 && !String(key).startsWith("TU_");
}

// Conexión HTTPS persistente (keep-alive): el handshake TLS cuesta ~2 s en el ESP32,
// así que Gemini y Cloud TTS mantienen cada uno su conexión abierta entre consultas.
// HTTPClient cierra el socket al destruirse, por eso ambos objetos viven aquí.
struct Conexion {
    WiFiClientSecure tls;
    HTTPClient http;
    bool reutilizada = false;   // la petición actual va por una conexión ya abierta
};
Conexion conexionGemini, conexionTts;

void cerrar(Conexion& c) {
    c.http.end();
    c.tls.stop();
}

bool beginHttps(Conexion& c, const char* host, const String& path, const char* apiKey, String& error) {
    HTTPClient& http = c.http;
    WiFiClientSecure& tls = c.tls;
    if (time(nullptr) < 1700000000) {
        error = "Esperando sincronizacion de hora para conectar con Google de forma segura.";
        return false;
    }
    c.reutilizada = tls.connected();
    IPAddress address;
    if (!c.reutilizada && !WiFi.hostByName(host, address)) {
        error = "El ESP32 no pudo resolver el nombre del servicio de Google. Revisa el DNS de la red WiFi.";
        return false;
    }
    if (c.reutilizada) Serial.printf("[Cloud] %s (conexion reutilizada) heap=%u\n", host, ESP.getFreeHeap());
    else Serial.printf("[Cloud] %s DNS=%s hora=%ld heap=%u max=%u\n", host, address.toString().c_str(), (long)time(nullptr), ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    // El almacén del SDK valida cadenas RSA/ECDSA sin analizar todas las raíces en RAM.
    tls.setCACertBundle(x509_crt_bundle_start);
    tls.setHandshakeTimeout(15);
    if (!http.begin(tls, String("https://") + host + path)) { error = "No se pudo preparar la conexion HTTPS."; return false; }
    http.setConnectTimeout(10000);
    http.setTimeout(45000);
    http.setReuse(true);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-goog-api-key", apiKey);
    return true;
}

bool beginCloud(Conexion& c, const char* model, String& error) {
    if (!claveValida(GEMINI_API_KEY)) {
        error = "Configura GEMINI_API_KEY en secrets.h y vuelve a cargar el firmware.";
        return false;
    }
    return beginHttps(c, "generativelanguage.googleapis.com",
                      String("/v1beta/models/") + model + ":generateContent", GEMINI_API_KEY, error);
}

// El handshake TLS necesita ~40 KB contiguos.
bool sinMemoriaTls(int code, WiFiClientSecure& tls, String& error) {
    if (code >= 0) return false;
    char detail[160] = {};
    int tlsError = tls.lastError(detail, sizeof(detail));
    Serial.printf("[Cloud] HTTP=%d TLS=%d %s heap=%u max=%u\n", code, tlsError, detail, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    if (tlsError != -32512) return false;   // MBEDTLS_ERR_SSL_ALLOC_FAILED
    error = "Sin memoria para conectar con Google. Reinicia la placa para liberarla.";
    return true;
}

// POST que reintenta una vez con conexión nueva si el servidor ya había cerrado la reutilizada.
template <typename Begin>
int postConReintento(Conexion& c, const String& payload, Begin begin) {
    if (!begin()) return HTTPC_ERROR_CONNECTION_REFUSED;
    int code = c.http.POST(payload);
    if (code < 0 && c.reutilizada) {
        Serial.printf("[Cloud] conexion reutilizada cerrada por el servidor (%d); reconectando\n", code);
        cerrar(c);
        if (!begin()) return HTTPC_ERROR_CONNECTION_REFUSED;
        code = c.http.POST(payload);
    }
    return code;
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

String horaLocal() {
    time_t t = time(nullptr) - 5 * 3600;   // Colombia, UTC-5 sin horario de verano
    struct tm h;
    gmtime_r(&t, &h);
    static const char* dias[] = {"domingo", "lunes", "martes", "miercoles", "jueves", "viernes", "sabado"};
    char out[48];
    snprintf(out, sizeof(out), "%s %02d/%02d/%04d %02d:%02d", dias[h.tm_wday], h.tm_mday, h.tm_mon + 1, h.tm_year + 1900, h.tm_hour, h.tm_min);
    return out;
}

void clearGeminiMemory() {
    for (size_t i = 0; i < 3; i++) { historyUser[i] = ""; historyModel[i] = ""; }
    historyCount = 0;
}

bool geminiReply(const String& message, const String& devices, const String& network, JsonDocument& reply, String& error) {
    JsonDocument request;
    String red = network.length()
        ? String(". Escaneo de la red WiFi recien hecho (SSDP y mDNS), solo informativo; solo controlas los que tambien ") +
          "esten en la lista de salidas: " + network +
          ". Si preguntan que dispositivos hay en la red, nombralos con su tipo de forma breve"
        : String("");
    request["systemInstruction"]["parts"][0]["text"] =
        String("Eres Gimena, asistente de hogar de la Universidad de La Guajira. Responde en espanol, sin emojis, ") +
        "con un maximo de 45 palabras. Estas ejecutandote directamente en un ESP32. Solo controla estas salidas: " + devices +
        red + ". Devuelve JSON estricto {\"reply\":\"respuesta\",\"face\":\"happy\",\"animation\":\"none\",\"device_actions\":[]}. " +
        "face debe ser neutral, happy, love, surprised, sad, thinking o sleepy. " +
        "animation es opcional (por defecto none): musica si cantas o hablan de musica, corazones si hay carino, " +
        "fiesta si celebran o felicitan, saludo al saludar o despedirse, reloj si preguntan la hora o la fecha. " +
        "Hora local actual (Colombia): " + horaLocal() + ". " +
        String("Si piden encender o apagar una salida, usa device_actions:[{\"device\":\"id\",\"state\":true}] ") +
        "con el id exacto (luz_sala, tuya_1...). Los tuya_N son enchufes de la app Smart Life: nombralos por su nombre. " +
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

    Conexion& c = conexionGemini;
    int code = postConReintento(c, payload, [&] { return beginCloud(c, GEMINI_MODEL, error); });
    payload = "";
    if (error.length()) { cerrar(c); return false; }
    if (sinMemoriaTls(code, c.tls, error) || !checkResponse(code, error)) { cerrar(c); return false; }
    BoundedJson body;
    int transferred = c.http.writeToStream(&body);
    if (transferred < 0 || body.overflow) { cerrar(c); error = "La respuesta de Gemini no se pudo leer completa."; return false; }
    c.http.end();   // deja el socket abierto para la próxima consulta
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
    Conexion& c = conexionGemini;
    int code = postConReintento(c, payload, [&] { return beginCloud(c, GEMINI_TTS_MODEL, error); });
    payload = "";
    if (error.length()) { cerrar(c); return false; }
    if (sinMemoriaTls(code, c.tls, error) || !checkResponse(code, error)) { cerrar(c); return false; }
    AudioJson sink(rate, volume, output);
    int transferred = c.http.writeToStream(&sink);
    sink.decoder.finish();
    if (transferred < 0 || !sink.decoder.valid()) { cerrar(c); error = "No se pudo completar la voz de Gemini."; return false; }
    c.http.end();
    Serial.printf("[Voz directa] %u muestras reproducidas a %u Hz\n", sink.decoder.sampleCount(), rate);
    return true;
}

// Google Cloud Text-to-Speech: sintetiza en <1 s (Gemini TTS tarda ~5 s) y, en µ-law
// de 8 bit a 16 kHz, pesa un tercio que el PCM de Gemini: la placa lo descarga más
// rápido de lo que dura y lo va reproduciendo mientras llega.
bool cloudSpeech(const String& text, uint32_t rate, void (*output)(const uint8_t*, size_t), String& error, int volume) {
    JsonDocument request;
    request["input"]["text"] = text;
    request["voice"]["languageCode"] = GOOGLE_TTS_LANGUAGE;
    request["voice"]["name"] = GOOGLE_TTS_VOICE;
    request["audioConfig"]["audioEncoding"] = "MULAW";
    request["audioConfig"]["sampleRateHertz"] = GOOGLE_TTS_RATE;
    request["audioConfig"]["speakingRate"] = GOOGLE_TTS_SPEED;
    String payload;
    serializeJson(request, payload);
    request.clear();
    Conexion& c = conexionTts;
    unsigned long inicio = millis();
    int code = postConReintento(c, payload, [&] {
        return beginHttps(c, "texttospeech.googleapis.com", "/v1/text:synthesize", GOOGLE_TTS_API_KEY, error);
    });
    payload = "";
    if (error.length()) { cerrar(c); return false; }
    if (sinMemoriaTls(code, c.tls, error)) { cerrar(c); return false; }
    if (code != 200) {
        if (code == 401 || code == 403) error = "Google Cloud TTS rechazo la clave. Activa la API Text-to-Speech para esa clave.";
        else if (code < 0) error = "No se pudo conectar con Google Cloud TTS.";
        else error = String("Google Cloud TTS respondio HTTP ") + code + ".";
        cerrar(c);
        return false;
    }
    AudioJson sink(rate, volume, output, "audioContent", GOOGLE_TTS_RATE, AudioStreamDecoder::MULAW_WAV);
    int transferred = c.http.writeToStream(&sink);
    sink.decoder.finish();
    if (transferred < 0 || !sink.decoder.valid()) { cerrar(c); error = "No se pudo completar la voz de Cloud TTS."; return false; }
    c.http.end();
    Serial.printf("[Voz Cloud TTS] %u muestras a %u Hz en %lu ms\n", sink.decoder.sampleCount(), rate, millis() - inicio);
    return true;
}

// Otra conexión TLS (Tuya) necesita ~45 KB contiguos: con Gemini y Cloud TTS abiertas
// el heap queda fragmentado. Se cierra primero Gemini (ya respondió en esta consulta)
// y, si aún no alcanza, Cloud TTS. precalentarConexiones() las reabre en reposo.
void liberarMemoriaTls(size_t bloqueNecesario) {
    if (ESP.getMaxAllocHeap() >= bloqueNecesario) return;
    if (conexionGemini.tls.connected()) cerrar(conexionGemini);
    if (ESP.getMaxAllocHeap() < bloqueNecesario && conexionTts.tls.connected()) cerrar(conexionTts);
    Serial.printf("[Cloud] conexiones liberadas para otra TLS: max=%u\n", ESP.getMaxAllocHeap());
}

// Abre (o reabre) en segundo plano la conexión que esté cerrada, para que la próxima
// consulta no pague el handshake TLS. Una por llamada: cada handshake bloquea ~2 s.
void precalentarConexiones() {
    static unsigned long ultimoIntento = 0;
    if (time(nullptr) < 1700000000 || millis() - ultimoIntento < 20000) return;
    struct { Conexion* c; const char* host; bool usar; } destinos[] = {
        {&conexionGemini, "generativelanguage.googleapis.com", claveValida(GEMINI_API_KEY)},
        {&conexionTts, "texttospeech.googleapis.com", claveValida(GOOGLE_TTS_API_KEY)},
    };
    for (auto& d : destinos) {
        if (!d.usar || d.c->tls.connected()) continue;
        ultimoIntento = millis();
        d.c->tls.setCACertBundle(x509_crt_bundle_start);
        d.c->tls.setHandshakeTimeout(15);
        unsigned long inicio = millis();
        bool ok = d.c->tls.connect(d.host, 443);
        Serial.printf("[Cloud] precalentar %s: %s en %lu ms heap=%u\n", d.host, ok ? "ok" : "fallo", millis() - inicio, ESP.getFreeHeap());
        if (!ok) d.c->tls.stop();
        return;
    }
}

bool sintetizarVoz(const String& text, uint32_t rate, void (*output)(const uint8_t*, size_t), String& error, int volume) {
    if (claveValida(GOOGLE_TTS_API_KEY)) return cloudSpeech(text, rate, output, error, volume);
    return geminiSpeech(text, rate, output, error, volume);
}
