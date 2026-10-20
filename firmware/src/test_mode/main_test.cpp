#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <uri/UriBraces.h>
#include <time.h>
#include "standalone_cloud.h"
#include "tuya_cloud.h"
#include "web_panel.h"
#include <driver/i2s.h>
#include <Preferences.h>
#include <math.h>

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

// Estado del chat y voz locales, sin servidor del PC.
bool chatPending = false, chatRunning = false, voicePending = false;
uint32_t chatJob = 0;
unsigned long voiceReadyAt = 0;
String pendingMessage, jobReply, jobError, voiceError;

// Buffer para entrada serial
String serialInput = "";

// ------------------------------------------------------------------------------
// Cara kawaii en Pantalla OLED (128x64 monocromática, sin texto)
//   Caras: neutral, happy, love, surprised, sad, thinking, sleepy
// ------------------------------------------------------------------------------
const int FACE_CY      = 22;   // altura de los ojos
const int FACE_EYE_L   = 38;
const int FACE_EYE_R   = 90;
const int FACE_MOUTH_Y = 45;

String oledCara = "neutral";
bool oledParpadeo = false;
bool oledBocaAbierta = false;
unsigned long oledProxParpadeo = 0;
unsigned long oledFinParpadeo = 0;
unsigned long oledUltimoFrame = 0;
unsigned long oledHablandoHasta = 0;
volatile bool audioReproduciendo = false;   // true mientras suena la voz (la boca se mueve)
int oledFrame = 0;

// Arco de círculo (ángulos en grados, eje Y hacia abajo: 0-180 = mitad inferior)
void dibujarArco(int cx, int cy, int r, int a0, int a1, int grosor) {
    for (int a = a0; a <= a1; a += 3) {
        float rad = a * PI / 180.0f;
        int x = cx + (int)roundf(cosf(rad) * r);
        int y = cy + (int)roundf(sinf(rad) * r);
        display.fillRect(x - grosor / 2, y - grosor / 2, grosor, grosor, SSD1306_WHITE);
    }
}

// Elipse punteada (efecto "rubor" en las mejillas)
void dibujarMejilla(int x, int y, int w, int h) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            float dx = (i - (w - 1) / 2.0f) / (w / 2.0f);
            float dy = (j - (h - 1) / 2.0f) / (h / 2.0f);
            if (dx * dx + dy * dy <= 1.0f && ((i + j) & 1) == 0) {
                display.drawPixel(x + i, y + j, SSD1306_WHITE);
            }
        }
    }
}

void dibujarMejillas() {
    dibujarMejilla(6, 40, 18, 9);
    dibujarMejilla(104, 40, 18, 9);
}

// Brillo de 4 puntas
void dibujarBrillo(int x, int y, int s) {
    display.drawFastHLine(x - s, y, 2 * s + 1, SSD1306_WHITE);
    display.drawFastVLine(x, y - s, 2 * s + 1, SSD1306_WHITE);
}

// Ojo grande de anime: óvalo blanco, pupila negra y dos brillos
void dibujarOjo(int cx, int px, int py, bool cerrado) {
    const int cy = FACE_CY;
    if (cerrado) {
        dibujarArco(cx, cy - 3, 9, 25, 155, 3);
        return;
    }
    display.fillRoundRect(cx - 10, cy - 14, 20, 28, 9, SSD1306_WHITE);
    display.fillRoundRect(cx - 6 + px, cy - 9 + py, 12, 19, 6, SSD1306_BLACK);
    display.fillCircle(cx - 2 + px, cy - 5 + py, 3, SSD1306_WHITE);
    display.fillCircle(cx + 3 + px, cy + 5 + py, 1, SSD1306_WHITE);
}

void dibujarOjoFeliz(int cx) {
    dibujarArco(cx, FACE_CY + 5, 10, 200, 340, 3);   // ^
}

void dibujarOjoSorpresa(int cx) {
    display.fillCircle(cx, FACE_CY, 14, SSD1306_WHITE);
    display.fillCircle(cx, FACE_CY, 6, SSD1306_BLACK);
    display.fillCircle(cx - 2, FACE_CY - 2, 2, SSD1306_WHITE);
}

void dibujarCorazon(int cx, int cy, int s) {
    display.fillCircle(cx - s / 2, cy - s / 4, s / 2 + 1, SSD1306_WHITE);
    display.fillCircle(cx + s / 2, cy - s / 4, s / 2 + 1, SSD1306_WHITE);
    display.fillTriangle(cx - s - 1, cy - s / 6, cx + s + 1, cy - s / 6, cx, cy + s + 1, SSD1306_WHITE);
    display.fillCircle(cx - s / 2, cy - s / 2, 1, SSD1306_BLACK);   // brillo
}

