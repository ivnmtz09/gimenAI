# GimenAI - Asistente de IA Robótico para el Hogar 🤖✨

## Modo actual: ESP32 autónomo (DevKit V1)

El entorno predeterminado `esp32-test-devkit` sirve el panel web y controla
Bluetooth, salidas GPIO y pantalla desde la propia placa. Consulta Gemini por
HTTPS y reproduce su voz por I2S o Bluetooth A2DP. **No necesita Python ni un
servidor en el PC**; sí necesita WiFi con Internet para Gemini y la voz.

1. Crea `firmware/include/secrets.h` (excluido de Git) con `#define TEST_WIFI_SSID`,
   `#define TEST_WIFI_PASSWORD` y `#define GEMINI_API_KEY`, cada valor entre comillas.
2. Compila y carga `esp32-test-devkit` desde `firmware/` con PlatformIO.
3. Abre `http://<IP-del-ESP32>/` en un celular o computador en la misma red.
   La dirección aparece en el monitor serie a 115200 baudios.
4. En Bluetooth, pulsa **Buscar**, elige un parlante en modo emparejamiento y
   pulsa **Conectar**. Escribe al asistente desde ese mismo panel.

Gemini y la síntesis de voz siguen ejecutándose en los servicios de Google;
la placa gestiona las solicitudes directamente. El historial conserva las tres
últimas interacciones en RAM y se borra al reiniciar o limpiar el chat. La voz usa
la cuota de Gemini TTS de la cuenta; un error de voz no descarta la respuesta escrita.

`server/` conserva la pasarela anterior para otras pruebas. No es parte del
arranque del modo autónomo. Las instrucciones de Python y del ESP32-S3 que
aparecen más abajo corresponden a esa arquitectura anterior y a la fase final.


**Proyecto de la Asignatura de Internet de las Cosas (IoT)**  
**Ingeniería de Sistemas — Universidad de La Guajira**  
**Entrega:** Fase 1 (Definición del problema, arquitectura, esquemáticos y estructura base)

---

## 📖 ¿De qué trata?

**GimenAI** es un robot de escritorio interactivo que funciona como asistente personal y de compañía en el hogar, con nombre e identidad propia: **Gimena**.

El usuario le habla en lenguaje natural; el robot escucha mediante un micrófono I2S, procesa la petición en la nube a través de la API de **Google Gemini** y responde por voz a través de un amplificador y parlante, mientras sincroniza:
1. **Expresiones faciales animadas** en una pantalla a color TFT ST7789.
2. **Movimientos expresivos de cabeza y brazos** mediante servomotores MG90S.

La meta principal es que la interacción no se sienta como una simple bocina estática, sino como un **personaje amigable, empático y vivo** que reacciona con gestos y emociones.

---

## 🎯 Problema que atiende

- **Soledad y falta de acompañamiento:** Adultos mayores y personas que viven solas carecen de recordatorios oportunos, apoyo diario y compañía interactiva.
- **Asistentes comerciales cerrados y costosos:** La mayoría de alternativas del mercado son rígidas, de alto costo y con poca capacidad de personalización local.
- **Falta de expresividad:** Los altavoces inteligentes convencionales responden únicamente con un pitido o una luz LED monocromática, careciendo de calidez humana.

---

## 👥 Usuarios y Contexto

- **Usuarios objetivo:** Adultos mayores y personas que viven solas en el hogar.
- **Contexto de uso:** Entorno doméstico, ubicado sobre mesas de noche, escritorios o muebles de la sala/habitación.

---

## 📋 Requisitos Verificables

| # | Requisito | Valor de Referencia | Estado / Validación |
| :---: | :--- | :--- | :--- |
| **1** | Reconocimiento de voz | Precisión aceptable en ambiente doméstico (8 de 10 aciertos) | Audio 16 kHz PCM vía I2S a Gemini Multimodal |
| **2** | Tiempo de respuesta | Latencia menor a 3 segundos tras el comando | Optimizado mediante respuestas concisas y streaming |
| **3** | Expresiones faciales | Al menos 4 expresiones distintas en pantalla | 7 expresiones implementadas (Neutro, Feliz, Escuchando, Pensando, Hablando, Sorprendido, Triste) |
| **4** | Movimiento físico | Al menos 2 grados de libertad (cabeza y brazos) | 2 servomotores MG90S con gestos suaves no bloqueantes |
| **5** | Estabilidad | Operación continua mínima de 30 minutos sin reinicios | Arquitectura de alimentación aislada y FSM sin bloqueos |

