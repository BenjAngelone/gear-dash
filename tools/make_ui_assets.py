"""Convierte los PNG de gear_dash_menu/icons (mascaras L8) a arreglos A8 -> src/ui_assets.h.

Fuente de verdad del diseño: gear_dash_menu/design/gear_menu.md.
Uso: python tools/make_ui_assets.py
"""
from pathlib import Path
from PIL import Image
import numpy as np

ROOT = Path(__file__).resolve().parent.parent
ICONS = ROOT / "gear_dash_menu" / "icons"

# (nombre C, archivo, tipo)  tipo: img = AlphaImg, map = ShadowMap a resolucion completa
ITEMS = [
    ("ICON_ENCENDER", "encender.png", "img"),
    ("ICON_APAGAR", "apagar.png", "img"),
    ("ICON_ERROR", "error.png", "img"),
    ("ICON_LISTO", "listo.png", "img"),
    ("GL_ENCENDER", "encender_glow.png", "map2"),   # mapas de sombra a media resolucion
    ("GL_APAGAR", "apagar_glow.png", "map2"),
    ("GL_ERROR", "error_glow.png", "map2"),
    ("GL_LISTO", "listo_glow.png", "map2"),
    ("CARGA_PISTA", "carga_pista.png", "img"),
    ("FONDO_ESQUINA", "fondo_esquina.png", "img"),
    ("LOGO_FONDO", "logo_fondo.png", "img"),
    ("LOGO_MARCHAS", "logo_marchas.png", "img"),
]


def main():
    out = ["// Generado por tools/make_ui_assets.py: no editar a mano.", "#pragma once", '#include "alpha_img.h"']
    total = 0
    for name, fn, kind in ITEMS:
        im = Image.open(ICONS / fn).convert("L")
        if kind == "map2":
            im = im.resize((im.width // 2, im.height // 2), Image.BOX)
        a = np.asarray(im)
        h, w = a.shape
        total += w * h
        vals = ",".join(str(int(v)) for v in a.flatten())
        out.append(f"inline const uint8_t {name}_A[] PROGMEM = {{{vals}}};")
        if kind == "img":
            out.append(f"inline const AlphaImg {name} = {{{w}, {h}, {name}_A}};")
        else:
            out.append(f"inline const ShadowMap {name} = {{{w}, {h}, {2 if kind == 'map2' else 1}, {name}_A}};")
        print(f"{name:14s} {w}x{h}")
    # Carga: en vez de la hoja de 12 frames (110 KB) se guarda el id de segmento (0..11, 255 = nada) de
    # cada pixel de la pista; el frame f se arma en el firmware (brillo TAIL[(f - id) mod 12]).
    frames = np.asarray(Image.open(ICONS / "carga_frames.png").convert("L")).astype(int)
    fr = np.stack([frames[:, i * 96:(i + 1) * 96] for i in range(12)])
    pista = np.asarray(Image.open(ICONS / "carga_pista.png").convert("L"))
    ids = np.where(pista > 0, fr.argmax(axis=0), 255).astype(np.uint8)
    TAIL = [1.0, 0.55, 0.28, 0.12] + [0.0] * 8
    err = 0
    for f in range(12):
        rec = np.zeros((96, 96))
        m = ids != 255
        k = (f - ids[m].astype(int)) % 12
        rec[m] = np.array(TAIL)[k] * pista[m]
        err = max(err, np.abs(rec - fr[f]).max())
    print("error maximo de reconstruccion de la carga (0..255):", err)
    total += ids.size
    out.append(f"inline const uint8_t CARGA_ID_A[] PROGMEM = {{{','.join(str(int(v)) for v in ids.flatten())}}};")
    out.append(f"inline const AlphaImg CARGA_ID = {{96, 96, CARGA_ID_A}};")
    (ROOT / "src/ui_assets.h").write_text("\n".join(out) + "\n")
    print("total bytes ui_assets:", total)


main()
