#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <WebServer.h>

#include "test_config.h"

// ------------------------------------------------------------------------------
// Pantalla OLED GM009605v4.3 (SSD1306 128x64 I2C)
// ------------------------------------------------------------------------------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Servidor Web local en el ESP32 (puerto 80)
WebServer localServer(80);

// Estado de los dispositivos IoT
bool estadoLuzSala = false;       // LED integrado GPIO 2
bool estadoLuzCocina = false;     // GPIO 4 simulado
bool estadoVentilador = false;    // GPIO 5 simulado

String ultimaRespuesta = "Gimena lista. Escribe en Serial o abre mi IP.";
String caraActual = "happy"; // happy, neutral, thinking, surprised

// Buffer para entrada serial
String serialInput = "";

// ------------------------------------------------------------------------------
// Renderizado en Pantalla OLED
// ------------------------------------------------------------------------------
void renderOLED(const String& cara, const String& mensaje) {
    display.clearDisplay();

    // 1. Barra superior de estado (0 - 12 px)
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("WiFi:");
    display.print(WiFi.status() == WL_CONNECTED ? "OK" : "NO");

    // Indicador del Bombillo de la Sala (LED GPIO 2)
    display.setCursor(55, 0);
    display.print("S:");
    display.print(estadoLuzSala ? "[ON]" : "[--]");

    display.setCursor(96, 0);
    display.print("C:");
    display.print(estadoLuzCocina ? "[ON]" : "[--]");

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // 2. Expresión / Cara de Gimena (13 - 26 px)
    display.setCursor(0, 14);
    display.setTextSize(1);
    if (cara == "happy") {
        display.print("(^.^) Gimena:");
    } else if (cara == "thinking") {
        display.print("(~.~) Gimena [Pensando...]");
    } else if (cara == "surprised") {
        display.print("(O.O) Gimena:");
    } else {
        display.print("(o.o) Gimena:");
    }

    // 3. Texto del mensaje (27 - 63 px)
    display.setCursor(0, 26);
    display.setTextSize(1);
    String textoVisible = mensaje;
    if (textoVisible.length() > 95) {
        textoVisible = textoVisible.substring(0, 92) + "...";
    }
    display.println(textoVisible);

    display.display();
}

// ------------------------------------------------------------------------------
// Actualizar Hardware Físico de Dispositivos
// ------------------------------------------------------------------------------
void actualizarHardware() {
    // Bombillo de la sala: LED integrado en la placa ESP32 (GPIO 2)
    digitalWrite(PIN_DEVICE_LUZ_SALA, estadoLuzSala ? HIGH : LOW);
    digitalWrite(PIN_DEVICE_LUZ_COCINA, estadoLuzCocina ? HIGH : LOW);
    digitalWrite(PIN_DEVICE_VENTILADOR, estadoVentilador ? HIGH : LOW);
}

// ------------------------------------------------------------------------------
// Anunciar ESP32 al Servidor Central
// ------------------------------------------------------------------------------
void registrarConServidor() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + "/api/devices/register";
    Serial.printf("\n[Auto-Descubrimiento] Anunciando ESP32 en %s...\n", url.c_str());

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["id"] = "esp32_devkit";
    doc["name"] = "ESP32 DevKit V1 - Bombillo Sala";
    doc["ip"] = WiFi.localIP().toString();
    JsonArray caps = doc["capabilities"].to<JsonArray>();
    caps.add("luz_sala");
    caps.add("luz_cocina");
    caps.add("ventilador");

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    if (code == HTTP_CODE_OK) {
        Serial.println("¡ESP32 registrado exitosamente en el servidor de Gimena!");
    } else {
        Serial.printf("Aviso: El servidor respondió con código %d al registrarse.\n", code);
    }
    http.end();
}

