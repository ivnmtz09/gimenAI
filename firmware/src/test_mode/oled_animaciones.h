#pragma once
// ------------------------------------------------------------------------------
// Animaciones de pantalla completa en la OLED (128x64), acordes a lo pedido.
// Se reproducen unos segundos y luego vuelve la cara. Todas son procedurales
// (sin mapas de bits) y no bloquean: actualizarPantalla() pide un cuadro cada 66 ms.
//
//   luz_on, luz_off          bombillo que se enciende / apaga
//   enchufe_on, enchufe_off  enchufe que se conecta / desconecta
//   ventilador_on, _off      aspas que aceleran / se detienen
//   radar                    escaneo de la red WiFi
//   musica, corazones, fiesta, saludo, reloj   (las elige Gemini)
// ------------------------------------------------------------------------------

#ifndef ZONA_HORARIA_SEG
#define ZONA_HORARIA_SEG (-5 * 3600)   // Colombia (UTC-5)
#endif

String oledAnim;
unsigned long oledAnimInicio = 0, oledAnimFin = 0;

bool animacionActiva() {
    return oledAnim.length() && (int32_t)(oledAnimFin - millis()) > 0;
}

bool animacionValida(const String& n) {
    static const char* nombres[] = {"luz_on", "luz_off", "enchufe_on", "enchufe_off", "ventilador_on",
                                    "ventilador_off", "radar", "musica", "corazones", "fiesta", "saludo", "reloj", "whisky"};
    for (const char* x : nombres) if (n == x) return true;
    return false;
}

void dibujarPantalla();
void invertirPantalla(bool si);

void iniciarAnimacion(const String& nombre, uint32_t duracionMs = 0) {
    if (!animacionValida(nombre)) return;
    if (!duracionMs) duracionMs = nombre == "whisky" ? 6500 : nombre == "reloj" ? 5000 : nombre.startsWith("luz") || nombre.startsWith("enchufe") ||
                                                             nombre.startsWith("ventilador") ? 3200 : 4000;
    oledAnim = nombre;
    oledAnimInicio = millis();
    oledAnimFin = oledAnimInicio + duracionMs;
    dibujarPantalla();
}

void detenerAnimacion() { oledAnim = ""; }

// Número pseudoaleatorio estable para cada partícula (no cambia entre cuadros).
uint32_t azar(uint32_t semilla) {
    semilla ^= semilla << 13; semilla ^= semilla >> 17; semilla ^= semilla << 5;
    return semilla * 2654435761u;
}

void rayos(int cx, int cy, int r0, int largo, float giro) {
    for (int i = 0; i < 8; i++) {
        float a = giro + i * PI / 4;
        float c = cosf(a), s = sinf(a);
        for (int g = -1; g <= 0; g++)
            display.drawLine(cx + c * r0 + g, cy + s * r0, cx + c * (r0 + largo) + g, cy + s * (r0 + largo), SSD1306_WHITE);
    }
}

void bombillo(int cx, int cy, bool encendido) {
    if (encendido) display.fillCircle(cx, cy, 13, SSD1306_WHITE);
    else {
        display.drawCircle(cx, cy, 13, SSD1306_WHITE);
        display.drawCircle(cx, cy, 12, SSD1306_WHITE);
    }
    // filamento (negro si está encendido)
    uint16_t col = encendido ? SSD1306_BLACK : SSD1306_WHITE;
    display.drawLine(cx - 4, cy + 4, cx - 2, cy - 2, col);
    display.drawLine(cx - 2, cy - 2, cx, cy + 2, col);
    display.drawLine(cx, cy + 2, cx + 2, cy - 2, col);
    display.drawLine(cx + 2, cy - 2, cx + 4, cy + 4, col);
    // cuello y rosca
    display.fillRect(cx - 7, cy + 11, 15, 4, SSD1306_WHITE);
    for (int i = 0; i < 3; i++) display.fillRoundRect(cx - 6, cy + 16 + i * 4, 13, 3, 1, SSD1306_WHITE);
    display.fillRect(cx - 2, cy + 28, 5, 2, SSD1306_WHITE);
}

