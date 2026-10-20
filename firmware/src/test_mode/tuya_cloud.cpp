#include "tuya_cloud.h"
#include "test_config.h"
#include "standalone_cloud.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/md.h>
#include <sys/time.h>

#ifndef TUYA_CLIENT_ID
#define TUYA_CLIENT_ID     ""
#endif
#ifndef TUYA_CLIENT_SECRET
#define TUYA_CLIENT_SECRET ""
#endif
#ifndef TUYA_REGION
#define TUYA_REGION        "us"
#endif

extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");

namespace {
const size_t MAX_TUYA = 8;
DispositivoTuya dispositivos[MAX_TUYA];
size_t cantidad = 0;
unsigned long listaEn = 0;
bool listaValida = false;
String token;
unsigned long tokenVence = 0;

const char* hostTuya() {
    String r = TUYA_REGION;
    if (r == "eu") return "openapi.tuyaeu.com";
    if (r == "in") return "openapi.tuyain.com";
    if (r == "cn") return "openapi.tuyacn.com";
    return "openapi.tuyaus.com";   // Western America (Colombia)
}

String hex(const uint8_t* data, size_t n, bool mayusculas) {
    static const char* minus = "0123456789abcdef";
    static const char* mayus = "0123456789ABCDEF";
    const char* d = mayusculas ? mayus : minus;
    String out;
    out.reserve(n * 2);
    for (size_t i = 0; i < n; i++) { out += d[data[i] >> 4]; out += d[data[i] & 15]; }
    return out;
}

String sha256Hex(const String& texto) {
    uint8_t out[32];
    mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const uint8_t*)texto.c_str(), texto.length(), out);
    return hex(out, 32, false);
}

String hmacHex(const String& texto) {
    uint8_t out[32];
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                    (const uint8_t*)TUYA_CLIENT_SECRET, strlen(TUYA_CLIENT_SECRET),
                    (const uint8_t*)texto.c_str(), texto.length(), out);
    return hex(out, 32, true);
}

// Firma de Tuya OpenAPI: HMAC-SHA256(secret, client_id + access_token + t + nonce + stringToSign).
bool llamar(const char* metodo, const String& ruta, const String& cuerpo, bool conToken,
            JsonDocument& respuesta, JsonDocument* filtro, String& error) {
    struct timeval ahora;
    gettimeofday(&ahora, nullptr);
    if (ahora.tv_sec < 1700000000) { error = "Esperando la hora de internet para hablar con Tuya."; return false; }
    char t[24];   // milisegundos Unix
    snprintf(t, sizeof(t), "%lld", (long long)ahora.tv_sec * 1000LL + ahora.tv_usec / 1000);
    uint8_t aleatorio[16];
    esp_fill_random(aleatorio, sizeof(aleatorio));
    String nonce = hex(aleatorio, sizeof(aleatorio), false);
    String aFirmar = String(metodo) + "\n" + sha256Hex(cuerpo) + "\n\n" + ruta;
    String firma = hmacHex(String(TUYA_CLIENT_ID) + (conToken ? token : String("")) + t + nonce + aFirmar);

    liberarMemoriaTls(60000);
    WiFiClientSecure tls;
    tls.setCACertBundle(x509_crt_bundle_start);
    tls.setHandshakeTimeout(15);
    HTTPClient http;
    if (!http.begin(tls, String("https://") + hostTuya() + ruta)) { error = "No se pudo preparar la conexion con Tuya."; return false; }
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    http.addHeader("client_id", TUYA_CLIENT_ID);
    http.addHeader("sign", firma);
    http.addHeader("t", t);
    http.addHeader("nonce", nonce);
    http.addHeader("sign_method", "HMAC-SHA256");
    http.addHeader("Content-Type", "application/json");
    if (conToken) http.addHeader("access_token", token);
    int codigo = strcmp(metodo, "POST") == 0 ? http.POST(cuerpo) : http.GET();
    if (codigo != 200) {
        char detalle[120] = {};
        int tlsError = tls.lastError(detalle, sizeof(detalle));
        Serial.printf("[Tuya] HTTP=%d TLS=%d %s heap=%u max=%u\n", codigo, tlsError, detalle, ESP.getFreeHeap(), ESP.getMaxAllocHeap());
        http.end();
        error = codigo < 0 ? "No se pudo conectar con la nube de Tuya." : String("Tuya respondio HTTP ") + codigo + ".";
        return false;
    }
    String texto = http.getString();
    http.end();
    DeserializationError e = filtro ? deserializeJson(respuesta, texto, DeserializationOption::Filter(*filtro))
                                    : deserializeJson(respuesta, texto);
    if (e) { error = "Tuya devolvio una respuesta invalida."; return false; }
    if (!(respuesta["success"] | false)) {
        int code = respuesta["code"] | 0;
        Serial.printf("[Tuya] %s %s -> code=%d %s\n", metodo, ruta.c_str(), code, (const char*)(respuesta["msg"] | ""));
        if (code == 1010 || code == 1011) tokenVence = 0;   // token vencido o inválido: pedir otro
        error = String("Tuya rechazo la peticion: ") + (const char*)(respuesta["msg"] | "error desconocido");
        return false;
    }
    return true;
}

