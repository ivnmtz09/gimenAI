#pragma once
// ------------------------------------------------------------------------------
// Escaneo de dispositivos IoT en la red local. Combina en paralelo:
//   - SSDP/UPnP (TVs, Echo, Chromecast, routers, Hue...) + su descripción XML
//   - mDNS/DNS-SD (Google Cast, HomeKit, ESPHome, impresoras, AirPlay...)
//   - Tuya / Smart Life: difusiones UDP 6666 (v3.1, texto) y 6667 (v3.3, AES)
//   - TP-Link Kasa: consulta de descubrimiento por UDP 9999
// El resultado se guarda unos minutos para no repetir el escaneo en cada mensaje.
// ------------------------------------------------------------------------------
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <mdns.h>
#include <mbedtls/aes.h>

struct DispositivoRed {
    IPAddress ip;
    String nombre;     // friendlyName / instancia mDNS / hostname
    String tipo;       // servicio o modelo
    String fabricante;
    String descripcion;   // URL de descripción UPnP (solo para consultarla)
};

const int RED_MAX_DISPOSITIVOS = 24;
const unsigned long RED_CACHE_MS = 3UL * 60UL * 1000UL;
DispositivoRed redDispositivos[RED_MAX_DISPOSITIVOS];
int redCantidad = 0;
unsigned long redEscaneadoEn = 0;
bool redEscaneada = false;

// Servicios mDNS más comunes en hogares (tipo, protocolo, etiqueta legible)
const char* const MDNS_SERVICIOS[][3] = {
    {"_googlecast", "_tcp", "Google Cast"},
    {"_hap", "_tcp", "HomeKit"},
    {"_esphomelib", "_tcp", "ESPHome"},
    {"_airplay", "_tcp", "AirPlay"},
    {"_spotify-connect", "_tcp", "Spotify Connect"},
    {"_ipp", "_tcp", "Impresora"},
    {"_printer", "_tcp", "Impresora"},
    {"_hue", "_tcp", "Philips Hue"},
    {"_shelly", "_tcp", "Shelly"},
    {"_mqtt", "_tcp", "Servidor MQTT"},
    {"_arduino", "_tcp", "Arduino/ESP"},
    {"_http", "_tcp", "Servidor web"},
};
const int MDNS_CANTIDAD = sizeof(MDNS_SERVICIOS) / sizeof(MDNS_SERVICIOS[0]);

DispositivoRed* redBuscarOCrear(IPAddress ip) {
    for (int i = 0; i < redCantidad; i++) if (redDispositivos[i].ip == ip) return &redDispositivos[i];
    if (redCantidad >= RED_MAX_DISPOSITIVOS) return nullptr;
    DispositivoRed* d = &redDispositivos[redCantidad++];
    *d = DispositivoRed();
    d->ip = ip;
    return d;
}

String cabeceraSsdp(const String& respuesta, const char* nombre) {
    String bajo = respuesta;
    bajo.toLowerCase();
    String clave = String("\n") + nombre + ":";
    int i = bajo.indexOf(clave);
    if (i < 0) return "";
    int inicio = i + clave.length();
    int fin = respuesta.indexOf('\r', inicio);
    String valor = respuesta.substring(inicio, fin < 0 ? respuesta.length() : fin);
    valor.trim();
    return valor;
}

String etiquetaXml(const String& xml, const char* etiqueta) {
    String abre = String("<") + etiqueta + ">";
    int i = xml.indexOf(abre);
    if (i < 0) return "";
    int fin = xml.indexOf("</", i);
    if (fin < 0) return "";
    String valor = xml.substring(i + abre.length(), fin);
    valor.trim();
    return valor.substring(0, 48);
}