// Boca de gatito "w"
void dibujarBocaGato(int cx, int y) {
    dibujarArco(cx - 4, y, 4, 0, 180, 2);
    dibujarArco(cx + 4, y, 4, 0, 180, 2);
}

// Boca abierta (medio círculo) con lengüita
void dibujarBocaAbierta(int cx, int y) {
    display.fillCircle(cx, y, 7, SSD1306_WHITE);
    display.fillRect(cx - 8, y - 8, 17, 8, SSD1306_BLACK);
    display.fillCircle(cx, y + 6, 3, SSD1306_BLACK);
    display.drawFastHLine(cx - 2, y + 4, 5, SSD1306_BLACK);
}

void dibujarCara(const String& cara, bool parpadeo) {
    const int xi = FACE_EYE_L, xd = FACE_EYE_R;
    const int my = FACE_MOUTH_Y;

    if (cara == "happy") {
        dibujarOjoFeliz(xi);
        dibujarOjoFeliz(xd);
        dibujarMejillas();
        if (oledBocaAbierta) dibujarBocaGato(64, my);
        else dibujarBocaAbierta(64, my - 1);
        if (oledFrame % 2 == 0) { dibujarBrillo(14, 12, 3); dibujarBrillo(114, 16, 2); }
        else                    { dibujarBrillo(14, 12, 2); dibujarBrillo(114, 16, 3); }

    } else if (cara == "love") {
        int s = (oledFrame % 2 == 0) ? 8 : 10;           // latido
        dibujarCorazon(xi, FACE_CY, s);
        dibujarCorazon(xd, FACE_CY, s);
        dibujarMejillas();
        if (oledBocaAbierta) dibujarBocaAbierta(64, my - 1);
        else dibujarBocaGato(64, my);

    } else if (cara == "surprised") {
        dibujarOjoSorpresa(xi);
        dibujarOjoSorpresa(xd);
        dibujarMejillas();
        display.drawCircle(64, my + 2, 4, SSD1306_WHITE);
        if (oledBocaAbierta) display.drawCircle(64, my + 2, 5, SSD1306_WHITE);

    } else if (cara == "sad") {
        dibujarOjo(xi, 0, 3, parpadeo);
        dibujarOjo(xd, 0, 3, parpadeo);
        display.drawLine(24, 9, 46, 2, SSD1306_WHITE);   // cejas caídas
        display.drawLine(24, 10, 46, 3, SSD1306_WHITE);
        display.drawLine(82, 2, 104, 9, SSD1306_WHITE);
        display.drawLine(82, 3, 104, 10, SSD1306_WHITE);
        dibujarArco(64, my + 5, 6, 205, 335, 2);         // boca triste
        int ly = 36 + (oledFrame % 3) * 3;               // lagrimita que cae
        display.fillCircle(104, ly + 3, 2, SSD1306_WHITE);
        display.fillTriangle(104, ly - 3, 101, ly + 2, 107, ly + 2, SSD1306_WHITE);

    } else if (cara == "thinking") {
        dibujarOjo(xi, 3, -3, parpadeo);
        dibujarOjo(xd, 3, -3, parpadeo);
        dibujarBocaGato(72, my);
        if (oledFrame % 4 >= 1) display.fillCircle(112, 40, 1, SSD1306_WHITE);
        if (oledFrame % 4 >= 2) display.fillCircle(117, 32, 2, SSD1306_WHITE);
        if (oledFrame % 4 >= 3) display.fillCircle(121, 20, 3, SSD1306_WHITE);

    } else if (cara == "sleepy") {
        dibujarOjo(xi, 0, 0, true);
        dibujarOjo(xd, 0, 0, true);
        dibujarMejillas();
        display.drawCircle(64, my + 1, 3, SSD1306_WHITE);
        // "Z" que sube
        int zy = (oledFrame % 2 == 0) ? 4 : 8;
        display.drawFastHLine(106, zy, 9, SSD1306_WHITE);
        display.drawLine(114, zy, 106, zy + 8, SSD1306_WHITE);
        display.drawFastHLine(106, zy + 8, 9, SSD1306_WHITE);

    } else { // neutral
        dibujarOjo(xi, 0, 0, parpadeo);
        dibujarOjo(xd, 0, 0, parpadeo);
        dibujarMejillas();
        if (oledBocaAbierta) dibujarBocaAbierta(64, my - 1);
        else dibujarBocaGato(64, my);
    }
}