void animLuz(uint32_t t, bool enciende) {
    const int cx = 64, cy = 22;
    if (enciende) {
        bool lit = t > 700 || (t > 450 && t < 550);          // parpadeo antes de prender
        bombillo(cx, cy, lit);
        if (t > 700) {
            int largo = 5 + (int)(4 * (1 + sinf(t / 120.0f)));
            rayos(cx, cy, 17, min<int>(largo, (t - 700) / 40 + 1), 0);
        }
        invertirPantalla(t > 700 && t < 820);                  // destello
    } else {
        bool lit = t < 900;
        bombillo(cx, cy, lit);
        if (lit) rayos(cx, cy, 17, max<int>(0, 9 - (int)t / 100), 0);
        if (t > 1100) {                                        // luna y Zz
            display.fillCircle(108, 14, 9, SSD1306_WHITE);
            display.fillCircle(113, 11, 8, SSD1306_BLACK);
            int zy = 34 - (int)((t / 60) % 14);
            display.drawFastHLine(14, zy, 7, SSD1306_WHITE);
            display.drawLine(20, zy, 14, zy + 6, SSD1306_WHITE);
            display.drawFastHLine(14, zy + 6, 7, SSD1306_WHITE);
        }
    }
}

void animEnchufe(uint32_t t, bool conecta) {
    // Tomacorriente a la derecha, clavija que entra o sale por la izquierda.
    display.drawRoundRect(88, 10, 36, 40, 6, SSD1306_WHITE);
    display.drawRoundRect(89, 11, 34, 38, 5, SSD1306_WHITE);
    display.fillRoundRect(98, 20, 4, 9, 1, SSD1306_WHITE);
    display.fillRoundRect(110, 20, 4, 9, 1, SSD1306_WHITE);
    display.fillCircle(106, 39, 3, SSD1306_WHITE);

    float p = constrain(t / 1000.0f, 0.0f, 1.0f);              // 0 = lejos, 1 = conectado
    if (!conecta) p = 1.0f - p;
    p = p * p * (3 - 2 * p);                                     // suavizado
    int x = 8 + (int)(p * 50);                                   // borde derecho de la clavija = x + 30
    display.fillRect(0, 26, x + 4, 3, SSD1306_WHITE);            // cable
    display.fillRoundRect(x, 16, 24, 22, 5, SSD1306_WHITE);      // cuerpo
    display.fillRect(x + 24, 21, 8, 3, SSD1306_WHITE);           // patas
    display.fillRect(x + 24, 30, 8, 3, SSD1306_WHITE);

    bool conectado = conecta ? t > 1000 : t < 150;
    if (conectado && conecta && ((t / 150) % 2 == 0)) {         // rayito parpadeante
        display.fillTriangle(70, 2, 62, 14, 68, 14, SSD1306_WHITE);
        display.fillTriangle(68, 12, 74, 12, 64, 26, SSD1306_WHITE);
    }
    if (!conecta && t > 1100) {                                  // chispa que se apaga
        for (int i = 0; i < 4; i++) {
            int d = 4 + (t - 1100) / 60;
            if (d < 16) display.drawPixel(80 + d * (i % 2 ? 1 : -1), 26 + d * (i < 2 ? 1 : -1) / 2, SSD1306_WHITE);
        }
    }
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    if (conecta && t > 1100) { display.setCursor(4, 46); display.print("ON"); }
    if (!conecta && t > 1100) { display.setCursor(4, 46); display.print("OFF"); }
    display.setTextSize(1);
}

