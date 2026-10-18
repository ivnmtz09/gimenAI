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
