"""Genera icons/*.png (máscara 96×96), icons/*_glow.png (mapa de sombra 168×168),
la hoja de carga y los SVG de referencia."""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from PIL import Image
from shapely.ops import unary_union
import geom, render as R

OUT = os.path.join(os.path.dirname(__file__), "..", "icons")
os.makedirs(OUT, exist_ok=True)


def svg_path(g, ox=0, oy=0):
    parts = []
    for p in R._polys(g):
        for ring in [p.exterior, *p.interiors]:
            pts = list(ring.coords)[:-1]
            parts.append("M" + " L".join(f"{x+ox:.2f} {y+oy:.2f}" for x, y in pts) + " Z")
    return " ".join(parts)


paths = {}
for name, fn in geom.ICONS.items():
    g = fn()
    R.raster(g, 96, 96).save(os.path.join(OUT, f"{name}.png"), optimize=True)
    hi = R.raster_hi(g, 168, 168, 36, 36, ss=4)
    R.shadow_map_from_hi(hi, 4).save(os.path.join(OUT, f"{name}_glow.png"), optimize=True)
    paths[name] = svg_path(g)
    with open(os.path.join(OUT, f"{name}.svg"), "w") as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96" width="96" height="96">'
                f'<path fill="#FFFFFF" fill-rule="evenodd" d="{paths[name]}"/></svg>\n')

# --- carga: pista + 12 frames (valor = brillo del amarillo) ---
segs = geom.loader_segments()
R.raster(unary_union(segs), 96, 96).save(os.path.join(OUT, "carga_pista.png"), optimize=True)
seg_masks = [np.asarray(R.raster(s, 96, 96), dtype=np.float32) / 255 for s in segs]
sheet = np.zeros((96, 96 * geom.LOAD_N), dtype=np.float32)
for fr in range(geom.LOAD_N):
    acc = np.zeros((96, 96), dtype=np.float32)
    for i, m in enumerate(seg_masks):
        k = (fr - i) % geom.LOAD_N
        if k < len(geom.LOAD_TAIL):
            acc = np.maximum(acc, m * geom.LOAD_TAIL[k])
    sheet[:, fr * 96:(fr + 1) * 96] = acc
Image.fromarray((sheet * 255).astype(np.uint8)).save(os.path.join(OUT, "carga_frames.png"), optimize=True)

import json
segs_svg = [svg_path(s) for s in segs]
json.dump({"icons": paths, "loader": segs_svg}, open(os.path.join(OUT, "..", "tools", "paths.json"), "w"))

tot = 0
for f in sorted(os.listdir(OUT)):
    s = os.path.getsize(os.path.join(OUT, f)); tot += s
    print(f"{f:22s} {s:6d} B")
print("total", tot)
