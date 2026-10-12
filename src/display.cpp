#include "display.h"

#include "fbaccess.h"

LGFX tft;
LGFX_Sprite spr(&tft);

namespace fb {
bool swapped = false;
uint16_t* buf() { return (uint16_t*)spr.getBuffer(); }
}  // namespace fb

namespace display {

void begin(bool rot180) {
  ledcAttach(PIN_LCD_BL, 5000, 8);
  ledcWrite(PIN_LCD_BL, 0);
  tft.init();
  tft.setRotation(rot180 ? 2 : 0);
  tft.fillScreen(TFT_BLACK);
  spr.setColorDepth(16);
  spr.setPsram(true);
  if (!spr.createSprite(tk::W, tk::H)) {
    spr.setPsram(false);
    spr.createSprite(tk::W, tk::H);
  }
  // detectar si el sprite guarda los bytes invertidos: se dibuja rojo puro y se mira el valor crudo
  spr.drawPixel(0, 0, (uint16_t)0xF800);
  fb::swapped = fb::buf() && fb::buf()[0] != 0xF800;
  spr.fillSprite(TFT_BLACK);
  ledcWrite(PIN_LCD_BL, 220);
}

void setRotation180(bool f) { tft.setRotation(f ? 2 : 0); }

void present() { spr.pushSprite(0, 0); }

void setBacklight(uint8_t duty) { ledcWrite(PIN_LCD_BL, duty); }

uint16_t col(const tk::Rgb& c, float level) {
  if (level < 0) level = 0;
  if (level > 1) level = 1;
  return tft.color565((uint8_t)(c.r * level), (uint8_t)(c.g * level), (uint8_t)(c.b * level));
}

}  // namespace display
