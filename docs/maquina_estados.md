# Máquina de Estados Finitos (FSM) - GimenAI

El firmware de **GimenAI** implementa una Máquina de Estados Finitos no bloqueante para coordinar concurrentemente la captura de audio, la reproducción sonora, el renderizado de expresiones en la pantalla TFT y el control de los servomotores sin congelar la ejecución.

---

## Diagrama de Estados

```mermaid
stateDiagram-v2
    [*] --> STATE_IDLE : Arranque & WiFi OK

    STATE_IDLE --> STATE_LISTENING : Trigger (Voz / Botón BOOT GPIO 0)
    note right of STATE_IDLE
        - Cara en reposo
        - Pestañeo aleatorio
        - Servos en posición default (90°, 10°)
    end note

    STATE_LISTENING --> STATE_PROCESSING : Fin de grabación (buffer lleno)
    note right of STATE_LISTENING
        - Expresión atenta (ojos verdes)
        - Beep de inicio
        - Grabación I2S en PSRAM (WAV 16kHz)
        - Inclinación curiosa de cabeza
    end note

    STATE_PROCESSING --> STATE_RESPONDING : Respuesta recibida (HTTP 200)
    STATE_PROCESSING --> STATE_ERROR : Timeout / Error de red
    note right of STATE_PROCESSING
        - Expresión pensativa (ojos mirando arriba)
        - Envío de audio a Google Gemini
        - Análisis de emoción y texto
    end note

    STATE_RESPONDING --> STATE_IDLE : Fin de respuesta (> 3.5s)
    note right of STATE_RESPONDING
        - Expresión sincronizada (feliz, sorpresa, etc.)
        - Animación de boca hablando
        - Movimiento de cabeza y brazo (saludo, asentir)
        - Reproducción de audio I2S
    end note

    STATE_ERROR --> STATE_IDLE : Timeout de recuperación (1.5s)
    note right of STATE_ERROR
        - Expresión triste / desconcertada
        - Tono de advertencia
    end note
```

---

## Detalle de Estados

| Estado | Expresión Pantalla | Acción de Servos | Acción de Audio | Siguiente Transición |
| :--- | :--- | :--- | :--- | :--- |
| **STATE_IDLE** | `EXPR_NEUTRAL` con pestañeo cada ~4s | Posición de reposo (90°, 10°) | Silencio / Monitor de escucha | Al presionar botón o detectar activación -> `STATE_LISTENING` |
| **STATE_LISTENING** | `EXPR_LISTENING` (indicador verde) | Inclinación de atención (`think`) | Graba por I2S0 (INMP441) hacia buffer PSRAM con cabecera WAV | Tras completar 4 segundos -> `STATE_PROCESSING` |
| **STATE_PROCESSING** | `EXPR_THINKING` (mirada arriba + puntos) | Inmóvil o ligero cabeceo | Espera respuesta de red | Al recibir JSON -> `STATE_RESPONDING`, si falla -> `STATE_ERROR` |
| **STATE_RESPONDING** | `EXPR_TALKING`, `EXPR_HAPPY`, etc. | Gesto contextual (`greet`, `nod`) | Reproduce respuesta por I2S1 (MAX98357A) | Tras finalizar reproducción -> `STATE_IDLE` |
| **STATE_ERROR** | `EXPR_SAD` | Retorno suave a neutro | Tono grave de error (300 Hz) | Tras 1.5s -> `STATE_IDLE` |
