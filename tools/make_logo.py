"""Convierte assets/renault.png (logo negro sobre blanco) a src/logo.h (RGB565).

Genera dos imagenes ya mezcladas con su fondo (sin alpha en el firmware):
  LOGO_SPLASH  rombo grande, blanco sobre negro (pantalla de inicio)
  LOGO_SMALL   rombo chico, blanco sobre negro (cabecera de la pantalla principal)
La palabra RENAULT del original se descarta a proposito.
Uso: python tools/make_logo.py
"""
from pathlib import Path
from PIL import Image
import numpy as np

ROOT = Path(__file__).resolve().parent.parent
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)

im = Image.open(ROOT / "assets/renault.png").convert("RGBA")
a = np.asarray(im).astype(float) / 255.0
cov = (1 - a[..., :3].mean(axis=2)) * a[..., 3]  # cobertura de tinta 0..1

rows = np.where(cov.max(axis=1) > 0.05)[0]
cols = np.where(cov.max(axis=0) > 0.05)[0]
cov = cov[rows[0]:rows[-1] + 1, cols[0]:cols[-1] + 1]

# separar rombo (arriba) del texto (abajo) por la primera fila vacia larga
empty = cov.max(axis=1) <= 0.05
gap = next(i for i in range(len(empty)) if empty[i] and i > cov.shape[0] * 0.4)
diamond = cov[:gap]
dr = np.where(diamond.max(axis=1) > 0.05)[0]
dc = np.where(diamond.max(axis=0) > 0.05)[0]
diamond = diamond[dr[0]:dr[-1] + 1, dc[0]:dc[-1] + 1]


def render(c, width, ink, bg):
    h = round(c.shape[0] * width / c.shape[1])
    img = Image.fromarray((c * 255).astype(np.uint8)).resize((width, h), Image.LANCZOS)
    m = np.asarray(img).astype(float)[..., None] / 255.0
    rgb = np.array(ink) * m + np.array(bg) * (1 - m)
    r, g, b = (rgb[..., i].astype(int) for i in range(3))
    px = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    return w_h(px), px


def w_h(px):
    return px.shape[1], px.shape[0]


def emit(name, c, width, ink, bg):
    (w, h), px = render(c, width, ink, bg)
    vals = ",".join(f"0x{v:04X}" for v in px.flatten())
    return (f"constexpr int {name}_W = {w}, {name}_H = {h};\n"
            f"static const uint16_t {name}[] PROGMEM = {{{vals}}};\n")


out = "// Generado por tools/make_logo.py: no editar a mano.\n#pragma once\n#include <Arduino.h>\n"
out += emit("LOGO_SPLASH", diamond, 130, WHITE, BLACK)
out += emit("LOGO_SMALL", diamond, 46, WHITE, BLACK)
(ROOT / "src/logo.h").write_text(out)
print("ok", cov.shape, diamond.shape)