// Lee el principio de la descripción UPnP (ahí están friendlyName, manufacturer y modelName)
void leerDescripcionUpnp(DispositivoRed& d) {
    if (!d.descripcion.startsWith("http://")) return;
    WiFiClient cliente;
    HTTPClient http;
    http.setConnectTimeout(1200);
    http.setTimeout(1500);
    if (!http.begin(cliente, d.descripcion)) return;
    if (http.GET() == 200) {
        WiFiClient* flujo = http.getStreamPtr();
        String xml;
        unsigned long limite = millis() + 1500;
        while (xml.length() < 3000 && millis() < limite && (flujo->connected() || flujo->available())) {
            while (flujo->available() && xml.length() < 3000) xml += (char)flujo->read();
            serviceStandalone();
        }
        String nombre = etiquetaXml(xml, "friendlyName");
        if (nombre.length()) d.nombre = nombre;
        String fab = etiquetaXml(xml, "manufacturer");
        if (fab.length()) d.fabricante = fab;
        String modelo = etiquetaXml(xml, "modelName");
        if (modelo.length()) d.tipo = modelo;
    }
    http.end();
}

// ---- Tuya / Smart Life ----
// Los equipos Tuya no usan SSDP ni mDNS: difunden cada ~5 s una trama 0x000055AA con
// JSON {ip, gwId, productKey, version}. En v3.3 va cifrado con AES-128-ECB y la clave
// pública de descubrimiento md5("yGAdlopoPVldABfn").
const uint8_t TUYA_CLAVE_UDP[16] = {0x6c, 0x1e, 0xc8, 0xe2, 0xbb, 0x9b, 0xb5, 0x9a,
                                    0xb5, 0x0b, 0x0d, 0xaf, 0x64, 0x9b, 0x41, 0x0a};

// Difunden a intervalos irregulares (3-11 s), así que se escuchan siempre en segundo
// plano y el escaneo solo incorpora los vistos en los últimos minutos.
struct TuyaVisto { IPAddress ip; char version[8]; unsigned long visto; };
const int TUYA_MAX = 8;
const unsigned long TUYA_VIGENCIA_MS = 5UL * 60UL * 1000UL;
TuyaVisto tuyaVistos[TUYA_MAX];
int tuyaCantidad = 0;
WiFiUDP tuyaUdp31, tuyaUdp33;
bool tuyaEscuchando = false;

void registrarTuya(const char* json, size_t n, IPAddress origen) {
    JsonDocument doc;
    if (deserializeJson(doc, json, n)) return;
    IPAddress ip;
    if (!ip.fromString(doc["ip"] | "")) ip = origen;
    int i = 0;
    while (i < tuyaCantidad && tuyaVistos[i].ip != ip) i++;
    if (i == tuyaCantidad) {
        if (tuyaCantidad == TUYA_MAX) return;
        tuyaCantidad++;
        tuyaVistos[i].ip = ip;
        Serial.printf("[Red] Tuya detectado en %s (v%s)\n", ip.toString().c_str(), doc["version"] | "?");
    }
    strlcpy(tuyaVistos[i].version, doc["version"] | "?", sizeof(tuyaVistos[i].version));
    tuyaVistos[i].visto = millis();
}

void leerTuya(WiFiUDP& udp, bool cifrado) {
    if (udp.parsePacket() <= 0) return;
    static uint8_t trama[512];
    int len = udp.read(trama, sizeof(trama));
    if (len < 36 || trama[2] != 0x55 || trama[3] != 0xAA) return;
    // prefijo(4) secuencia(4) comando(4) longitud(4) retorno(4) | carga | crc(4) sufijo(4)
    uint8_t* carga = trama + 20;
    size_t n = len - 28;
    if (!cifrado) {
        uint8_t* ini = (uint8_t*)memchr(carga, '{', n);
        if (ini) registrarTuya((const char*)ini, n - (ini - carga), udp.remoteIP());
        return;
    }
    n -= n % 16;
    if (!n) return;
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, TUYA_CLAVE_UDP, 128);
    for (size_t i = 0; i < n; i += 16) mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, carga + i, carga + i);
    mbedtls_aes_free(&aes);
    uint8_t relleno = carga[n - 1];   // PKCS#7
    if (relleno >= 1 && relleno <= 16) n -= relleno;
    registrarTuya((const char*)carga, n, udp.remoteIP());
}

void iniciarEscuchaTuya() {
    tuyaEscuchando = tuyaUdp31.begin(6666);
    tuyaUdp33.begin(6667);
}

