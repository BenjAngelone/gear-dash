#pragma once
#include "alpha_img.h"
#include "tokens.h"

// Composicion de fondo (luz de esquina, logo traslucido, marcas HUD) y sombra de pulsacion.
// Todo se arma en un framebuffer RGB565 propio en PSRAM (`work`) y recien despues se vuelca al
// sprite de LovyanGFX, que dibuja encima numeros e iconos con su alpha.
// Especificacion: gear_dash_menu/design/gear_menu.md §2 y §4.
namespace fx {

struct Bg {
  tk::Rgb color;  // color de la luz
  float corner;   // opacidad pico de la luz de esquina (0..1)
  float logo;     // opacidad del logo traslucido (0..1)
};

void begin();
// Fija el fondo objetivo; si cambia, hace un fundido de BG_FADE_MS (ease-in-out) desde el actual.
void setBg(const Bg& target, uint32_t now);
// Arma el fondo de este frame en `work` (cornerMul modula la luz de esquina: respiracion de la carga).
void bgFrame(uint32_t now, float cornerMul = 1.0f);
// Sombra de pulsacion: mezcla `color` sobre `work` siguiendo el mapa (I = intensidad, R = expansion px).
void shadow(const ShadowMap& m, int x0, int y0, const tk::Rgb& color, float I, float R);
// Marco que se completa (feedback de pulsacion): lineas desde las 4 esquinas, a 12 px del borde, que
// avanzan hacia el medio de cada lado y cierran el cuadrado cuando p = 1.
void frame(float p, const tk::Rgb& color, float level, int thick);
// Destello de toda la pantalla (aditivo) en `color` con opacidad `alpha` (la "explosion").
void flash(const tk::Rgb& color, float alpha);
// true mientras el fondo esta en fundido de color (hay que seguir redibujando).
bool fading(uint32_t now);
// Vuelca `work` al sprite.
void present();

}  // namespace fx
