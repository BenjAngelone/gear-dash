#pragma once
#include <Arduino.h>

#include "logic.h"

// Pantallas: inicio, marchas (con animacion de cambio), calibracion y menu del boton unico.
// Diseño: gear_dash_menu/design/gear_menu.md
namespace ui {

// Lo que el menu tiene que mostrar (lo arma main segun car:: y los temporizadores de resultado)
struct MenuView {
  enum Kind { CONSULTING, REST, SENDING, DONE, ERROR, PIN, ENROLL } kind = CONSULTING;
  logic::CarState state = logic::CAR_UNKNOWN;  // REST/DONE: estado actual del auto
  bool turningOn = true;                       // SENDING: true = ARRANCANDO, false = APAGANDO
  int step = 0;                                // SENDING: comandos ya enviados (0..5)
  bool noResponse = false;                     // ERROR: true = SIN RESPUESTA
  uint32_t kindSince = 0;                      // ms en que empezo este Kind (destellos)
  enum Auth : uint8_t { AUTH_NONE, AUTH_PHONE, AUTH_PIN } auth = AUTH_NONE;  // REST: quien habilito ENCENDER
  // PIN: teclado del PIN de respaldo (auto apagado y sin telefono cerca)
  int pinLen = 0;
  bool pinForEnroll = false;                   // se pidio el PIN para vincular un telefono
  uint32_t pinWrongAt = 0;                     // ms del ultimo error (0 = ninguno)
  uint32_t lockLeftMs = 0;                     // bloqueado tras 3 errores
  int pressedKey = -1;
  // ENROLL: vinculacion de un telefono
  uint8_t enroll = 0;                          // phone::Enroll
  uint32_t passkey = 0;
  uint32_t enrollLeftMs = 0;
  int phones = 0;
};

// Feedback de pulsacion: sombra blanca (I, R), marco que se cierra (p) y explosion de color (flash)
struct PressView {
  float I = 0, R = 0;
  float p = 0;
  float flash = 0;
  tk::Rgb flashColor{255, 255, 255};
};

void drawSplash();  // directo al panel (con 3 cuadros de prueba de color)
void drawGear(uint32_t now, const PressView& pv);
void drawCalibration(uint32_t now);
void drawMenu(uint32_t now, const MenuView& v, const PressView& pv);

// zonas tactiles
void bench();  // mide cuanto tarda un cuadro con animacion (log serie, una vez al arrancar)
bool inOpenZone(int x, int y);  // MARCHAS: logo o numero
bool inButton(int x, int y);    // MENU: toda la pantalla (sin placa) menos el margen de 12 px
constexpr int KEY_DEL = 10, KEY_BT = 11;
int keyAt(int x, int y);        // PIN: 0..9, KEY_DEL, KEY_BT o -1

}  // namespace ui