void animVentilador(uint32_t t, bool enciende) {
    const int cx = 64, cy = 26, r = 22;
    // ángulo con aceleración (on) o frenado (off)
    float s = t / 1000.0f;
    float ang = enciende ? 2.5f * s * s + 1.0f * s : 9.0f * s - 2.2f * s * s;
    if (!enciende && s > 2.0f) ang = 9.0f * 2.0f - 2.2f * 4.0f;   // detenido
    for (int i = 0; i < 3; i++) {
        float a = ang * 2 + i * 2 * PI / 3;
        float a1 = a - 0.38f, a2 = a + 0.38f;
        display.fillTriangle(cx, cy, cx + cosf(a1) * r, cy + sinf(a1) * r, cx + cosf(a2) * r, cy + sinf(a2) * r, SSD1306_WHITE);
        display.fillCircle(cx + cosf(a) * (r - 3), cy + sinf(a) * (r - 3), 6, SSD1306_WHITE);
    }
    display.drawCircle(cx, cy, r + 4, SSD1306_WHITE);
    display.fillCircle(cx, cy, 4, SSD1306_BLACK);
    display.drawCircle(cx, cy, 4, SSD1306_WHITE);
    display.fillRect(cx - 1, cy + r + 4, 3, 64 - (cy + r + 4) - 2, SSD1306_WHITE);
    display.fillRoundRect(cx - 14, 61, 29, 3, 1, SSD1306_WHITE);
    if (enciende && t > 1200) {                                 // líneas de viento
        for (int i = 0; i < 3; i++) {
            int x = 100 + (int)((t / 30 + i * 9) % 26);
            display.drawFastHLine(x, 14 + i * 12, 6, SSD1306_WHITE);
        }
    }
}

void animRadar(uint32_t t) {
    const int cx = 64, cy = 32;
    for (int r = 10; r <= 30; r += 10) display.drawCircle(cx, cy, r, SSD1306_WHITE);
    display.drawFastHLine(cx - 31, cy, 63, SSD1306_WHITE);
    display.drawFastVLine(cx, cy - 31, 63, SSD1306_WHITE);
    float a = t / 350.0f;
    for (int k = 0; k < 4; k++) {                                // barrido con estela
        float b = a - k * 0.08f;
        display.drawLine(cx, cy, cx + cosf(b) * 30, cy + sinf(b) * 30, SSD1306_WHITE);
    }
    for (int i = 0; i < 6; i++) {                                // dispositivos que aparecen
        uint32_t h = azar(i + 7);
        float ai = (h % 628) / 100.0f;
        int ri = 8 + (h >> 10) % 20;
        float dif = fmodf(a - ai + 6 * PI, 2 * PI);
        if (t / 350.0f > ai && dif < 2.2f)
            display.fillCircle(cx + cosf(ai) * ri, cy + sinf(ai) * ri, dif < 0.6f ? 3 : 2, SSD1306_WHITE);
    }
    // ondas WiFi a los lados
    int fase = (t / 250) % 4;
    for (int i = 0; i < fase; i++) {
        dibujarArco(14, 44, 5 + i * 5, 225, 315, 2);
        dibujarArco(114, 44, 5 + i * 5, 225, 315, 2);
    }
    display.fillCircle(14, 44, 2, SSD1306_WHITE);
    display.fillCircle(114, 44, 2, SSD1306_WHITE);
}

void nota(int x, int y, bool doble) {
    display.fillCircle(x, y, 3, SSD1306_WHITE);
    display.drawFastVLine(x + 3, y - 12, 12, SSD1306_WHITE);
    if (doble) {
        display.fillCircle(x + 10, y - 2, 3, SSD1306_WHITE);
        display.drawFastVLine(x + 13, y - 14, 12, SSD1306_WHITE);
        display.fillRect(x + 3, y - 14, 11, 3, SSD1306_WHITE);
    } else {
        display.drawLine(x + 3, y - 12, x + 8, y - 7, SSD1306_WHITE);
        display.drawLine(x + 4, y - 12, x + 8, y - 8, SSD1306_WHITE);
    }
}

void animMusica(uint32_t t) {
    for (int i = 0; i < 4; i++) {
        int periodo = 2200;
        int fase = (t + i * 550) % periodo;
        int y = 54 - fase * 50 / periodo;
        int x = 14 + i * 30 + (int)(6 * sinf((t + i * 400) / 250.0f));
        nota(x, y, i % 2);
    }
    for (int i = 0; i < 16; i++) {                               // ecualizador
        int h = 2 + azar(i * 31 + t / 120) % 9;
        display.fillRect(i * 8 + 1, 64 - h, 6, h, SSD1306_WHITE);
    }
}