bool asegurarToken(String& error) {
    if (token.length() && (int32_t)(tokenVence - millis()) > 60000) return true;
    JsonDocument r;
    if (!llamar("GET", "/v1.0/token?grant_type=1", "", false, r, nullptr, error)) return false;
    token = r["result"]["access_token"].as<String>();
    tokenVence = millis() + (uint32_t)(r["result"]["expire_time"] | 3600) * 1000UL;
    return token.length() > 0;
}
}

bool tuyaConfigurado() {
    return strlen(TUYA_CLIENT_ID) >= 10 && strlen(TUYA_CLIENT_SECRET) >= 20;
}

bool tuyaActualizar(String& error, uint32_t maxEdadMs) {
    if (!tuyaConfigurado()) return false;
    if (listaValida && millis() - listaEn < maxEdadMs) return true;
    if (!asegurarToken(error)) return false;
    // Solo se guardan los campos necesarios (la respuesta trae local_key, icono, etc.).
    JsonDocument filtro;
    filtro["success"] = true; filtro["code"] = true; filtro["msg"] = true;
    JsonObject f = filtro["result"]["devices"][0].to<JsonObject>();
    f["id"] = true; f["name"] = true; f["online"] = true; f["category"] = true;
    f["status"][0]["code"] = true; f["status"][0]["value"] = true;
    JsonDocument r;
    if (!llamar("GET", "/v1.0/iot-01/associated-users/devices?size=20", "", true, r, &filtro, error)) return false;
    cantidad = 0;
    for (JsonObject d : r["result"]["devices"].as<JsonArray>()) {
        if (cantidad >= MAX_TUYA) break;
        DispositivoTuya& x = dispositivos[cantidad];
        x = DispositivoTuya();
        x.id = d["id"].as<String>();
        x.nombre = d["name"].as<String>();
        x.alias = String("tuya_") + (cantidad + 1);
        x.enLinea = d["online"] | false;
        for (JsonObject s : d["status"].as<JsonArray>()) {
            String code = s["code"] | "";
            if (!x.codigoSwitch.length() && code.startsWith("switch") && s["value"].is<bool>()) {
                x.codigoSwitch = code;
                x.encendido = s["value"];
            } else if (code == "cur_power") {
                x.potencia = (s["value"] | 0) / 10.0f;
            }
        }
        if (x.codigoSwitch.length()) cantidad++;   // por ahora solo enchufes/interruptores
    }
    listaEn = millis();
    listaValida = true;
    Serial.printf("[Tuya] %u dispositivo(s) en la cuenta vinculada\n", (unsigned)cantidad);
    return true;
}

bool tuyaComando(const String& aliasOId, bool encender, String& error) {
    DispositivoTuya* x = nullptr;
    for (size_t i = 0; i < cantidad; i++) {
        if (dispositivos[i].alias == aliasOId || dispositivos[i].id == aliasOId) x = &dispositivos[i];
    }
    if (!x) { error = "Dispositivo Tuya desconocido: " + aliasOId; return false; }
    if (!asegurarToken(error)) return false;
    JsonDocument cmd;
    cmd["commands"][0]["code"] = x->codigoSwitch;
    cmd["commands"][0]["value"] = encender;
    String cuerpo;
    serializeJson(cmd, cuerpo);
    JsonDocument r;
    if (!llamar("POST", "/v1.0/devices/" + x->id + "/commands", cuerpo, true, r, nullptr, error)) return false;
    x->encendido = encender;
    Serial.printf("[Tuya] %s -> %s\n", x->nombre.c_str(), encender ? "ON" : "OFF");
    return true;
}

String tuyaResumen() {
    String out;
    for (size_t i = 0; i < cantidad; i++) {
        const DispositivoTuya& x = dispositivos[i];
        if (out.length()) out += "; ";
        out += x.alias + " = " + x.nombre + " (enchufe inteligente Tuya";
        if (!x.enLinea) out += ", desconectado";
        out += String("): ") + (x.encendido ? "ON" : "OFF");
        if (x.enLinea && x.potencia >= 0) out += ", consumo " + String(x.potencia, 1) + " W";
    }
    return out;
}

size_t enchufesTuya() { return cantidad; }
const DispositivoTuya& enchufeTuya(size_t i) { return dispositivos[i]; }
