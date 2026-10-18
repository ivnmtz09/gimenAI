#pragma once
// ------------------------------------------------------------------------------
// Configuración WiFi sin cables: si Gimena no logra conectarse, crea su propia
// red (punto de acceso), muestra cara triste + QR en la OLED y sirve una página
// donde se escanean las redes cercanas y se guardan las credenciales.
// Se incluye desde main_test.cpp (usa display, localServer y dibujarArco).
// ------------------------------------------------------------------------------
#include <DNSServer.h>
#include <esp_netif.h>
#include <dhcpserver/dhcpserver.h>
#include <qrcode.h>
#include "wifi_setup_page.h"

DNSServer portalDns;
Preferences wifiPrefs;
unsigned long portalReinicioEn = 0;
String portalRedFallida;   // última red que no se pudo conectar (se muestra en la página)

// QR para unirse a la red de Gimena; al entrar, el celular abre el portal solo.
const uint8_t QR_VERSION = 1;   // 21x21 módulos -> 63 px a 3 px por módulo
QRCode qrRed;
uint8_t qrRedDatos[56];

bool conectarWiFi(const String& ssid, const String& clave, unsigned long esperaMs) {
    if (ssid.length() == 0) return false;
    Serial.printf("Conectando a WiFi: %s", ssid.c_str());
    WiFi.begin(ssid.c_str(), clave.c_str());
    unsigned long inicio = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicio < esperaMs) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    return WiFi.status() == WL_CONNECTED;
}

// Primero la red guardada desde el portal; si falla, la de secrets.h.
bool conectarRedConocida() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(200);

    wifiPrefs.begin("wifi", true);
    String ssid = wifiPrefs.getString("ssid", "");
    String clave = wifiPrefs.getString("pass", "");
    wifiPrefs.end();

    if (ssid.length() > 0) {
        if (conectarWiFi(ssid, clave, 15000)) return true;
        portalRedFallida = ssid;
        WiFi.disconnect(true);
        delay(200);
    }
    if (ssid != TEST_WIFI_SSID && conectarWiFi(TEST_WIFI_SSID, TEST_WIFI_PASSWORD, 15000)) return true;
    if (portalRedFallida.length() == 0) portalRedFallida = TEST_WIFI_SSID;
    return false;
}

// ------------------------------------------------------------------------------
// Pantalla fija (sin animación) para que la cámara enfoque el QR.
// El QR ocupa toda la altura; se dibuja invertido (módulos encendidos sobre
// negro) porque el borde negro de la OLED sirve como zona de silencio.
// ------------------------------------------------------------------------------
void dibujarPantallaSinWiFi() {
    const int escala = 3;
    const int lado = qrRed.size * escala;
    const int x0 = 1, y0 = (SCREEN_HEIGHT - lado) / 2;
    for (int y = 0; y < qrRed.size; y++)
        for (int x = 0; x < qrRed.size; x++)
            if (qrcode_getModule(&qrRed, x, y))
                display.fillRect(x0 + x * escala, y0 + y * escala, escala, escala, SSD1306_WHITE);

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    const int tx = x0 + lado + 6;
    display.setCursor(tx, 4);  display.print("Sin WiFi");
    display.setCursor(tx, 22); display.print("Escanea");
    display.setCursor(tx, 32); display.print("para");
    display.setCursor(tx, 42); display.print("conectar");
    display.setCursor(tx, 56); display.print(PORTAL_AP_SSID);
}

// Secuencia al quedarse sin red: mensaje, cara triste y luego el QR fijo.
void mostrarSecuenciaSinWiFi() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(16, 16); display.print("Sin WiFi");
    display.setTextSize(1);
    display.setCursor(10, 44); display.print("No estoy conectada");
    display.display();
    delay(2500);

    display.clearDisplay();
    dibujarCara("sad", false);
    display.display();
    delay(2500);

    portalWiFiActivo = true;   // a partir de aquí dibujarPantalla() muestra el QR
    dibujarPantalla();
}

// ------------------------------------------------------------------------------
// Rutas del portal
// ------------------------------------------------------------------------------
void handlePortalPagina() {
    localServer.send_P(200, "text/html", WIFI_SETUP_PAGE);
}

