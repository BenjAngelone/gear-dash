#pragma once
// Sistema de diseño de Gear Dash. Única fuente de valores visuales, de tiempo y de filtros.
// Documento: design/gear_design.md
#include <stdint.h>

namespace tk {

// ---- pantalla -----------------------------------------------------------------
constexpr int W = 240, H = 280;

// ---- color (RGB888) -----------------------------------------------------------
struct Rgb { uint8_t r, g, b; };
constexpr Rgb BG{0, 0, 0};            // fondo
constexpr Rgb FG{255, 255, 255};      // logo y numero
constexpr Rgb ACCENT{255, 204, 0};    // amarillo Renault: estado "encendido", progreso, destello
constexpr Rgb DANGER{255, 59, 48};    // error
constexpr Rgb MUTE{120, 120, 120};    // texto secundario / inactivo
constexpr Rgb PANEL{22, 22, 22};      // fondo de boton
constexpr Rgb LINE{60, 60, 60};       // borde de boton en reposo
constexpr Rgb HUD{60, 60, 60};        // marcas de esquina #3C3C3C
constexpr Rgb TRACK{42, 42, 42};      // pista de la carga / pendientes #2A2A2A
constexpr Rgb DIM_ACCENT{128, 102, 0};  // comando en curso #806600
constexpr Rgb GOOD{0, 230, 90};       // explosion al ENCENDER

// ---- medidas (grilla de 8 px) -------------------------------------------------
constexpr int GRID = 8;
constexpr int GEAR_CY = 172;          // centro vertical del numero
constexpr int BTN_X = 16, BTN_Y = 92, BTN_W = 208, BTN_H = 128;
constexpr int BTN_CUT = 16;           // esquina cortada a 45 grados
constexpr int RING_THICK = 4;

// ---- tiempos (ms) -------------------------------------------------------------
constexpr uint32_t GEAR_ANIM_MS = 320;     // animacion completa de cambio de marcha
constexpr uint32_t GEAR_OUT_MS = 140;      // salida del numero anterior
constexpr uint32_t MENU_OPEN_HOLD_MS = 600;
constexpr uint32_t ACTION_HOLD_MS = 1000;
constexpr uint32_t MENU_IDLE_MS = 10000;   // sin toques -> vuelve a marchas
constexpr uint32_t DONE_SHOW_MS = 800;    // LISTO visible; al encender vuelve a marchas, al apagar duerme
constexpr uint32_t ERROR_SHOW_MS = 3000;  // ERROR visible (o hasta un toque)
constexpr uint32_t BG_FADE_MS = 300;       // fundido de color del fondo
constexpr uint32_t LEVER_QUIET_MS = 700;   // palanca quieta antes de aceptar toques/abrir menu
constexpr uint32_t STATE_REFRESH_MS = 4000;
constexpr uint32_t IDLE_CHECK_MS = 270000;  // sin movimiento: consultar el estado; si esta apagado, dormir
constexpr uint32_t SLEEP_WAKE_GUARD_MS = 3000;  // tras dormirse se ignoran sacudidas un rato

// ---- filtro de toques falsos --------------------------------------------------
constexpr uint32_t TOUCH_MIN_PRESS_MS = 60;
constexpr int TOUCH_EDGE_PX = 1;   // solo el pixel del borde: el teclado de PIN llega casi hasta abajo (y=272..279)
constexpr int TOUCH_MAX_DRIFT_PX = 40;
constexpr uint32_t TOUCH_DEAD_AFTER_OPEN_MS = 300;

// ---- palanca ------------------------------------------------------------------
constexpr float LEVER_MOTION_DELTA = 0.04f;  // cambio de direccion (unitaria) en 100 ms = "se movio"
constexpr float JOLT_DELTA = 0.08f;          // cambio brusco de inclinacion en 100 ms (~5 grados): despierta
constexpr float JOLT_MAG = 0.35f;            // golpe: |a| se aparta 0,35 g de 1 g: despierta

// ---- deteccion de marcha (ver gear.cpp) ---------------------------------------------
// La marcha queda fija y solo se vuelve a decidir tras un movimiento real de la palanca, con la
// palanca asentada: asi las aceleraciones, frenadas, curvas y pendientes no la cambian.
constexpr float GYRO_SHIFT_DPS = 40.0f;      // giro de la palanca que abre la ventana de cambio
constexpr float GYRO_STILL_DPS = 8.0f;       // por debajo: palanca asentada
constexpr float ACC_EXT_MAX = 0.08f;         // |a| a mas de 0,08 g de 1 g = aceleracion externa: no clasificar
constexpr uint32_t SHIFT_WINDOW_MS = 1500;   // tras el ultimo movimiento, tiempo para decidir la nueva marcha
constexpr uint32_t GEAR_CONFIRM_MS = 150;    // lectura asentada y estable antes de aceptar
constexpr float GEAR_MAX_DIST = 0.08f;       // mas lejos que esto de toda nube calibrada = no se sabe: conservar
constexpr float GEAR_MIN_CONF = 0.25f;       // 1 - d1/d2: por debajo, ambiguo entre dos marchas

// ---- llave: telefono (BLE) o PIN ----------------------------------------------
constexpr int PHONE_NEAR_RSSI = -90;           // tolerante: alcanza con que este en el auto o al lado
constexpr uint32_t PHONE_NEAR_WINDOW_MS = 30000;  // visto cerca en los ultimos 30 s = presente
constexpr uint16_t PHONE_SCAN_INTERVAL_MS = 100;
constexpr uint16_t PHONE_SCAN_WINDOW_MS = 100;    // escaneo continuo: ahora es corto y a pedido (3 s), no hace falta ahorrar ciclo de trabajo
constexpr uint32_t PHONE_SCAN_GRACE_MS = 2500;   // tras abrir el menu: espera al telefono antes de pedir el PIN
constexpr uint32_t RADIO_KEEP_MS = 15000;        // la radio WiFi queda encendida este tiempo tras el ultimo uso
constexpr uint32_t TOUCH_DROPOUT_MS = 120;       // cortes de lectura del CST816 mas cortos que esto no sueltan el toque
constexpr uint32_t PHONE_ENROLL_MS = 90000;       // ventana para vincular un telefono
constexpr uint32_t PIN_OK_MS = 60000;             // PIN correcto: habilita ENCENDER hasta salir del menu
constexpr int PIN_LEN = 4;
constexpr int PIN_MAX_FAILS = 3;
constexpr uint32_t PIN_LOCK_MS = 30000;           // tras 3 errores
constexpr uint32_t PIN_WRONG_SHOW_MS = 600;       // sacudida de los puntos al errar
constexpr uint32_t ENROLL_RESULT_MS = 2000;       // VINCULADO / FALLO visible

// teclado: 3 x 4 teclas de 64 x 44 con 8 px de separacion
constexpr int KEY_W = 64, KEY_H = 44, KEY_GAP = 8;
constexpr int KEYPAD_X = 16, KEYPAD_Y = 72;
constexpr int PIN_DOTS_Y = 52;

}  // namespace tk
