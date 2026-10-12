#pragma once
#include <Arduino.h>

// Deteccion de marcha por IMU (QMI8658). La direccion de la gravedad se compara con una nube de puntos
// por marcha (calibracion guiada que recorre el juego de la palanca); el giroscopio decide cuando la
// palanca se movio, y solo entonces se vuelve a decidir la marcha.
namespace gear {

enum Gear { G_N, G_1R, G_2, G_3, G_4, G_5, G_COUNT };

bool begin();                 // requiere Wire iniciado; carga la calibracion guardada
void update(uint32_t now);    // llamar seguido (lee a 100 Hz)

bool imuOk();
int current();                // -1 si indefinida
int previous();               // marcha anterior (-1 si no hay)
uint32_t changedAt();         // ms del ultimo cambio de marcha

bool calibrating();
void startCal();
int calStep();                // 0..G_COUNT-1
bool calWaitingMove();
bool calExploring();              // recorriendo el juego de la palanca dentro de la marcha
int calPoints();                  // puntos juntados para la marcha actual
float calProgress(uint32_t now);  // 0..1 de la fase actual (quieta / explorar)

bool leverQuiet(uint32_t now);    // true si la palanca no se movio en LEVER_QUIET_MS
uint32_t lastMotionAt();          // ms del ultimo movimiento de la palanca
bool consumeJolt();               // true si hubo una sacudida/movimiento brusco desde la ultima llamada
void debugLine(char* out, size_t n);

}  // namespace gear