// ------------------------------------------------------------------------------
// Consulta al Servidor de Gimena (Chat Real)
// ------------------------------------------------------------------------------
void consultarGimena(const String& mensajeUsuario) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n[Error]: WiFi no conectado.");
        renderOLED("neutral", "Error: WiFi desconectado.");
        return;
    }

    renderOLED("thinking", "Procesando con Gemini...");

    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + "/api/chat";
    Serial.printf("\n[Tú]: %s\n", mensajeUsuario.c_str());

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(12000);

    JsonDocument reqDoc;
    reqDoc["message"] = mensajeUsuario;
    String reqPayload;
    serializeJson(reqDoc, reqPayload);

    int httpCode = http.POST(reqPayload);

    if (httpCode == HTTP_CODE_OK) {
        String respPayload = http.getString();
        JsonDocument respDoc;
        DeserializationError err = deserializeJson(respDoc, respPayload);

        if (!err) {
            ultimaRespuesta = respDoc["reply"].as<String>();
            caraActual = respDoc["face"] | "happy";

            if (respDoc["devices"].is<JsonObject>()) {
                JsonObject devs = respDoc["devices"].as<JsonObject>();
                if (devs.containsKey("luz_sala")) estadoLuzSala = devs["luz_sala"];
                if (devs.containsKey("luz_cocina")) estadoLuzCocina = devs["luz_cocina"];
                if (devs.containsKey("ventilador")) estadoVentilador = devs["ventilador"];
            }

            actualizarHardware();

            Serial.printf("[Gimena (%s)]: %s\n", caraActual.c_str(), ultimaRespuesta.c_str());
            Serial.printf("-> Estado Físico: Bombillo Sala (LED GPIO 2) = %s\n\n",
                          estadoLuzSala ? "ENCENDIDO [ON]" : "APAGADO [OFF]");

            renderOLED(caraActual, ultimaRespuesta);
        } else {
            Serial.println("[Error]: Error deserializando JSON.");
            renderOLED("neutral", "Error en respuesta JSON.");
        }
    } else {
        Serial.printf("[Error HTTP]: Código %d. Asegúrate que 'python3 main.py' esté corriendo.\n", httpCode);
        renderOLED("neutral", "Servidor no responde.");
    }

    http.end();
}

// ------------------------------------------------------------------------------
// Endpoints HTTP en el ESP32 para Control en Tiempo Real desde el Servidor
// ------------------------------------------------------------------------------
void handleApiDevice() {
    if (localServer.hasArg("plain")) {
        String body = localServer.arg("plain");
        JsonDocument doc;
        if (!deserializeJson(doc, body)) {
            const char* dev = doc["device"] | "";
            bool state = doc["state"] | false;

            if (strcmp(dev, "luz_sala") == 0) estadoLuzSala = state;
            else if (strcmp(dev, "luz_cocina") == 0) estadoLuzCocina = state;
            else if (strcmp(dev, "ventilador") == 0) estadoVentilador = state;

            actualizarHardware();
            ultimaRespuesta = String("Dispositivo ") + dev + (state ? " encendido." : " apagado.");
            renderOLED(state ? "happy" : "neutral", ultimaRespuesta);

            Serial.printf("[Orden Remota]: %s -> %s\n", dev, state ? "ON" : "OFF");
            localServer.send(200, "application/json", "{\"status\":\"ok\"}");
            return;
        }
    }
    localServer.send(400, "application/json", "{\"status\":\"error\"}");
}