void animCorazones(uint32_t t) {
    for (int i = 0; i < 6; i++) {
        uint32_t h = azar(i + 3);
        int periodo = 1800 + h % 900;
        int fase = (t + h % periodo) % periodo;
        int y = 72 - fase * 84 / periodo;
        int x = 10 + (h >> 8) % 108 + (int)(4 * sinf((t + i * 300) / 200.0f));
        int s = 4 + (h >> 4) % 5;
        dibujarCorazon(x, y, s);
    }
    int s = (t / 250) % 2 ? 11 : 13;                             // corazón grande que late
    dibujarCorazon(64, 30, s);
}

void animFiesta(uint32_t t) {
    for (int i = 0; i < 36; i++) {                               // confeti cayendo
        uint32_t h = azar(i + 11);
        int vel = 30 + h % 40;
        int y = (int)((t * vel / 1000 + (h >> 8) % 64) % 72) - 4;
        int x = (h >> 16) % 128 + (int)(3 * sinf((t + i * 90) / 150.0f));
        if (i % 3 == 0) display.fillRect(x, y, 3, 2, SSD1306_WHITE);
        else if (i % 3 == 1) display.drawPixel(x, y, SSD1306_WHITE);
        else display.fillRect(x, y, 2, 3, SSD1306_WHITE);
    }
    uint32_t c = t % 1300;                                       // fuego artificial
    if (c < 900) {
        int r = c / 30;
        for (int k = 0; k < 12; k++) {
            float a = k * PI / 6;
            display.drawLine(64 + cosf(a) * r * 0.6f, 26 + sinf(a) * r * 0.6f, 64 + cosf(a) * r, 26 + sinf(a) * r, SSD1306_WHITE);
        }
    }
}

void animSaludo(uint32_t t) {
    int cx = 64 + (int)(9 * sinf(t / 140.0f)), cy = 34;
    display.fillRoundRect(cx - 12, cy - 4, 24, 22, 7, SSD1306_WHITE);            // palma
    for (int i = 0; i < 4; i++) display.fillRoundRect(cx - 12 + i * 6, cy - 18 + (i == 0 || i == 3 ? 4 : 0), 5, 18, 2, SSD1306_WHITE);
    for (int i = 1; i < 4; i++) display.drawFastVLine(cx - 12 + i * 6 - 1, cy - 14, 12, SSD1306_BLACK);
    display.fillRoundRect(cx + 9, cy + 1, 12, 6, 3, SSD1306_WHITE);              // pulgar
    display.fillRect(cx - 8, cy + 18, 16, 6, SSD1306_WHITE);                     // muñeca
    // líneas de movimiento
    dibujarArco(64, 30, 34, 200, 240, 2);
    dibujarArco(64, 30, 34, 300, 340, 2);
    if ((t / 300) % 2) { dibujarArco(64, 30, 40, 205, 235, 1); dibujarArco(64, 30, 40, 305, 335, 1); }
}

void animReloj(uint32_t t) {
    const int cx = 64, cy = 32, r = 28;
    display.drawCircle(cx, cy, r, SSD1306_WHITE);
    display.drawCircle(cx, cy, r - 1, SSD1306_WHITE);
    for (int i = 0; i < 12; i++) {
        float a = i * PI / 6;
        int l = i % 3 == 0 ? 5 : 2;
        display.drawLine(cx + cosf(a) * (r - 3), cy + sinf(a) * (r - 3), cx + cosf(a) * (r - 3 - l), cy + sinf(a) * (r - 3 - l), SSD1306_WHITE);
    }
    time_t ahora = time(nullptr) + ZONA_HORARIA_SEG;
    struct tm h;
    gmtime_r(&ahora, &h);
    float am = (h.tm_min + h.tm_sec / 60.0f) * PI / 30;
    float ah = ((h.tm_hour % 12) + h.tm_min / 60.0f) * PI / 6;
    float as = h.tm_sec * PI / 30;
    float p = constrain(t / 900.0f, 0.0f, 1.0f);                 // las agujas giran hasta la hora real
    p = 1 - (1 - p) * (1 - p);
    am = am * p + 4 * PI * (1 - p) * p; ah *= p;
    auto aguja = [&](float a, int largo, int grosor) {
        a -= PI / 2;
        for (int g = 0; g < grosor; g++)
            display.drawLine(cx + g % 2, cy + g / 2, cx + cosf(a) * largo + g % 2, cy + sinf(a) * largo + g / 2, SSD1306_WHITE);
    };
    aguja(ah, 14, 4);
    aguja(am, 21, 2);
    if (t > 900) aguja(as, 23, 1);
    display.fillCircle(cx, cy, 3, SSD1306_WHITE);
    if (time(nullptr) > 1700000000 && t > 900) {                 // hora digital al costado
        char hora[6];
        snprintf(hora, sizeof(hora), "%02d:%02d", h.tm_hour, h.tm_min);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(97, 54);
        display.print(hora);
    }
}