// Se llama desde loop(): lee como mucho una trama por puerto en cada vuelta
void atenderEscuchaTuya() {
    if (!tuyaEscuchando) return;
    leerTuya(tuyaUdp31, false);
    leerTuya(tuyaUdp33, true);
}

void incorporarTuyaVistos() {
    for (int i = 0; i < tuyaCantidad; i++) {
        if (millis() - tuyaVistos[i].visto > TUYA_VIGENCIA_MS) continue;
        DispositivoRed* d = redBuscarOCrear(tuyaVistos[i].ip);
        if (!d) return;
        d->nombre = "Dispositivo Tuya / Smart Life";
        d->tipo = String("enchufe o equipo Tuya v") + tuyaVistos[i].version + ", control local por TCP 6668";
        d->fabricante = "Tuya";
    }
}

// ---- TP-Link Kasa: JSON con XOR encadenado (clave inicial 171) ----
size_t kasaCifrar(const char* texto, uint8_t* salida) {
    uint8_t clave = 171;
    size_t n = strlen(texto);
    for (size_t i = 0; i < n; i++) { clave ^= (uint8_t)texto[i]; salida[i] = clave; }
    return n;
}

void leerKasa(WiFiUDP& udp) {
    if (udp.parsePacket() <= 0) return;
    static char texto[1024];
    int len = udp.read((uint8_t*)texto, sizeof(texto) - 1);
    if (len <= 0) return;
    uint8_t clave = 171;
    for (int i = 0; i < len; i++) { uint8_t c = (uint8_t)texto[i]; texto[i] = (char)(clave ^ c); clave = c; }
    JsonDocument doc;
    if (deserializeJson(doc, texto, len)) return;
    JsonObject info = doc["system"]["get_sysinfo"];
    if (info.isNull()) return;
    DispositivoRed* d = redBuscarOCrear(udp.remoteIP());
    if (!d) return;
    d->nombre = String(info["alias"] | "Dispositivo Kasa").substring(0, 48);
    d->tipo = String(info["model"] | (info["dev_name"] | "TP-Link Kasa")).substring(0, 48);
    d->fabricante = "TP-Link Kasa";
}

