"""Renderiza numeros y etiquetas angulares a bitmaps alpha de 8 bits -> src/glyph_data.h.

Fuente: Bahnschrift (DIN) de Windows. Se rasteriza con supersampling x4.
Tambien genera el mapa de sombra de pulsacion de cada numero de marcha (ver
gear_dash_menu/design/gear_menu.md §4.2): v = 255 dentro de la forma; afuera 255*(1 - d/24).
Los mapas de los numeros se guardan a MEDIA resolucion (scale = 2) para ahorrar flash.
Uso: python tools/make_glyphs.py
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import numpy as np
from scipy.ndimage import distance_transform_edt

ROOT = Path(__file__).resolve().parent.parent
FONT = r"C:\Windows\Fonts\bahnschrift.ttf"
SS = 4
SHADOW_RANGE = 24.0
MARGIN = 24

# (nombre C, texto, altura de mayuscula en px, variante, espaciado extra entre letras en px, con sombra)
ITEMS = [
    ("G_N", "N", 150, "Bold SemiCondensed", 0, True),
    ("G_2", "2", 150, "Bold SemiCondensed", 0, True),
    ("G_3", "3", 150, "Bold SemiCondensed", 0, True),
    ("G_4", "4", 150, "Bold SemiCondensed", 0, True),
    ("G_5", "5", 150, "Bold SemiCondensed", 0, True),
    ("G_1R", "1/R", 100, "Bold SemiCondensed", 6, True),
    # etiquetas del menu: Bahnschrift SemiBold SemiCondensed ~26 px (altura de mayuscula ~19 px)
    ("M_ENCENDER", "ENCENDER", 19, "SemiBold SemiCondensed", 2, False),
    ("M_APAGAR", "APAGAR", 19, "SemiBold SemiCondensed", 2, False),
    ("M_ARRANCANDO", "ARRANCANDO", 19, "SemiBold SemiCondensed", 2, False),
    ("M_APAGANDO", "APAGANDO", 19, "SemiBold SemiCondensed", 2, False),
    ("M_CONSULTANDO", "CONSULTANDO", 19, "SemiBold SemiCondensed", 2, False),
    ("M_LISTO", "LISTO", 19, "SemiBold SemiCondensed", 2, False),
    ("M_SINRESP", "SIN RESPUESTA", 19, "SemiBold SemiCondensed", 2, False),
    ("M_ERROR", "ERROR", 19, "SemiBold SemiCondensed", 2, False),
    ("S_ERROR", "ERROR", 12, "Bold", 3, False),
]


def font_for(cap_px, variant):
    f = ImageFont.truetype(FONT, 100)
    f.set_variation_by_name(variant)
    bb = f.getbbox("H")
    size = round(100 * cap_px * SS / (bb[3] - bb[1]))
    f = ImageFont.truetype(FONT, size)
    f.set_variation_by_name(variant)
    return f


def render(text, cap_px, variant, spacing):
    f = font_for(cap_px, variant)
    asc, desc = f.getmetrics()
    adv = [f.getlength(ch) for ch in text]
    wid = int(sum(adv) + spacing * SS * (len(text) - 1)) + 8 * SS
    pad = 3 * SS
    im = Image.new("L", (wid + 2 * pad, asc + desc + 2 * pad), 0)
    d = ImageDraw.Draw(im)
    x = pad
    for ch, a in zip(text, adv):
        d.text((x, pad), ch, font=f, fill=255)
        x += a + spacing * SS
    arr = np.asarray(im)
    ys = np.where(arr.max(axis=1) > 8)[0]
    xs = np.where(arr.max(axis=0) > 8)[0]
    arr = arr[ys[0]:ys[-1] + 1, xs[0]:xs[-1] + 1]
    h, w = arr.shape[0] // SS, arr.shape[1] // SS
    return np.asarray(Image.fromarray(arr[:h * SS, :w * SS]).resize((w, h), Image.BOX))


def shadow_map(mask):
    """Mapa de distancia (mismo algoritmo que gear_dash_menu/tools/make_shadow.py), media resolucion."""
    h, w = mask.shape
    W, H = w + 2 * MARGIN, h + 2 * MARGIN
    W += W % 2
    H += H % 2
    canvas = Image.new("L", (W, H), 0)
    canvas.paste(Image.fromarray(mask), (MARGIN, MARGIN))
    hi = canvas.resize((W * SS, H * SS), Image.BILINEAR)
    inside = np.asarray(hi) > 127
    d = distance_transform_edt(~inside) / SS
    v = np.clip(1 - d / SHADOW_RANGE, 0, 1)
    v[inside] = 1
    full = Image.fromarray((v * 255).astype(np.uint8)).resize((W, H), Image.BOX)
    return np.asarray(full.resize((W // 2, H // 2), Image.BOX))


def emit_alpha(out, name, a):
    h, w = a.shape
    out.append(f"inline const uint8_t {name}_A[] PROGMEM = {{{','.join(str(int(v)) for v in a.flatten())}}};")
    out.append(f"inline const AlphaImg {name} = {{{w}, {h}, {name}_A}};")
    return w * h


def emit_shadow(out, name, a, scale):
    h, w = a.shape
    out.append(f"inline const uint8_t {name}_A[] PROGMEM = {{{','.join(str(int(v)) for v in a.flatten())}}};")
    out.append(f"inline const ShadowMap {name} = {{{w}, {h}, {scale}, {name}_A}};")
    return w * h


def main():
    out = ["// Generado por tools/make_glyphs.py: no editar a mano.", "#pragma once", '#include "alpha_img.h"']
    total = 0
    for name, text, cap, variant, sp, shadow in ITEMS:
        a = render(text, cap, variant, sp)
        total += emit_alpha(out, name, a)
        msg = f"{name:14s} {a.shape[1]}x{a.shape[0]}"
        if shadow:
            sh = shadow_map(a)
            total += emit_shadow(out, "SH_" + name, sh, 2)
            msg += f"  sombra {sh.shape[1]}x{sh.shape[0]} (x2)"
        print(msg)
    (ROOT / "src/glyph_data.h").write_text("\n".join(out) + "\n")
    print("total bytes glyph_data:", total)


main()