void handleWebRoot() {
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<title>GimenAI ESP32</title>"
                  "<style>"
                  "body { font-family: sans-serif; background: #0f172a; color: #f8fafc; padding: 15px; margin: 0; }"
                  "h2 { color: #38bdf8; text-align: center; margin-bottom: 5px; }"
                  ".card { background: #1e293b; border-radius: 10px; padding: 15px; margin-bottom: 15px; }"
                  ".btn { background: #0284c7; color: white; border: none; padding: 10px 14px; border-radius: 6px; cursor: pointer; font-weight: bold; width: 100%; margin-top: 8px; }"
                  "input[type=text] { width: 100%; padding: 12px; box-sizing: border-box; background: #020617; color: white; border: 1px solid #334155; border-radius: 6px; font-size: 16px; }"
                  ".msg { background: #334155; padding: 10px; border-radius: 8px; margin-top: 10px; }"
                  "</style></head><body>"
                  "<h2>🤖 Gimena (ESP32 DevKit)</h2>"
                  "<div class='card'>"
                  "<h3>💡 Estado de Bombillos</h3>"
                  "<p>Bombillo Sala (LED GPIO 2): <b>" + String(estadoLuzSala ? "ENCENDIDO [ON]" : "APAGADO [OFF]") + "</b></p>"
                  "<p>Bombillo Cocina: <b>" + String(estadoLuzCocina ? "ENCENDIDO [ON]" : "APAGADO [OFF]") + "</b></p>"
                  "</div>"
                  "<div class='card'>"
                  "<h3>💬 Chat con Gimena</h3>"
                  "<form action='/chat' method='GET'>"
                  "<input type='text' name='q' placeholder='Ej: Enciende el bombillo de la sala' autofocus>"
                  "<button class='btn' type='submit'>Enviar Instrucción</button>"
                  "</form>";
    if (ultimaRespuesta.length() > 0) {
        html += "<div class='msg'><b>Gimena (" + caraActual + "):</b> " + ultimaRespuesta + "</div>";
    }
    html += "</div></body></html>";
    localServer.send(200, "text/html", html);
}

void handleWebChat() {
    if (localServer.hasArg("q")) {
        String query = localServer.arg("q");
        consultarGimena(query);
    }
    localServer.sendHeader("Location", "/");
    localServer.send(303);
}

// ------------------------------------------------------------------------------
// Setup
// ------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n==============================================");
    Serial.println("  GimenAI - ESP32 DEVKIT V1 & OLED GM009605   ");
    Serial.println("==============================================");

    // 1. Configurar Pines de Salida
    pinMode(PIN_DEVICE_LUZ_SALA, OUTPUT);
    pinMode(PIN_DEVICE_LUZ_COCINA, OUTPUT);
    pinMode(PIN_DEVICE_VENTILADOR, OUTPUT);
    actualizarHardware();

    // 2. Inicializar Pantalla OLED GM009605v4.3 (SSD1306)
    Wire.begin(OLED_PIN_SDA, OLED_PIN_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
        display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(15, 20);
    display.println("Iniciando Gimena...");
    display.setCursor(20, 35);
    display.println("Conectando WiFi");
    display.display();

    // 3. Conexión WiFi
    Serial.printf("Conectando a WiFi: %s", TEST_WIFI_SSID);
    WiFi.begin(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);

    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 25) {
        delay(500);
        Serial.print(".");
        intentos++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi Conectado! IP del ESP32: %s\n", WiFi.localIP().toString().c_str());
        ultimaRespuesta = "Conectado. IP: " + WiFi.localIP().toString();
        renderOLED("happy", ultimaRespuesta);

        // Servidor Web del ESP32
        localServer.on("/", handleWebRoot);
        localServer.on("/chat", handleWebChat);
        localServer.on("/api/device", HTTP_POST, handleApiDevice);
        localServer.begin();

        // Anunciar este ESP32 al servidor de Gimena
        registrarConServidor();
    } else {
        Serial.println("\nFallo al conectar WiFi. Revisa test_config.h.");
        ultimaRespuesta = "Sin WiFi. Revisa config.";
        renderOLED("neutral", ultimaRespuesta);
    }

    Serial.println("\n---------------------------------------------------------");
    Serial.println("¡Listo! Puedes escribir tu mensaje en este Monitor Serial:");
    Serial.println("Ejemplos:");
    Serial.println(" - Hola Gimena, me llamo Iván");
    Serial.println(" - ¿Qué dispositivos inteligentes detectas?");
    Serial.println(" - Enciende el bombillo de la sala");
    Serial.println("---------------------------------------------------------\n");
}

// ------------------------------------------------------------------------------
// Bucle Principal (Loop)
// ------------------------------------------------------------------------------
void loop() {
    localServer.handleClient();

    // Leer entrada del Monitor Serial USB
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            serialInput.trim();
            if (serialInput.length() > 0) {
                consultarGimena(serialInput);
                serialInput = "";
            }
        } else {
            serialInput += c;
        }
    }
}