// Solo envía el comando a la OLED cuando cambia (el destello de luz_on invierte la pantalla).
void invertirPantalla(bool si) {
    static bool invertida = false;
    if (si != invertida) { display.invertDisplay(si); invertida = si; }
}

// ---- Comando secreto "tengo sed": botella de whisky cuadrada (Old Parr 12) que sirve un trago ----
bool dentroCuerpo(int x, int y, int x0, int y0, int x1, int y1, int r) {
    if (x < x0 || x > x1 || y < y0 || y > y1) return false;
    int cx = x < x0 + r ? x0 + r : x > x1 - r ? x1 - r : x;
    int cy = y < y0 + r ? y0 + r : y > y1 - r ? y1 - r : y;
    return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
}

void botellaOldParr(int dx) {
    const int x0 = 4 + dx, x1 = 50 + dx, y0 = 21, y1 = 63, cx = 27 + dx;
    // tapa dorada con franjas
    display.fillRoundRect(cx - 7, 0, 15, 9, 2, SSD1306_WHITE);
    for (int y = 2; y < 9; y += 3) display.drawFastHLine(cx - 6, y, 13, SSD1306_BLACK);
    // cuello y hombros
    display.drawRect(cx - 6, 9, 13, 8, SSD1306_WHITE);
    for (int y = 11; y < 17; y += 2) for (int x = cx - 4 + (y / 2) % 2; x < cx + 5; x += 2) display.drawPixel(x, y, SSD1306_WHITE);
    display.fillRect(cx - 7, 16, 15, 2, SSD1306_WHITE);               // anillo del cuello
    display.drawLine(cx - 7, 17, x0 + 3, y0, SSD1306_WHITE);
    display.drawLine(cx + 7, 17, x1 - 3, y0, SSD1306_WHITE);
    display.fillCircle(cx, 19, 2, SSD1306_WHITE);                     // sello rojo
    // cuerpo cuadrado con la textura "craquelada" en rombos
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (!dentroCuerpo(x, y, x0, y0, x1, y1, 6)) continue;
            bool borde = !dentroCuerpo(x - 1, y, x0, y0, x1, y1, 6) || !dentroCuerpo(x + 1, y, x0, y0, x1, y1, 6) ||
                         !dentroCuerpo(x, y - 1, x0, y0, x1, y1, 6) || !dentroCuerpo(x, y + 1, x0, y0, x1, y1, 6);
            int u = x - dx;
            if (borde || (u + y) % 6 == 0 || (u - y + 120) % 6 == 0) display.drawPixel(x, y, SSD1306_WHITE);
        }
    // etiqueta ovalada dorada
    display.fillRoundRect(cx - 19, 27, 39, 33, 15, SSD1306_BLACK);
    display.fillRoundRect(cx - 18, 28, 37, 31, 14, SSD1306_WHITE);
    display.drawRoundRect(cx - 16, 30, 33, 27, 12, SSD1306_BLACK);
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(cx - 8, 33);  display.print("Old");
    display.setCursor(cx - 11, 41); display.print("Parr");
    display.setCursor(cx - 5, 49);  display.print("12");
    display.setTextColor(SSD1306_WHITE);
}