void escanearRed() {
    Serial.println("[Red] Escaneando dispositivos IoT (SSDP + mDNS + Tuya + Kasa)...");
    unsigned long inicio = millis();
    redCantidad = 0;

    // 1. Lanzar todas las búsquedas mDNS a la vez (corren en segundo plano)
    mdns_search_once_t* busquedas[MDNS_CANTIDAD] = {};
    for (int i = 0; i < MDNS_CANTIDAD; i++)
        busquedas[i] = mdns_query_async_new(nullptr, MDNS_SERVICIOS[i][0], MDNS_SERVICIOS[i][1],
                                            MDNS_TYPE_PTR, 2500, 8, nullptr);

    // 2. Mientras tanto: SSDP y Kasa (Tuya se escucha siempre en segundo plano)
    WiFiUDP udp, kasa;
    bool ssdpOk = udp.begin(41900);
    if (kasa.begin(41999)) {
        static uint8_t consulta[64];
        size_t n = kasaCifrar("{\"system\":{\"get_sysinfo\":{}}}", consulta);
        kasa.beginPacket(WiFi.broadcastIP(), 9999);
        kasa.write(consulta, n);
        kasa.endPacket();
    }
    if (ssdpOk) {
        const char* msearch =
            "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: ssdp:all\r\n\r\n";
        for (int intento = 0; intento < 2; intento++) {   // UDP puede perderse: se envía dos veces
            udp.beginPacket(IPAddress(239, 255, 255, 250), 1900);
            udp.write((const uint8_t*)msearch, strlen(msearch));
            udp.endPacket();
            delay(50);
        }
    }
    unsigned long fin = millis() + 3000;
    char paquete[700];
    while (millis() < fin) {
        int n = ssdpOk ? udp.parsePacket() : 0;
        if (n > 0) {
            int leidos = udp.read(paquete, sizeof(paquete) - 1);
            paquete[max(leidos, 0)] = 0;
            String respuesta = String("\n") + paquete;
            DispositivoRed* d = redBuscarOCrear(udp.remoteIP());
            if (d) {
                if (!d->descripcion.length()) d->descripcion = cabeceraSsdp(respuesta, "location");
                if (!d->tipo.length()) d->tipo = cabeceraSsdp(respuesta, "server").substring(0, 48);
            }
            continue;
        }
        atenderEscuchaTuya();
        leerKasa(kasa);
        serviceStandalone();
        delay(5);
    }
    udp.stop();
    kasa.stop();
    incorporarTuyaVistos();

    // 3. Recoger resultados mDNS
    for (int i = 0; i < MDNS_CANTIDAD; i++) {
        if (!busquedas[i]) continue;
        mdns_result_t* resultados = nullptr;
        mdns_query_async_get_results(busquedas[i], 500, &resultados);
        for (mdns_result_t* r = resultados; r; r = r->next) {
            for (mdns_ip_addr_t* a = r->addr; a; a = a->next) {
                if (a->addr.type != ESP_IPADDR_TYPE_V4) continue;
                DispositivoRed* d = redBuscarOCrear(IPAddress(a->addr.u_addr.ip4.addr));
                if (!d) break;
                String nombre = r->instance_name ? r->instance_name : (r->hostname ? r->hostname : "");
                for (size_t t = 0; t < r->txt_count; t++)   // Google Cast publica el nombre visible en "fn"
                    if (r->txt[t].key && r->txt[t].value && strcmp(r->txt[t].key, "fn") == 0) nombre = r->txt[t].value;
                if (nombre.length() && !d->nombre.length()) d->nombre = nombre.substring(0, 48);
                // Los servicios específicos describen mejor que la cabecera SERVER de SSDP
                if (strcmp(MDNS_SERVICIOS[i][0], "_http") != 0 || !d->tipo.length()) d->tipo = MDNS_SERVICIOS[i][2];
                break;
            }
        }
        if (resultados) mdns_query_results_free(resultados);
        mdns_query_async_delete(busquedas[i]);
    }

    // 4. Nombre y fabricante desde la descripción UPnP (máx. 8 para acotar el tiempo)
    int consultas = 0;
    for (int i = 0; i < redCantidad && consultas < 8; i++) {
        if (redDispositivos[i].descripcion.length()) { leerDescripcionUpnp(redDispositivos[i]); consultas++; }
    }

    redEscaneadoEn = millis();
    redEscaneada = true;
    Serial.printf("[Red] %d dispositivos en %lu ms\n", redCantidad, millis() - inicio);
    for (int i = 0; i < redCantidad; i++) {
        DispositivoRed& d = redDispositivos[i];
        Serial.printf("  %s  %s | %s | %s\n", d.ip.toString().c_str(), d.nombre.c_str(), d.tipo.c_str(), d.fabricante.c_str());
    }
}

void asegurarEscaneoRed() {
    if (!redEscaneada || millis() - redEscaneadoEn > RED_CACHE_MS) escanearRed();
}

// Texto compacto para el prompt de Gemini
String resumenRed() {
    if (!redCantidad) return "ningun dispositivo respondio por SSDP ni mDNS";
    IPAddress puerta = WiFi.gatewayIP();
    String s;
    for (int i = 0; i < redCantidad; i++) {
        DispositivoRed& d = redDispositivos[i];
        if (s.length()) s += "; ";
        s += d.ip.toString() + " ";
        s += d.nombre.length() ? d.nombre : String("sin nombre");
        if (d.tipo.length()) s += " (" + d.tipo + ")";
        if (d.fabricante.length()) s += " de " + d.fabricante;
        if (d.ip == puerta) s += " [router]";
    }
    return s;
}

// La pregunta trata sobre lo que hay conectado en la red
bool preguntaPorRed(String m) {
    m.toLowerCase();
    const char* claves[] = {"dispositiv", "aparato", "escane", "iot", "conectad", "detect",
                            "en la red", "de la red", "mi red", "la red"};
    for (const char* c : claves) if (m.indexOf(c) >= 0) return true;
    return false;
}
