"""Geometría de los íconos del menú de Gear Dash (espacio 96×96, origen arriba-izquierda).

Lenguaje: rombo de la marca, trazo uniforme de 10 px, terminaciones cortadas a 45°,
sin curvas. Todo se construye con polígonos (shapely) y se rasteriza a máscara de 8 bits.
"""
from shapely.geometry import Polygon, LineString, box
from shapely import affinity
import math

S = 96          # lado de la máscara
C = 48          # centro
STROKE = 10     # trazo uniforme de los íconos (mínimo pedido: 6)


def diamond(h, cx=C, cy=C):
    """Rombo (cuadrado a 45°) de semidiagonal h."""
    return Polygon([(cx, cy - h), (cx + h, cy), (cx, cy + h), (cx - h, cy)])


def centered(g):
    minx, miny, maxx, maxy = g.bounds
    return affinity.translate(g, C - (minx + maxx) / 2, C - (miny + maxy) / 2)


def encender():
    """Rombo hueco abierto arriba + barra vertical que entra por la abertura.
    Lectura: "contacto": la barra (I) entra en el rombo. Corte vertical del aro
    sobre un brazo a 45° = terminación biselada a 45°."""
    H = 44
    ring = diamond(H).difference(diamond(H - STROKE * math.sqrt(2)))
    gap = box(C - 14, 0, C + 14, C)                # abertura superior
    ring = ring.difference(gap)
    bar = box(C - STROKE / 2, 10, C + STROKE / 2, 54)
    return ring.union(bar)


def apagar():
    """Rombo macizo con ranura horizontal: la barra de ENCENDER "acostada" = contacto cortado.
    Se distingue de ENCENDER por masa (lleno vs. contorno), no solo por detalle."""
    H = 44
    slot = box(C - 22, C - STROKE / 2, C + 22, C + STROKE / 2)
    return diamond(H).difference(slot)


def error():
    """Placa octogonal (cuadrado con esquinas cortadas a 45°, como la placa del panel)
    con un "!" de barra + punto cuadrado. Silueta distinta de los rombos."""
    o = 6
    ch = 24
    outer = Polygon([(o + ch, o), (S - o - ch, o), (S - o, o + ch), (S - o, S - o - ch),
                     (S - o - ch, S - o), (o + ch, S - o), (o, S - o - ch), (o, o + ch)])
    sw = 9
    inner = outer.buffer(-sw, join_style="mitre")
    bar = box(C - 5, 25, C + 5, 55)
    dot = box(C - 5, 62, C + 5, 72)
    return outer.difference(inner).union(bar).union(dot)


def listo():
    """Tilde de dos brazos exactamente a 45°/−45°, trazo 12, unión en inglete (punta),
    extremos cortados perpendiculares al brazo."""
    line = LineString([(12, 46), (36, 70), (84, 22)])
    g = line.buffer(6, cap_style="flat", join_style="mitre", mitre_limit=4)
    return centered(g)


ICONS = {"encender": encender, "apagar": apagar, "error": error, "listo": listo}


# ---------- indicador de carga: rombo de 12 segmentos ----------
LOAD_H = 40        # semidiagonal (centro de trazo) → caja de 96 px con el trazo
LOAD_W = 8         # grosor del segmento
LOAD_N = 12        # segmentos (3 por lado) = 12 frames
LOAD_GAP = 4       # hueco entre segmentos, medido sobre el trazo


def loader_segments(cx=C, cy=C, h=LOAD_H, w=LOAD_W, n=LOAD_N, gap=LOAD_GAP):
    """Devuelve n polígonos en sentido horario empezando en el vértice superior."""
    verts = [(cx, cy - h), (cx + h, cy), (cx, cy + h), (cx - h, cy)]
    per = n // 4
    segs = []
    for side in range(4):
        (x0, y0), (x1, y1) = verts[side], verts[(side + 1) % 4]
        L = math.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / L, (y1 - y0) / L
        seg_len = (L - per * gap) / per
        for k in range(per):
            a = gap / 2 + k * (seg_len + gap)
            b = a + seg_len
            p0 = (x0 + ux * a, y0 + uy * a)
            p1 = (x0 + ux * b, y0 + uy * b)
            segs.append(LineString([p0, p1]).buffer(w / 2, cap_style="flat"))
    return segs


# brillo del segmento según distancia a la cabeza (0 = cabeza)
LOAD_TAIL = [1.00, 0.55, 0.28, 0.12]
