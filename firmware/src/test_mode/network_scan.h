#pragma once
// ------------------------------------------------------------------------------
// Escaneo de dispositivos IoT en la red local. Combina en paralelo:
//   - SSDP/UPnP (TVs, Echo, Chromecast, routers, Hue...) + su descripción XML
//   - mDNS/DNS-SD (Google Cast, HomeKit, ESPHome, impresoras, AirPlay...)
// El resultado se guarda unos minutos para no repetir el escaneo en cada mensaje.
// ------------------------------------------------------------------------------
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <mdns.h>

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

void escanearRed() {
    Serial.println("[Red] Escaneando dispositivos IoT (SSDP + mDNS)...");
    unsigned long inicio = millis();
    redCantidad = 0;

    // 1. Lanzar todas las búsquedas mDNS a la vez (corren en segundo plano)
    mdns_search_once_t* busquedas[MDNS_CANTIDAD] = {};
    for (int i = 0; i < MDNS_CANTIDAD; i++)
        busquedas[i] = mdns_query_async_new(nullptr, MDNS_SERVICIOS[i][0], MDNS_SERVICIOS[i][1],
                                            MDNS_TYPE_PTR, 2500, 8, nullptr);

    // 2. SSDP mientras tanto
    WiFiUDP udp;
    if (udp.begin(41900)) {
        const char* msearch =
            "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: ssdp:all\r\n\r\n";
        for (int intento = 0; intento < 2; intento++) {   // UDP puede perderse: se envía dos veces
            udp.beginPacket(IPAddress(239, 255, 255, 250), 1900);
            udp.write((const uint8_t*)msearch, strlen(msearch));
            udp.endPacket();
            delay(50);
        }
        unsigned long fin = millis() + 2800;
        char paquete[700];
        while (millis() < fin) {
            int n = udp.parsePacket();
            if (n > 0) {
                int leidos = udp.read(paquete, sizeof(paquete) - 1);
                paquete[max(leidos, 0)] = 0;
                String respuesta = String("\n") + paquete;
                DispositivoRed* d = redBuscarOCrear(udp.remoteIP());
                if (d) {
                    if (!d->descripcion.length()) d->descripcion = cabeceraSsdp(respuesta, "location");
                    if (!d->tipo.length()) d->tipo = cabeceraSsdp(respuesta, "server").substring(0, 48);
                }
            } else {
                serviceStandalone();
                delay(5);
            }
        }
        udp.stop();
    }

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