---

## 📐 Arquitectura del Sistema

El microcontrolador **ESP32-S3 (N8R8)** opera como el cerebro de hardware central. Se comunica con la nube de Google Gemini vía WiFi para el análisis conversacional y coordina los periféricos locales mediante buses de alta velocidad:

```
                          ┌──────────────────────────┐
                          │  Nube de Inteligencia    │
                          │   Google Gemini API      │
                          │  (Audio Multimodal / LLM)│
                          └─────────────▲────────────┘
                                        │
                                      WiFi
                                        │
                          ┌─────────────▼────────────┐
                          │    ESP32-S3 DevKitC-1    │
                          │    (N8R8 - 8MB PSRAM)    │
                          └──────┬────┬────┬────┬────┘
                                 │    │    │    │
            ┌────────────────────┘    │    │    └────────────────────┐
            ▼                         ▼    ▼                         ▼
   INMP441 (Micrófono)             ST7789  MAX98357A + Parlante   MG90S (x2)
   Bus I2S0 (Entrada)               (SPI)   Bus I2S1 (Salida)     Control PWM
```

---

## 🔌 Asignación de Pines (ESP32-S3 N8R8)

> [!WARNING]
> En la versión **N8R8**, los pines **GPIO 33 al 37 están reservados para la memoria PSRAM Octal**. Nunca deben utilizarse para periféricos externos.

| Módulo | Función / Señal | Pin ESP32-S3 | Bus / Protocolo | Notas |
| :--- | :--- | :--- | :--- | :--- |
| **INMP441** | SCK / WS / SD | **GPIO 4 / 5 / 6** | I2S0 (RX) | L/R a GND (canal izquierdo), VDD a 3.3V |
| **MAX98357A** | BCLK / LRC / DIN | **GPIO 7 / 8 / 9** | I2S1 (TX) | SD a 3.3V, GAIN a GND (12 dB), VIN a 5V |
| **ST7789** | CS / MOSI / SCK | **GPIO 10 / 11 / 12** | SPI (Display) | Pantalla TFT 1.54" (240x240) |
| **ST7789** | DC / RST / BLK | **GPIO 14 / 21 / 3V3** | Control Pantalla | Retroiluminación fija a 3.3V |
| **MG90S (1)** | PWM Cabeza | **GPIO 15** | PWM (50 Hz) | Giro / Inclinación de cabeza |
| **MG90S (2)** | PWM Brazo | **GPIO 16** | PWM (50 Hz) | Saludo y gestos corporales |
| **Trigger** | Botón Activación | **GPIO 0** | Entrada Pull-Up | Botón BOOT integrado del DevKit |

Para más detalles eléctricos y esquemas de desacoplo, consulta [docs/pinout_y_conexiones.md](docs/pinout_y_conexiones.md).

---

## ⚡ Esquema de Alimentación

Para evitar reinicios por caída de voltaje (*Brownout*):
- **Lógica (3.3 V):** ESP32-S3, micrófono INMP441, pantalla ST7789 y lógica del MAX98357A.
- **Audio (5 V):** Pin VIN del amplificador MAX98357A desde la fuente externa.
- **Servomotores (5 V dedicado):** Alimentados **únicamente** desde el riel de potencia de la fuente (nunca desde los pines de 3.3V o 5V de salida de la placa). Se recomienda un capacitor de 470 µF a 1000 µF en paralelo con los servos.
- **GND Común:** Todos los negativos (tierra) deben estar unidos firmemente.

---

## 📁 Estructura del Repositorio