#include "oled_animaciones.h"

bool portalWiFiActivo = false;   // sin red: la OLED muestra el QR del portal WiFi
void dibujarPantallaSinWiFi();

void dibujarPantalla() {
    display.clearDisplay();
    if (portalWiFiActivo) {
        dibujarPantallaSinWiFi();
        display.display();
        return;
    }
    if (animacionActiva()) {
        dibujarAnimacion();
        display.display();
        return;
    }
    invertirPantalla(false);
    dibujarCara(oledCara, oledParpadeo);

    // Único indicador: circulito vacío arriba a la izquierda si no hay WiFi
    if (WiFi.status() != WL_CONNECTED) display.drawCircle(3, 3, 2, SSD1306_WHITE);

    display.display();
}

// Muestra una cara. Si 'hablar' es true, mueve la boca mientras "habla" el mensaje.
// (El texto ya no se dibuja: solo se usa para estimar cuánto dura el habla.)
void renderOLED(const String& cara, const String& mensaje, bool hablar = false) {
    oledCara = cara;
    detenerAnimacion();
    oledParpadeo = false;
    oledBocaAbierta = false;
    oledProxParpadeo = millis() + 2500;
    oledFrame = 0;
    oledHablandoHasta = hablar ? millis() + constrain((int)mensaje.length() * 55, 800, 6000) : 0;
    dibujarPantalla();
}

// Animaciones no bloqueantes: pestañeo, boca hablando, burbujas, latido y lágrima
void actualizarPantalla() {
    if (portalWiFiActivo) return;   // QR fijo: no redibujar mientras se escanea
    unsigned long ahora = millis();
    bool redibujar = false;

    if (oledAnim.length()) {                     // animación de pantalla completa (~15 cuadros/s)
        if (!animacionActiva()) { detenerAnimacion(); dibujarPantalla(); return; }
        if (ahora - oledUltimoFrame >= 66) { oledUltimoFrame = ahora; dibujarPantalla(); }
        return;
    }

    bool pestanea = (oledCara == "neutral" || oledCara == "sad" || oledCara == "thinking");
    if (pestanea) {
        if (!oledParpadeo && ahora >= oledProxParpadeo) {
            oledParpadeo = true;
            oledFinParpadeo = ahora + 150;
            redibujar = true;
        } else if (oledParpadeo && ahora >= oledFinParpadeo) {
            oledParpadeo = false;
            oledProxParpadeo = ahora + random(2500, 5000);
            redibujar = true;
        }
    }

    bool hablando = audioReproduciendo || ahora < oledHablandoHasta;
    if (hablando) {
        if (ahora - oledUltimoFrame >= 170) {
            oledUltimoFrame = ahora;
            oledBocaAbierta = !oledBocaAbierta;
            oledFrame++;
            redibujar = true;
        }
    } else {
        if (oledBocaAbierta) { oledBocaAbierta = false; redibujar = true; }
        bool animada = (oledCara == "thinking" || oledCara == "love" || oledCara == "sad" ||
                        oledCara == "sleepy" || oledCara == "happy");
        unsigned long periodo = (oledCara == "thinking") ? 400 : 700;
        if (animada && ahora - oledUltimoFrame >= periodo) {
            oledUltimoFrame = ahora;
            oledFrame++;
            redibujar = true;
        }
    }

    if (redibujar) dibujarPantalla();
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
// Voz: descarga el WAV (16 kHz, mono, 16 bit) del servidor y lo reproduce por I2S
//   Corre en una tarea aparte (núcleo 0) para que la cara y el servidor web
//   del ESP32 sigan animándose sin cortar el audio.
// ------------------------------------------------------------------------------
bool audioListo = false;

void iniciarAudio() {
    // API del core Arduino 2.x que instala la plataforma oficial de PlatformIO.
    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = AUDIO_SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 4;
    config.dma_buf_len = 256;
    config.tx_desc_auto_clear = true;
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = AUDIO_PIN_BCLK;
    pins.ws_io_num = AUDIO_PIN_LRC;
    pins.data_out_num = AUDIO_PIN_DIN;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    audioListo = i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) == ESP_OK;
    if (audioListo && i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
        i2s_driver_uninstall(I2S_NUM_0);
        audioListo = false;
    }
    Serial.printf("[Audio] I2S %s (BCLK=%d LRC=%d DIN=%d)\n", audioListo ? "listo" : "FALLÓ",
                  AUDIO_PIN_BCLK, AUDIO_PIN_LRC, AUDIO_PIN_DIN);
}

