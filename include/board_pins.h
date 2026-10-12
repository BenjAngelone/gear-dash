#pragma once
// Pines de la Waveshare ESP32-S3-Touch-LCD-1.69.
// Fuente: https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69 (tabla de asignación de GPIO)
// y https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.69 (SYS_EN/SYS_OUT/RTC_INT/BUZZER/BAT_ADC).
// Los pines del LCD/I2C/táctil salen solo de la imagen "Pinout" de la doc oficial, que se leyó
// vía texto extraído: TODO VERIFICAR contra el esquemático (PDF) antes de dar por buena la placa.
// Un listado de terceros (spotpear) difiere (LCD SCLK=18, CS=16, DC=2...) y NO se usó.

// ---- LCD ST7789V2 (SPI, 240x280) -------------------------------------------
#define PIN_LCD_MOSI   7   // LCD_DIN  (docs.waveshare.com)
#define PIN_LCD_SCLK   6   // LCD_CLK  (docs.waveshare.com)
#define PIN_LCD_CS     5   // docs.waveshare.com
#define PIN_LCD_DC     4   // docs.waveshare.com
#define PIN_LCD_RST    8   // docs.waveshare.com
#define PIN_LCD_BL     15  // backlight, PWM (docs.waveshare.com)
#define LCD_OFFSET_Y   20  // el panel 240x280 está desplazado 20 px en la RAM de 320 (indicado en el pedido)

// ---- I2C compartido: CST816T (0x15), QMI8658 (0x6A/0x6B), PCF85063 (0x51) --
#define PIN_I2C_SDA    11  // docs.waveshare.com
#define PIN_I2C_SCL    10  // docs.waveshare.com
#define I2C_FREQ_HZ    400000

// ---- Táctil CST816T ----------------------------------------------------------
#define PIN_TP_INT     14  // docs.waveshare.com
#define PIN_TP_RST     13  // docs.waveshare.com
#define CST816_ADDR    0x15

// ---- IMU QMI8658 / RTC PCF85063 -------------------------------------------
#define PIN_IMU_INT    38  // QMI_INT1 (docs.waveshare.com; sin uso, se sondea por I2C)
#define PIN_RTC_INT    39  // wiki.waveshare.com (placa nueva); sin uso
#define QMI8658_ADDR_A 0x6B
#define QMI8658_ADDR_B 0x6A
#define PCF85063_ADDR  0x51

// ---- Botones / energía -----------------------------------------------------
#define PIN_BTN_BOOT   0   // BOOT, activo en bajo
#define PIN_SYS_OUT    40  // lectura del botón PWR (baja al pulsar) - wiki.waveshare.com
#define PIN_SYS_EN     41  // "power latch": mantener en ALTO para seguir encendida con batería
#define PIN_BAT_ADC    1   // VBAT = lectura * 3 (divisor) - wiki.waveshare.com
#define PIN_BUZZER     42  // sin uso
// El botón RST va a CHIP_PU, no es un GPIO.
