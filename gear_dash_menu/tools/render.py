"""Rasterizado de máscaras, mapas de sombra y composición de mockups 240×280."""
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy.ndimage import distance_transform_edt
from shapely.geometry import Polygon, MultiPolygon
from shapely import affinity

SS = 8  # supersampling

FONT_DIR = "/home/claude/gear/package/files/"
FONT_BOLD = FONT_DIR + "barlow-semi-condensed-latin-700-normal.woff"
FONT_SEMI = FONT_DIR + "barlow-semi-condensed-latin-600-normal.woff"

# Paleta del sistema
BG = (0, 0, 0)
WHITE = (255, 255, 255)
YELLOW = (255, 204, 0)
RED = (255, 59, 48)
GRAY = (120, 120, 120)
PANEL = (22, 22, 22)
BORDER = (60, 60, 60)
TRACK = (42, 42, 42)


def _polys(g):
    if isinstance(g, Polygon):
        return [g]
    if isinstance(g, MultiPolygon):
        return list(g.geoms)
    return [p for p in getattr(g, "geoms", []) if isinstance(p, Polygon)]


def raster(geom, w, h, ox=0.0, oy=0.0, ss=SS):
    """Máscara L (0..255) de la geometría, desplazada (ox, oy), con antialias por supersampling."""
    im = Image.new("L", (w * ss, h * ss), 0)
    d = ImageDraw.Draw(im)
    g = affinity.scale(affinity.translate(geom, ox, oy), ss, ss, origin=(0, 0))
    for p in _polys(g):
        d.polygon(list(p.exterior.coords), fill=255)
        for hole in p.interiors:
            d.polygon(list(hole.coords), fill=0)
    return im.resize((w, h), Image.BOX)


def raster_hi(geom, w, h, ox=0.0, oy=0.0, ss=4):
    im = Image.new("L", (w * ss, h * ss), 0)
    d = ImageDraw.Draw(im)
    g = affinity.scale(affinity.translate(geom, ox, oy), ss, ss, origin=(0, 0))
    for p in _polys(g):
        d.polygon(list(p.exterior.coords), fill=255)
        for hole in p.interiors:
            d.polygon(list(hole.coords), fill=0)
    return im


# ---------------- mapa de sombra (pulsación) ----------------
SHADOW_RANGE = 24.0  # px de distancia codificados en el mapa (0..24 → 255..0)


def shadow_map_from_hi(mask_hi, ss):
    """mask_hi: L a ss× resolución. Devuelve mapa L a 1× donde
    v = 255 dentro de la forma y v = 255·(1 − d/24) afuera (d = distancia al borde en px)."""
    m = np.asarray(mask_hi) > 127
    d = distance_transform_edt(~m) / ss
    v = np.clip(1.0 - d / SHADOW_RANGE, 0, 1)
    v[m] = 1.0
    im = Image.fromarray((v * 255).astype(np.uint8))
    w, h = mask_hi.size
    return im.resize((w // ss, h // ss), Image.BOX)


# ---------------- curva de la sombra ----------------
A_MAX_YELLOW = 0.30     # opacidad pico del amarillo en el borde de la forma
SOFT = 6.0              # extensión inicial (px)
GROW = 12.0             # expansión adicional 0 → +12 px


def press_params(p):
    """p = t/T (0..1). Devuelve (I, R): intensidad 0..1 y expansión en px."""
    p = min(max(p, 0.0), 1.0)
    R = GROW * (1 - (1 - p) ** 2)          # ease-out quad: crece rápido al principio
    I = 0.40 + 0.60 * p                     # lineal con piso: se ve desde el primer frame
    return I, R


def halo_alpha(vmap, I, R, a_max):
    """Alpha (0..1) del halo a partir del mapa de sombra; misma fórmula que el firmware (LUT de 256)."""
    v = np.asarray(vmap, dtype=np.float32) / 255.0
    d = SHADOW_RANGE * (1.0 - v)
    g = np.clip(1.0 - d / (SOFT + R), 0, 1) ** 2
    return np.clip(a_max * I * g, 0, 1)


# ---------------- composición ----------------
def new_screen():
    return np.zeros((280, 240, 3), dtype=np.float32)


def paint(scr, alpha, color, x=0, y=0):
    """Pinta color con alpha (H×W, 0..1) en scr (en sitio)."""
    a = np.asarray(alpha, dtype=np.float32)
    if a.max() > 1.0:
        a = a / 255.0
    h, w = a.shape
    sub = scr[y:y + h, x:x + w]
    c = np.array(color, dtype=np.float32)
    sub[:] = sub * (1 - a[..., None]) + c * a[..., None]


def to_img(scr):
    return Image.fromarray(np.clip(scr, 0, 255).astype(np.uint8), "RGB")


def text_mask(txt, font_path, size, w=240, h=280, cx=120, cy=None, top=None, anchor="mm"):
    im = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(im)
    f = ImageFont.truetype(font_path, size)
    if top is not None:
        d.text((cx, top), txt, font=f, fill=255, anchor="mt")
    else:
        d.text((cx, cy), txt, font=f, fill=255, anchor=anchor)
    return im


def text_mask_hi(txt, font_path, size, w, h, cx, cy, ss=4, anchor="mm"):
    im = Image.new("L", (w * ss, h * ss), 0)
    d = ImageDraw.Draw(im)
    f = ImageFont.truetype(font_path, size * ss)
    d.text((cx * ss, cy * ss), txt, font=f, fill=255, anchor=anchor)
    return im
