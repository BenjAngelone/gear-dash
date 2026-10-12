#pragma once
#include <Arduino.h>

#include "logic.h"

// Enlace con el controlador del auto. Las operaciones son bloqueantes (conexion,
// reintentos) y corren en una tarea aparte; la UI solo pide trabajos y lee el estado.
namespace car {

enum Phase : uint8_t { P_IDLE, P_CONNECTING, P_QUERYING, P_SENDING, P_DONE, P_FAILED };
enum Err : uint8_t { E_NONE, E_NO_SIGNAL, E_NO_STATE, E_STEP, E_MISMATCH };

void begin();
void requestState();   // consulta el estado del auto (no mueve reles)
void requestToggle();  // ejecuta ENCENDER/APAGAR segun el estado confirmado
void clearResult();    // olvida un resultado DONE/FAILED anterior (si no hay trabajo en curso)

void debugSetState(logic::CarState s);  // solo para pruebas por serie: fija el estado como si el auto lo hubiera informado
bool busy();
bool toggling();       // true mientras corre una secuencia ENCENDER/APAGAR (no una consulta)
Phase phase();
Err error();
int step();            // paso en curso de la secuencia (0..SEQ_LEN)
logic::CarState state();
uint32_t voltageMv();
uint32_t stateAgeMs();  // ms desde la ultima respuesta de estado valida (UINT32_MAX si nunca)

}  // namespace car
