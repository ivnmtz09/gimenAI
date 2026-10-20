#pragma once
#include <Arduino.h>

// Dispositivos de la cuenta Smart Life vinculada al proyecto de Tuya IoT Platform.
// Gimena los identifica con alias cortos (tuya_1, tuya_2...) para que Gemini no
// tenga que copiar los ID largos de Tuya.
struct DispositivoTuya {
    String id, nombre, alias, codigoSwitch;
    bool enLinea = false, encendido = false;
    float potencia = -1;   // W, -1 si el dispositivo no la reporta
};

bool tuyaConfigurado();
// Descarga la lista si la copia en memoria tiene más de maxEdadMs. Devuelve false si falla.
bool tuyaActualizar(String& error, uint32_t maxEdadMs = 60000);
bool tuyaComando(const String& aliasOId, bool encender, String& error);
// Texto para el prompt: "tuya_1 = luz cuarto (enchufe Tuya): OFF; ..."
String tuyaResumen();
size_t enchufesTuya();
const DispositivoTuya& enchufeTuya(size_t i);