void liberarI2S() {
    if (!audioListo || audioReproduciendo) return;
    i2s_driver_uninstall(I2S_NUM_0);
    audioListo = false;
    Serial.printf("[Audio] I2S apagado (libera RAM para TLS). Memoria libre: %u\n", ESP.getFreeHeap());
}

void restaurarI2S() {
    if (!audioListo && !audioReproduciendo) iniciarAudio();
}

void escribirI2S(const void* datos, size_t longitud) {
    if (!audioListo) return;
    size_t escritos = 0;
    i2s_write(I2S_NUM_0, datos, longitud, &escritos, portMAX_DELAY);
}

// Tono corto (para comprobar el cableado del parlante al arrancar)
void tonoPrueba(int frecuencia, int ms) {
    if (!audioListo) return;
    const int muestras = AUDIO_SAMPLE_RATE * ms / 1000;
    static int16_t buf[256];
    int hecho = 0;
    while (hecho < muestras) {
        int n = min(256, muestras - hecho);
        for (int i = 0; i < n; i++) {
            float fase = 2.0f * PI * frecuencia * (hecho + i) / AUDIO_SAMPLE_RATE;
            buf[i] = (int16_t)(sinf(fase) * 8000 * AUDIO_VOLUMEN / 100);
        }
        escribirI2S(buf, n * sizeof(int16_t));
        hecho += n;
    }
}

void silencioI2S(int muestras) {
    static int16_t cero[256];   // inicializado en 0
    while (muestras > 0) {
        int n = min(256, muestras);
        escribirI2S(cero, n * sizeof(int16_t));
        muestras -= n;
    }
}

// Envía PCM al amplificador I2S
void salidaAudio(const uint8_t* datos, int n) {
    escribirI2S(datos, n);
}

#include "network_scan.h"
#include "standalone_chat.h"

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
            renderOLED(state ? "happy" : "sleepy", ultimaRespuesta, true);

            Serial.printf("[Orden Remota]: %s -> %s\n", dev, state ? "ON" : "OFF");
            localServer.send(200, "application/json", "{\"status\":\"ok\"}");
            return;
        }
    }
    localServer.send(400, "application/json", "{\"status\":\"error\"}");
}

void enviarJson(JsonDocument& doc, int codigo = 200) {
    String out;
    serializeJson(doc, out);
    localServer.send(codigo, "application/json", out);
}

#include "standalone_routes.h"
#include "wifi_setup.h"

// Vista previa de una cara: http://<ip-del-esp32>/cara?c=happy|neutral|thinking|surprised|sad
// Animación: http://<ip-del-esp32>/cara?a=luz_on|luz_off|enchufe_on|enchufe_off|ventilador_on|ventilador_off|radar|musica|corazones|fiesta|saludo|reloj
void handleCara() {
    if (localServer.hasArg("buffer")) {   // copia del búfer de la OLED (1024 bytes, formato SSD1306 por páginas)
        localServer.send_P(200, "application/octet-stream", (const char*)display.getBuffer(), SCREEN_WIDTH * SCREEN_HEIGHT / 8);
        return;
    }
    if (localServer.hasArg("a")) {
        String a = localServer.arg("a");
        if (!animacionValida(a)) { localServer.send(400, "text/plain", "animacion desconocida"); return; }
        iniciarAnimacion(a);
        localServer.send(200, "text/plain", "ok " + a);
        return;
    }
    String c = localServer.hasArg("c") ? localServer.arg("c") : "neutral";
    caraActual = c;
    renderOLED(c, "Cara: " + c);
    localServer.send(200, "text/plain", "ok " + c);
}

