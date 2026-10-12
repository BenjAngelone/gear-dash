"""Genera el mapa de sombra de pulsación (8 bits) a partir de cualquier máscara: números de marcha,
logo o íconos nuevos.  Uso:
    python3 make_shadow.py entrada.png salida_glow.png [margen_px=24]
La salida agrega `margen` px por lado. Valor: 255 dentro de la forma; afuera 255·(1 − d/24),
con d = distancia al borde en px (0 a partir de 24 px)."""
import sys
import numpy as np
from PIL import Image
from scipy.ndimage import distance_transform_edt

RANGE = 24.0
SS = 4

src, dst = sys.argv[1], sys.argv[2]
margin = int(sys.argv[3]) if len(sys.argv) > 3 else 24
m = Image.open(src).convert("L")
w, h = m.size
canvas = Image.new("L", (w + 2 * margin, h + 2 * margin), 0)
canvas.paste(m, (margin, margin))
hi = canvas.resize((canvas.width * SS, canvas.height * SS), Image.BILINEAR)
inside = np.asarray(hi) > 127
d = distance_transform_edt(~inside) / SS
v = np.clip(1 - d / RANGE, 0, 1)
v[inside] = 1
out = Image.fromarray((v * 255).astype(np.uint8)).resize(canvas.size, Image.BOX)
out.save(dst, optimize=True)
print(dst, out.size)