```text
gimenAI/
├── README.md                      # Documentación principal del proyecto
├── .gitignore                     # Exclusión de binarios y claves privadas
├── docs/                          # Documentación técnica de soporte
│   ├── pinout_y_conexiones.md     # Tabla de conexiones y consideraciones eléctricas
│   └── maquina_estados.md         # Diagrama de la FSM y flujo de operación
├── firmware/                      # Código C++ para ESP32-S3 (PlatformIO)
│   ├── platformio.ini             # Configuración del microcontrolador y librerías
│   ├── include/
│   │   ├── config.h               # Pines GPIO, red WiFi y parámetros de audio
│   │   └── expressions.h          # Enums de estados y emociones faciales
│   └── src/
│       ├── main.cpp               # Bucle principal y Máquina de Estados Finitos
│       ├── audio_controller.h/.cpp# Grabación (INMP441) y reproducción (MAX98357A) I2S
│       ├── display_controller.h/.cpp# Animaciones de ojos y expresiones en ST7789
│       └── servo_controller.h/.cpp  # Control suave y gestos de servomotores MG90S
└── server/                        # Pasarela de baja latencia con Google Gemini (Python)
    ├── main.py                    # Servidor FastAPI con audio multimodal
    ├── requirements.txt           # Dependencias de Python
    └── .env.example               # Plantilla de variables de entorno (API Key)
```

---

## 🚀 Guía de Inicio Paso a Paso

### 1. Obtener la Clave de Google Gemini API
1. Entra a [Google AI Studio](https://aistudio.google.com/).
2. Inicia sesión con tu cuenta de Google y haz clic en **"Get API key"**.
3. Copia la clave generada.

### 2. Configurar y Ejecutar el Servidor Backend (PC o Servidor Local)
El servidor en `server/` recibe el audio del ESP32, lo envía directamente a Gemini 1.5 Flash y responde con la emoción, el gesto y el texto de Gimena:

```bash
# Entrar a la carpeta del servidor
cd server

# Crear y activar entorno virtual
python3 -m venv .venv
source .venv/bin/activate    # En Linux / macOS
# .venv\Scripts\activate     # En Windows

# Instalar dependencias
pip install -r requirements.txt

# Configurar tu API Key
cp .env.example .env
# Edita .env y coloca tu GEMINI_API_KEY
```

Inicia el servidor:
```bash
python3 main.py
```
> El servidor quedará escuchando en `http://0.0.0.0:8000`. Puedes verificar la IP local de tu computador con `ip a` (Linux) o `ipconfig` (Windows).

### 3. Configurar el Firmware del ESP32-S3
1. Abre la carpeta `gimenAI` en **VS Code** con la extensión **PlatformIO IDE**.
2. Abre [firmware/include/config.h](firmware/include/config.h) y ajusta:
   - `WIFI_SSID`: Nombre de tu red WiFi.
   - `WIFI_PASSWORD`: Contraseña de tu red WiFi.
   - `SERVER_HOST`: La dirección IP local de tu computador donde corre el servidor (ej. `"192.168.1.50"`).
3. Conecta tu ESP32-S3 por el puerto USB principal (USB/UART).
4. En la barra inferior de PlatformIO, haz clic en **Build** (Compilar) y luego en **Upload** (Subir).
5. Abre el **Serial Monitor** (115200 baudios) para observar el arranque y la conexión.

### 4. Probar la Interacción
- Al arrancar, Gimena mostrará sus ojos, se conectará a WiFi y hará un saludo con su brazo.
- Presiona el botón **BOOT (GPIO 0)** del ESP32: Gimena pasará a modo de **Escucha** (ojos verdes) y emitirá un pitido.
- Háblale: *"Hola Gimena, ¿cómo estás hoy?"*.
- Gimena pasará a modo **Pensando** (ojos amarillos) mientras procesa con Gemini.
- Gimena responderá mostrando su expresión facial correspondiente, moviendo la cabeza o el brazo y reproduciendo audio.

---

## 🛠️ Tecnologías y Librerías Utilizadas

- **Microcontrolador:** ESP32-S3 dual-core Tensilica Xtensa LX7 a 240 MHz.
- **Entorno de Desarrollo:** PlatformIO con framework Arduino Core for ESP32.
- **Librería Gráfica:** [LovyanGFX](https://github.com/lovyan03/LovyanGFX) (rendimiento SPI acelerado en ST7789 sin necesidad de modificar archivos de configuración internos).
- **Librería de Servos:** [ESP32Servo](https://github.com/madhephaestus/ESP32Servo) (control PWM con temporizadores de hardware).
- **IA Conversacional:** [Google Gemini API](https://ai.google.dev/) (procesamiento multimodal de audio directo en formato WAV 16kHz).
- **Backend:** FastAPI + Uvicorn (Python 3).

---

## 👥 Equipo de Trabajo

- **Asignatura:** Internet de las Cosas (IoT).
- **Programa:** Ingeniería de Sistemas.
- **Institución:** Universidad de La Guajira.