// Escaneo asíncrono: la primera llamada lo inicia y las siguientes devuelven el resultado.
void handleWifiEscaneo() {
    int n = WiFi.scanComplete();
    JsonDocument doc;
    doc["failed"] = portalRedFallida;
    doc["current"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "";
    if (n == WIFI_SCAN_FAILED || (localServer.hasArg("nuevo") && n >= 0)) {
        if (n >= 0) WiFi.scanDelete();
        WiFi.scanNetworks(true);
        doc["scanning"] = true;
    } else if (n == WIFI_SCAN_RUNNING) {
        doc["scanning"] = true;
    } else {
        doc["scanning"] = false;
        JsonArray redes = doc["networks"].to<JsonArray>();
        // Una entrada por nombre (la de mejor señal); los resultados ya vienen ordenados por RSSI
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) continue;
            bool repetida = false;
            for (JsonObject r : redes) if (r["ssid"] == ssid) { repetida = true; break; }
            if (repetida) continue;
            JsonObject r = redes.add<JsonObject>();
            r["ssid"] = ssid;
            r["rssi"] = WiFi.RSSI(i);
            r["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        }
    }
    enviarJson(doc);
}

void handleWifiConectar() {
    JsonDocument body;
    if (!localServer.hasArg("plain") || deserializeJson(body, localServer.arg("plain")) ||
        !body["ssid"].is<const char*>()) {
        localServer.send(400, "application/json", "{\"error\":\"Datos inválidos\"}");
        return;
    }
    String ssid = body["ssid"].as<String>();
    String clave = body["password"] | "";
    ssid.trim();
    if (ssid.length() == 0 || ssid.length() > 32 || clave.length() > 64) {
        localServer.send(400, "application/json", "{\"error\":\"Nombre o clave con longitud inválida\"}");
        return;
    }
    wifiPrefs.begin("wifi", false);
    wifiPrefs.putString("ssid", ssid);
    wifiPrefs.putString("pass", clave);
    wifiPrefs.end();
    Serial.printf("[WiFi] Red guardada: %s. Reiniciando...\n", ssid.c_str());
    localServer.send(200, "application/json", "{\"ok\":true}");
    portalReinicioEn = millis() + 2000;   // deja salir la respuesta antes de reiniciar
}

// Cualquier URL desconocida lleva a la página: así el celular abre el portal solo.
// Con cuerpo no vacío (evita un log_w por petición, que bloquea el serial) y
// un resumen periódico en lugar de imprimir cada petición.
uint32_t portalRedirecciones = 0;
unsigned long portalUltimoResumen = 0;

void handlePortalRedireccion() {
    String destino = String("http://") + WiFi.softAPIP().toString() + "/";
    localServer.sendHeader("Location", destino, true);
    localServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    localServer.send(302, "text/html", "<a href=\"" + destino + "\">Conectar a Gimena</a>");

    portalRedirecciones++;
    String uri = localServer.uri();
    bool sonda = uri.indexOf("generate_204") >= 0 || uri.indexOf("gen_204") >= 0 ||
                 uri.indexOf("hotspot") >= 0 || uri.indexOf("connecttest") >= 0;
    if (sonda) Serial.printf("[Portal] Sonda %s%s -> portal\n", localServer.hostHeader().c_str(), uri.c_str());
    if (millis() - portalUltimoResumen > 5000) {
        portalUltimoResumen = millis();
        Serial.printf("[Portal] %u redirecciones (clientes conectados: %d)\n",
                      portalRedirecciones, WiFi.softAPgetStationNum());
    }
}

// El core 2.0.x no anuncia DNS por DHCP en el AP: sin esto el celular no puede
// resolver su URL de comprobación y nunca abre el portal automáticamente.
void anunciarDnsDelPortal(IPAddress ip) {
    esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap) return;
    esp_netif_dhcps_stop(ap);
    esp_netif_dns_info_t dns = {};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(ip);
    esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &dns);
    dhcps_offer_t ofrecerDns = OFFER_DNS;
    esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &ofrecerDns, sizeof(ofrecerDns));
    esp_netif_dhcps_start(ap);
}

void iniciarPortalWiFi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP_STA);   // AP para el celular + STA para poder escanear
    WiFi.softAP(PORTAL_AP_SSID);
    delay(100);
    // IP pública ficticia: Android (sobre todo Samsung) descarta el portal si su
    // sonda se resuelve a una IP privada como 192.168.4.1 y marca "sin internet".
    IPAddress ip(4, 3, 2, 1);
    WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
    anunciarDnsDelPortal(ip);
    portalDns.start(53, "*", ip);

    // Red abierta: "WIFI:S:<nombre>;;" cabe en un QR versión 1
    String qrTextoRed = String("WIFI:S:") + PORTAL_AP_SSID + ";;";
    qrcode_initText(&qrRed, qrRedDatos, QR_VERSION, ECC_LOW, qrTextoRed.c_str());

    localServer.on("/", handlePortalPagina);
    localServer.on("/api/wifi/scan", HTTP_GET, handleWifiEscaneo);
    localServer.on("/api/wifi/connect", HTTP_POST, handleWifiConectar);
    localServer.onNotFound(handlePortalRedireccion);
    localServer.begin();
    WiFi.scanNetworks(true);   // primer escaneo listo para cuando abran la página

    Serial.printf("[Portal] Sin WiFi. Conéctate a la red abierta '%s' y abre http://%s/\n",
                  PORTAL_AP_SSID, ip.toString().c_str());
    mostrarSecuenciaSinWiFi();
}

void atenderPortalWiFi() {
    if (!portalWiFiActivo && portalReinicioEn == 0) return;
    // Atender todas las consultas DNS en cola: si se retrasan, Android da la red por "sin internet"
    if (portalWiFiActivo) for (int i = 0; i < 16; i++) portalDns.processNextRequest();
    if (portalReinicioEn && (long)(millis() - portalReinicioEn) >= 0) ESP.restart();
}
