#pragma once
#include <LovyanGFX.hpp>

#include "board_pins.h"
#include "tokens.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_SPI bus_;

 public:
  LGFX() {
    {
      auto c = bus_.config();
      c.spi_host = SPI3_HOST;
      c.spi_mode = 0;
      c.freq_write = 80000000;  // 40 MHz tardaba 30 ms por cuadro completo; si hay ruido en la imagen, volver a 40000000
      c.freq_read = 16000000;
      c.spi_3wire = true;
      c.use_lock = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = PIN_LCD_SCLK;
      c.pin_mosi = PIN_LCD_MOSI;
      c.pin_miso = -1;
      c.pin_dc = PIN_LCD_DC;
      bus_.config(c);
      panel_.setBus(&bus_);
    }
    {
      auto c = panel_.config();
      c.pin_cs = PIN_LCD_CS;
      c.pin_rst = PIN_LCD_RST;
      c.pin_busy = -1;
      c.panel_width = tk::W;
      c.panel_height = tk::H;
      c.memory_width = 240;
      c.memory_height = 320;
      c.offset_x = 0;
      c.offset_y = LCD_OFFSET_Y;
      c.offset_rotation = 0;
      c.readable = false;
      c.invert = true;      // TODO VERIFICAR: colores en negativo -> false
      c.rgb_order = true;   // verificado con cuadros de prueba: el panel tiene R y B invertidos
      c.dlen_16bit = false;
      c.bus_shared = false;
      panel_.config(c);
    }
    setPanel(&panel_);
  }
};

extern LGFX tft;
extern LGFX_Sprite spr;

namespace display {
void begin(bool rot180);
void setRotation180(bool f);
void present();  // sprite -> panel
void setBacklight(uint8_t duty);  // 0 = apagada
// color del sistema de diseño, con nivel de brillo 0..1 (mezcla contra el fondo negro)
uint16_t col(const tk::Rgb& c, float level = 1.0f);  // uint16_t: LovyanGFX lo toma como RGB565 (uint32_t seria RGB888)
}  // namespace display