void handleWebChat() {
    if (localServer.hasArg("q")) {
        String query = localServer.arg("q");
        encolarChat(query.substring(0, 500));
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

    Wire.setClock(400000);

    // Parlante I2S: dos notas cortas al arrancar para comprobar el cableado
    iniciarAudio();
    tonoPrueba(660, 90);
    tonoPrueba(990, 120);
    silencioI2S(AUDIO_SAMPLE_RATE / 10);
    renderOLED("neutral", "Iniciando Gimena... Conectando WiFi");

    // 3. Conexión WiFi (red guardada desde el portal o la de secrets.h).
    //    Mantener BOOT (GPIO 0) presionado al arrancar abre el portal directamente.
    pinMode(0, INPUT_PULLUP);
    bool forzarPortal = digitalRead(0) == LOW;
    if (forzarPortal) Serial.println("[Portal] Botón BOOT presionado: abriendo portal WiFi.");
    if (!forzarPortal && conectarRedConocida()) {
        Serial.printf("WiFi Conectado! IP del ESP32: %s\n", WiFi.localIP().toString().c_str());
        ultimaRespuesta = "Hola, soy Gimena. Estoy lista para ayudarte.";
        renderOLED("happy", ultimaRespuesta);

        // Servidor Web del ESP32
        localServer.on("/", handleWebRoot);
        localServer.on("/chat", handleWebChat);
        localServer.on("/cara", handleCara);
        localServer.on("/api/device", HTTP_POST, handleApiDevice);
        localServer.on("/api/devices", HTTP_GET, handleDevices);
        localServer.on(UriBraces("/api/devices/{}/toggle"), HTTP_POST, handleToggle);
        localServer.on("/api/chat", HTTP_POST, handleChat);
        localServer.on("/api/chat/status", HTTP_GET, handleChatStatus);
        localServer.on("/api/chat/voice", HTTP_GET, handleChatVoice);
        localServer.on("/api/chat/history", HTTP_DELETE, handleClearHistory);
        localServer.on("/api/network/check", HTTP_POST, handleNetworkCheck);
        localServer.on("/api/network/scan", HTTP_GET, handleNetworkScan);
        // Cambiar de red sin recompilar: http://<IP>/wifi
        localServer.on("/wifi", handlePortalPagina);
        localServer.on("/api/wifi/scan", HTTP_GET, handleWifiEscaneo);
        localServer.on("/api/wifi/connect", HTTP_POST, handleWifiConectar);
        localServer.begin();

        configTime(0, 0, "time.google.com", "pool.ntp.org");
        MDNS.begin("gimena");   // necesario para el escaneo mDNS; además publica http://gimena.local/
        MDNS.addService("http", "tcp", 80);
        WiFi.setSleep(false);   // sin ahorro de energía el ESP32 no pierde difusiones (Tuya, SSDP)
        iniciarEscuchaTuya();
        Serial.printf("[Autonomo] Abre http://%s/ en tu celular o computador.\n", WiFi.localIP().toString().c_str());
        renderOLED("happy", ultimaRespuesta, true);

    } else {
        Serial.printf("Fallo al conectar WiFi (status=%d). Abriendo portal de configuración.\n", (int)WiFi.status());
        ultimaRespuesta = "Sin WiFi. Escanea el QR para conectarme.";
        iniciarPortalWiFi();
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
    atenderPortalWiFi();
    atenderEscuchaTuya();
    actualizarPantalla();

    // Primera lectura de los enchufes Tuya en cuanto hay hora de internet (firma de la API).
    static bool tuyaPrecargado = false;
    if (!tuyaPrecargado && !portalWiFiActivo && tuyaConfigurado() && time(nullptr) > 1700000000 && !chatPending) {
        tuyaPrecargado = true;
        String error;
        if (!tuyaActualizar(error, 0)) Serial.printf("[Tuya] %s\n", error.c_str());
    }

    if (chatPending) {
        chatPending = false;
        String message = pendingMessage;
        pendingMessage = "";
        consultarGimena(message);
    }
    if (voicePending && (int32_t)(millis() - voiceReadyAt) >= 0) hablarDirectamente();
    else if (!chatOcupado() && !portalWiFiActivo && WiFi.status() == WL_CONNECTED && !animacionActiva()) {
        // En reposo: lista de enchufes al día (cada 3 min) y conexiones HTTPS listas.
        static unsigned long ultimoTuya = 0;
        if (tuyaConfigurado() && time(nullptr) > 1700000000 && millis() - ultimoTuya > 180000) {
            ultimoTuya = millis();
            String error;
            if (!tuyaActualizar(error, 170000)) Serial.printf("[Tuya] %s\n", error.c_str());
        }
        precalentarConexiones();
    }

    // Leer entrada del Monitor Serial USB
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            serialInput.trim();
            if (serialInput.length() > 0) {
                encolarChat(serialInput.substring(0, 500));
                serialInput = "";
            }
        } else {
            if (serialInput.length() < 500) serialInput += c;
        }
    }
}
