# Guía de Conexiones y Pinout - GimenAI (ESP32-S3-DevKitC-1 N8R8)

Este documento detalla el conexionado eléctrico para el robot asistente **GimenAI**, considerando las restricciones de hardware del **ESP32-S3 con 8MB de PSRAM Octal (N8R8)**.

---

## ⚠️ Advertencia Crucial: Pines Reservados para PSRAM Octal

En los módulos ESP32-S3 con PSRAM Octal (variante `N8R8` o `N16R8`), los **GPIO 33, 34, 35, 36 y 37** están conectados internamente a las líneas de datos de alta velocidad del chip de memoria PSRAM.
> **NUNCA conectes sensores, pantallas ni actuadores a los pines GPIO 33 al 37**, ya que causará fallos de caché inmediatos (*Guru Meditation Error: Core 0 panic / Cache error*).

---

## Tabla de Asignación de Pines

| Módulo / Periférico | Señal del Módulo | Pin ESP32-S3 | Bus / Función | Notas de Conexión |
| :--- | :--- | :--- | :--- | :--- |
| **INMP441** (Micrófono) | **SCK** (BCLK) | **GPIO 4** | I2S0 BCLK | Entrada de reloj |
| | **WS** (LRCL) | **GPIO 5** | I2S0 WS | Selector de palabra |
| | **SD** (DOUT) | **GPIO 6** | I2S0 SD | Datos serie de audio |
| | **L/R** | **GND** | Canal | GND selecciona canal izquierdo |
| | **VDD / GND** | 3.3V / GND | Alimentación | Alimentar desde riel de 3.3V |
| **MAX98357A** (Amplificador) | **BCLK** | **GPIO 7** | I2S1 BCLK | Reloj de bits I2S |
| | **LRC** | **GPIO 8** | I2S1 LRC | Reloj izquierdo/derecho |
| | **DIN** | **GPIO 9** | I2S1 DIN | Entrada digital de audio |
| | **SD** | **3.3V** | Control | Modo activo (o flotante) |
| | **GAIN** | **GND** | Ganancia | GND fija ganancia en 12 dB |
| | **VIN / GND** | 5V / GND | Alimentación | **VIN a fuente de 5V** (no al pin 3V3) |
| **ST7789** (Pantalla 1.54") | **CS** | **GPIO 10** | SPI CS | Chip Select |
| | **SDA** (MOSI) | **GPIO 11** | SPI MOSI | Datos Master Out |
| | **SCL** (SCK) | **GPIO 12** | SPI SCK | Reloj SPI |
| | **DC** | **GPIO 14** | Control | Data / Command |
| | **RES** (RST) | **GPIO 21** | Control | Reset de hardware |
| | **BLK** | **3.3V** | Retroiluminación | Puede conectarse a 3.3V directo |
| | **VCC / GND** | 3.3V / GND | Alimentación | Lógica a 3.3V |
| **MG90S Servo 1** | **PWM** (Naranja/Amarillo) | **GPIO 15** | PWM / Ledc | Movimiento Cabeza |
| | **VCC** (Rojo) | **+5V Servos** | Alimentación | **Riel 5V dedicado** |
| | **GND** (Marrón/Negro) | **GND Común** | Tierra | Tierra unificada |
| **MG90S Servo 2** | **PWM** (Naranja/Amarillo) | **GPIO 16** | PWM / Ledc | Movimiento Brazo |
| | **VCC** (Rojo) | **+5V Servos** | Alimentación | **Riel 5V dedicado** |
| | **GND** (Marrón/Negro) | **GND Común** | Tierra | Tierra unificada |
| **Botón Trigger (Opcional)** | Señal | **GPIO 0** | Entrada Pull-Up | Botón BOOT integrado de la placa |

---

## Esquema de Alimentación

```
             Fuente de Alimentación Principal (5V / 3A)
                              │
         ┌────────────────────┼────────────────────┐
         │                    │                    │
         ▼                    ▼                    ▼
   [ VIN ESP32-S3 ]    [ VIN MAX98357A ]   [ Riel 5V Servos ]
         │                    │                    │
   (Regulador 3.3V)           │              ┌─────┴─────┐
         │                    │              ▼           ▼
   ┌─────┴─────┐              │          [ Servo 1 ] [ Servo 2 ]
   ▼           ▼              │            (MG90S)     (MG90S)
[INMP441]  [ST7789]           │
                              │
══════════════════════════════╧══════════════════════════════
               TIERRA COMÚN (GND UNIFICADO)
```

### Reglas Esenciales de Electricidad:
1. **Separación de Potencia de Servos**: Los servomotores generan picos inductivos bruscos al arrancar que pueden hacer caer el voltaje del ESP32-S3 y provocar reinicios espontáneos (*Brownout Reset*). Usa un capacitor electrolítico de **470 µF a 1000 µF (16V)** en paralelo con las líneas de alimentación de los servomotores.
2. **Tierra Común Obligatoria**: Conecta todos los terminales de GND entre sí (Fuente 5V, ESP32, Pantalla, Micrófono, Amplificador y Servos). Si no comparten tierra, la señal PWM y los buses I2S tendrán ruido incontrolable.

---

## Entorno de pruebas: ESP32 DevKit V1 + OLED + parlante

Conexiones del firmware `esp32-test-devkit` (`firmware/src/test_mode`):

| Módulo | Señal | Pin ESP32 DevKit V1 | Notas |
| :--- | :--- | :--- | :--- |
| **OLED SSD1306** | SDA / SCL | **GPIO 21 / 22** | I2C, dirección 0x3C, VCC a 3V3 |
| **MAX98357A** (parlante) | BCLK | **GPIO 26** | I2S |
| | LRC (WS) | **GPIO 25** | I2S |
| | DIN | **GPIO 27** | I2S |
| | VIN / GND | **5V (pin VIN) / GND** | El pin VIN del DevKit entrega 5 V por USB |
| | SD | 3V3 (o sin conectar) | Amplificador activo (mezcla L+R) |
| | GAIN | sin conectar (9 dB) o GND (12 dB) | |
| **Parlante** | + / − | Salidas del MAX98357A | 4–8 Ω, 3 W máx. |

Al arrancar suenan dos notas cortas: si las oyes, el cableado del parlante es correcto.
El volumen se ajusta con `AUDIO_VOLUMEN` en `firmware/include/test_config.h`.

**Flujo autónomo:** el ESP32 sirve el panel en el puerto 80 y llama directamente
por HTTPS a Gemini para obtener la respuesta y las acciones GPIO. Después consulta
Gemini TTS y decodifica el audio PCM de 24 kHz por bloques, sin almacenarlo completo
en RAM. Remuestrea a 16 kHz para I2S o 44,1 kHz para A2DP. El PC puede estar apagado.
Se valida el certificado TLS con el almacén de autoridades de confianza del SDK
ESP32; la hora se sincroniza por NTP. Mantén actualizada la plataforma para
actualizar ese almacén de certificados. La voz depende de la cuota de Gemini TTS de la cuenta.

### Selector Bluetooth del panel web

En el entorno `esp32-test-devkit`, la voz también puede salir por un parlante o
audífonos Bluetooth Classic con perfil A2DP, sin conectar el MAX98357A.
El ESP32 realiza el escaneo y aloja el panel; el navegador se conecta directamente a la placa.

1. Compila y carga el entorno `esp32-test-devkit` de `firmware/platformio.ini`.
   La librería ESP32-A2DP se instala automáticamente. La partición `huge_app.csv`
   deja espacio para WiFi, Bluetooth y el panel local; este entorno no usa OTA.
2. Conecta el ESP32 y el celular o computador a la misma red WiFi. Abre
   `http://<IP-del-ESP32>/` (la IP aparece en el monitor serie). No ejecutes Python.
   Configura `firmware/include/secrets.h` (ignorado por Git) con `TEST_WIFI_SSID`,
   `TEST_WIFI_PASSWORD` y `GEMINI_API_KEY`, definidos con `#define` y valores entre
   comillas. Esta configuración tiene prioridad sobre `test_config.h`.
3. Enciende el parlante y activa su modo de emparejamiento. En **Bluetooth** pulsa
   **Buscar**: el selector se actualiza mientras el ESP32 escanea durante 45 segundos.
4. Elige el nombre y dirección del dispositivo y pulsa **Conectar**. El panel muestra
   el progreso y permite **Cancelar conexión**. La conexión puede tardar hasta 60 segundos.
5. Cuando aparezca **Conectado**, escribe al asistente: su voz saldrá por el parlante.
   Usa **Desconectar** antes de elegir otro. El ESP32 recuerda el último parlante
   y trata de reconectarlo al arrancar; una desconexión voluntaria borra ese recuerdo.

El selector muestra dispositivos de audio Bluetooth Classic detectables por el ESP32;
no es una lista de sensores BLE ni de dispositivos detectados por el computador.
Si no aparece un parlante, comprueba que no esté conectado a otro teléfono o PC y
vuelve a activar el emparejamiento. Un error de firmware en el panel requiere
volver a cargar `esp32-test-devkit`.

El ESP32-S3 del entorno final solo admite Bluetooth LE, por lo que no puede usar
A2DP directamente ([compatibilidad de Espressif](https://docs.espressif.com/projects/esp-adf/en/latest/solution-center/bluetooth-audio.html)).

La placa expone `/api/devices`, `/api/chat`, `/api/chat/status` y
`/api/bluetooth` con sus acciones de búsqueda, conexión y desconexión. El chat
se acepta como trabajo (HTTP 202) y el panel consulta su estado; la clave API
nunca se incluye en las respuestas HTTP ni en el HTML.