void vasoConHielo(uint32_t t, float nivel) {
    const int gx0 = 76, gx1 = 116, gy0 = 28, gy1 = 62;
    int alto = (int)(nivel * 22);
    for (int y = gy1 - 1 - alto; y < gy1 - 1; y++)                  // whisky (tramado)
        for (int x = gx0 + 2; x < gx1 - 1; x++)
            if ((x + y) % 2 == 0) display.drawPixel(x, y, SSD1306_WHITE);
    if (alto > 0) display.drawFastHLine(gx0 + 1, gy1 - 1 - alto, gx1 - gx0 - 1, SSD1306_WHITE);
    for (int i = 0; i < 2; i++) {                                    // dos cubos de hielo que flotan
        int iy = gy1 - 1 - max(alto, 8) - 2 + (int)(2 * sinf(t / 300.0f + i * 2)) + i * 5;
        int ix = gx0 + 7 + i * 15;
        display.fillRect(ix, iy, 11, 10, SSD1306_BLACK);
        display.drawRoundRect(ix, iy, 11, 10, 2, SSD1306_WHITE);
        display.drawLine(ix + 2, iy + 2, ix + 4, iy + 2, SSD1306_WHITE);
    }
    display.drawLine(gx0, gy0, gx0 + 2, gy1, SSD1306_WHITE);         // vaso
    display.drawLine(gx1, gy0, gx1 - 2, gy1, SSD1306_WHITE);
    display.drawFastHLine(gx0 + 2, gy1, gx1 - gx0 - 3, SSD1306_WHITE);
    display.drawFastHLine(gx0 + 3, gy1 - 1, gx1 - gx0 - 5, SSD1306_WHITE);
}

void animWhisky(uint32_t t) {
    float entra = constrain(t / 700.0f, 0.0f, 1.0f);
    entra = 1 - (1 - entra) * (1 - entra);
    botellaOldParr((int)(-56 * (1 - entra)));
    if (t < 700) return;
    float nivel = constrain((t - 1000) / 2200.0f, 0.0f, 1.0f);
    if (t > 1000 && t < 3200) {                                      // chorro desde arriba
        int fondo = 61 - (int)(nivel * 22);
        for (int y = 0; y < fondo; y++) if ((y + t / 40) % 5 != 0) display.drawFastVLine(96 + (y % 3 == 0), y, 1, SSD1306_WHITE);
        if ((t / 120) % 2) { display.drawPixel(92, fondo - 2, SSD1306_WHITE); display.drawPixel(100, fondo - 3, SSD1306_WHITE); }
    }
    vasoConHielo(t, nivel);
    if (t > 3400) {                                                  // brindis
        display.setTextSize(1);
        display.setCursor(70, 6);
        display.print("Salud!");
        int f = (t / 200) % 2;
        dibujarBrillo(64, 4 + f * 3, 2 + f);
        dibujarBrillo(122, 12 - f * 3, 3 - f);
    }
}

void dibujarAnimacion() {
    uint32_t t = millis() - oledAnimInicio;
    if (oledAnim != "luz_on") invertirPantalla(false);
    if (oledAnim == "luz_on") animLuz(t, true);
    else if (oledAnim == "luz_off") animLuz(t, false);
    else if (oledAnim == "enchufe_on") animEnchufe(t, true);
    else if (oledAnim == "enchufe_off") animEnchufe(t, false);
    else if (oledAnim == "ventilador_on") animVentilador(t, true);
    else if (oledAnim == "ventilador_off") animVentilador(t, false);
    else if (oledAnim == "radar") animRadar(t);
    else if (oledAnim == "musica") animMusica(t);
    else if (oledAnim == "corazones") animCorazones(t);
    else if (oledAnim == "fiesta") animFiesta(t);
    else if (oledAnim == "saludo") animSaludo(t);
    else if (oledAnim == "reloj") animReloj(t);
    else if (oledAnim == "whisky") animWhisky(t);
}

// Animación que corresponde a encender/apagar una salida (GPIO o enchufe Tuya).
String animacionDeDispositivo(const String& id, bool estado, const String& nombre = "") {
    String n = nombre; n.toLowerCase();
    String base;
    if (id == "ventilador" || n.indexOf("ventil") >= 0) base = "ventilador";
    else if (id.startsWith("luz") || n.indexOf("luz") >= 0 || n.indexOf("lampara") >= 0 ||
             n.indexOf("bombill") >= 0 || n.indexOf("foco") >= 0) base = "luz";
    else base = "enchufe";
    return base + (estado ? "_on" : "_off");
}
