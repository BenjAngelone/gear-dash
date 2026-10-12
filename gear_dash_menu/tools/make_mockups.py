"""Mockups 240×280, comparación de color de la sombra, hoja de contraste y GIFs de referencia."""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from shapely.geometry import Polygon
from shapely.ops import unary_union
import geom, render as R

ROOT = os.path.join(os.path.dirname(__file__), "..")
ICONS = os.path.join(ROOT, "icons")
OUT = os.path.join(ROOT, "design", "mockups")
os.makedirs(OUT, exist_ok=True)

GRAY_SHADOW = (58, 58, 58)      # opción A: gris #3A3A3A al 100 % en el borde
OPT = {"amarillo": (R.YELLOW, R.A_MAX_YELLOW), "gris": (GRAY_SHADOW, 1.0)}

# ---------- piezas fijas ----------
ICX, ICY = 120, 116                     # centro del ícono en MENÚ
# ---------- fondo ambiental del MENÚ (reemplaza a la placa) ----------
# Luz desde dos esquinas opuestas (arriba-izq. y abajo-der.) + marcas HUD en las 4 esquinas.
AMB_RAD = 140            # radio de la luz de esquina (px)
AMB_EXP = 1.8            # caída: (1 − r/140)^1.8
AMB = {"encender": ((255, 255, 255), 0.10), "apagar": (R.YELLOW, 0.24), "cargando": (R.YELLOW, 0.14),
       "listo": (R.YELLOW, 0.24), "error": (R.RED, 0.24),
       "marchas_on": (R.YELLOW, 0.16), "marchas_off": ((255, 255, 255), 0.10)}
_yy, _xx = np.mgrid[0:AMB_RAD, 0:AMB_RAD].astype(np.float32) + 0.5
CORNER = np.clip(1 - np.hypot(_xx, _yy) / AMB_RAD, 0, 1) ** AMB_EXP      # = icons/fondo_esquina.png


def hud_marks():
    from shapely.geometry import LineString
    from shapely.ops import unary_union
    i, arm, ch = 12, 20, 6       # margen, largo de brazo, chaflán 45°
    tl = LineString([(i, i + arm), (i, i + ch), (i + ch, i), (i + arm, i)]).buffer(1, cap_style="flat",
                                                                                 join_style="mitre")
    from shapely import affinity
    parts = [tl, affinity.scale(tl, -1, 1, origin=(120, 140)), affinity.scale(tl, 1, -1, origin=(120, 140)),
             affinity.scale(tl, -1, -1, origin=(120, 140))]
    return np.asarray(R.raster(unary_union(parts), 240, 280), dtype=np.float32) / 255


HUD = hud_marks()

# ---------- logo de fondo (marca del usuario, src_logo/) ----------
_LOGO_SRC = Image.open(os.path.join(ROOT, "src_logo", "rombo_mask_full.png"))
LOGO_BG = {  # variante: (alto px, centro x, centro y, opacidad)
    "centro": (250, 120, 128, 0.07),
    "esquina": (330, 214, 236, 0.10),
    "esquina_sup": (330, 26, 44, 0.10),
}
_logo_cache = {}


def logo_layer(variant):
    if variant not in _logo_cache:
        h, cx, cy, _ = LOGO_BG[variant]
        w = round(_LOGO_SRC.width * h / _LOGO_SRC.height)
        m = np.asarray(_LOGO_SRC.resize((w, h), Image.LANCZOS), dtype=np.float32) / 255
        full = np.zeros((280, 240), np.float32)
        x0, y0 = round(cx - w / 2), round(cy - h / 2)
        xs, ys = max(0, x0), max(0, y0)
        xe, ye = min(240, x0 + w), min(280, y0 + h)
        full[ys:ye, xs:xe] = m[ys - y0:ye - y0, xs - x0:xe - x0]
        _logo_cache[variant] = full
    return _logo_cache[variant]


LOGO_VARIANT = "esquina"


def ambient(scr, state, k=1.0, logo=None):
    c, a = AMB[state]
    logo = LOGO_VARIANT if logo is None else logo
    if logo:
        op = LOGO_BG[logo][3]
        white = tuple(c) == (255, 255, 255)
        k_logo = 1.0 if white else (1.3 if not state.startswith("marchas") else 1.0)
        scr += logo_layer(logo)[..., None] * np.array(c, np.float32) * op * k_logo
    add = np.zeros((280, 240), np.float32)
    add[:AMB_RAD, :AMB_RAD] += CORNER
    add[280 - AMB_RAD:, 240 - AMB_RAD:] += CORNER[::-1, ::-1]
    scr += add[..., None] * np.array(c, np.float32) * a * k        # aditivo: es luz
    R.paint(scr, HUD, R.BORDER)


icon = {n: np.asarray(Image.open(os.path.join(ICONS, f"{n}.png")), dtype=np.float32) / 255 for n in geom.ICONS}
glow = {n: Image.open(os.path.join(ICONS, f"{n}_glow.png")) for n in geom.ICONS}
load_frames = np.asarray(Image.open(os.path.join(ICONS, "carga_frames.png")), dtype=np.float32) / 255
load_track = np.asarray(Image.open(os.path.join(ICONS, "carga_pista.png")), dtype=np.float32) / 255

# número de marcha (Barlow Semi Condensed Bold como sustituto de Bahnschrift Bold SemiCondensed)
NUM_SIZE, NUM_CY = 216, 158
_num_cache = {}


def number_assets(ch):
    if ch not in _num_cache:
        hi = R.text_mask_hi(ch, R.FONT_BOLD, NUM_SIZE, 240, 280, 120, NUM_CY, ss=4)
        mask = np.asarray(hi.resize((240, 280), Image.BOX), dtype=np.float32) / 255
        smap = R.shadow_map_from_hi(hi, 4)
        _num_cache[ch] = (mask, smap)
    return _num_cache[ch]


_lsrc = Image.open(os.path.join(ROOT, "src_logo", "rombo_mask_full.png"))
LOGO_H = 38
_lw = round(_lsrc.width * LOGO_H / _lsrc.height)
LOGO_SMALL = _lsrc.resize((_lw, LOGO_H), Image.LANCZOS)          # = icons/logo_marchas.png
LOGO_MASK = np.zeros((280, 240), np.float32)
LOGO_X, LOGO_Y = 120 - _lw // 2, 16
LOGO_MASK[LOGO_Y:LOGO_Y + LOGO_H, LOGO_X:LOGO_X + _lw] = np.asarray(LOGO_SMALL, dtype=np.float32) / 255


def label(scr, txt, color, cy=214, size=26):
    m = np.asarray(R.text_mask(txt, R.FONT_SEMI, size, cy=cy), dtype=np.float32) / 255
    R.paint(scr, m, color)


# ---------- pantallas ----------
def menu(state, press=None, opt="amarillo", dots=None, frame=0, amb=1.0, logo=None):
    """state: encender | apagar | cargando | listo | error.
    press: None o (I, R) de la sombra."""
    scr = R.new_screen()
    ambient(scr, state, amb, logo)
    if state == "cargando":
        R.paint(scr, load_track, R.TRACK, ICX - 48, ICY - 48)
        R.paint(scr, load_frames[:, frame * 96:(frame + 1) * 96], R.YELLOW, ICX - 48, ICY - 48)
        if dots is not None:  # 5 comandos
            for i in range(5):
                x = 120 + (i - 2) * 20
                d = Polygon([(x, 170), (x + 6, 176), (x, 182), (x - 6, 176)])
                col = R.YELLOW if i < dots else (R.TRACK if i > dots else (128, 102, 0))
                R.paint(scr, np.asarray(R.raster(d, 240, 280), dtype=np.float32) / 255, col)
        label(scr, "ARRANCANDO", R.GRAY)
        return scr
    name = {"encender": "encender", "apagar": "apagar", "listo": "listo", "error": "error"}[state]
    col = {"encender": R.WHITE, "apagar": R.YELLOW, "listo": R.YELLOW, "error": R.RED}[state]
    if press is not None:
        c, amax = OPT[opt]
        if state == "error":
            c = R.RED
        a = R.halo_alpha(glow[name], *press, amax)
        R.paint(scr, a, c, ICX - 84, ICY - 84)
    R.paint(scr, icon[name], col, ICX - 48, ICY - 48)
    txt = {"encender": "ENCENDER", "apagar": "APAGAR", "listo": "LISTO", "error": "SIN RESPUESTA"}[state]
    label(scr, txt, R.WHITE if state in ("encender", "apagar") else col)
    return scr


def marchas(ch="3", press=None, opt="amarillo", car_on=True):
    scr = R.new_screen()
    ambient(scr, "marchas_on" if car_on else "marchas_off")
    mask, smap = number_assets(ch)
    if press is not None:
        c, amax = OPT[opt]
        R.paint(scr, R.halo_alpha(smap, *press, amax), c)
    R.paint(scr, LOGO_MASK, R.WHITE)
    R.paint(scr, mask, R.WHITE)
    return scr


def p_at(t_ms, T):
    return R.press_params(t_ms / T)


FLASH = (1.8, 16.0)   # pico del destello de confirmación

if __name__ == "__main__":
    Image.fromarray((CORNER * 255).astype(np.uint8)).save(os.path.join(ICONS, "fondo_esquina.png"), optimize=True)
    LOGO_SMALL.save(os.path.join(ICONS, "logo_marchas.png"), optimize=True)
    print("logo chico", LOGO_SMALL.size, (LOGO_X, LOGO_Y))
    shots = {
        "menu_reposo_encender": menu("encender"),
        "menu_reposo_apagar": menu("apagar"),
        "menu_pulsando_000": menu("encender", p_at(60, 1500)),
        "menu_pulsando_050": menu("encender", p_at(750, 1500)),
        "menu_pulsando_100": menu("encender", FLASH),
        "menu_cargando": menu("cargando", dots=2, frame=4),
        "menu_listo": menu("listo"),
        "menu_error": menu("error"),
        "marchas_reposo": marchas(),
        "marchas_pulsando_000": marchas(press=p_at(60, 1000)),
        "marchas_pulsando_050": marchas(press=p_at(500, 1000)),
        "marchas_pulsando_100": marchas(press=FLASH),
    }
    for k, s in shots.items():
        R.to_img(s).save(os.path.join(OUT, f"{k}.png"), optimize=True)

    # ---- hoja de mockups ----
    f = ImageFont.truetype(R.FONT_SEMI, 15)
    order = [("menu_reposo_encender", "Reposo · ENCENDER"), ("menu_reposo_apagar", "Reposo · APAGAR"),
             ("menu_pulsando_000", "Pulsando 0 % (60 ms)"), ("menu_pulsando_050", "Pulsando 50 % (750 ms)"),
             ("menu_pulsando_100", "100 % · destello"), ("menu_cargando", "Cargando (cmd 3/5)"),
             ("menu_listo", "Listo"), ("menu_error", "Error"),
             ("marchas_reposo", "MARCHAS · reposo"), ("marchas_pulsando_000", "MARCHAS · 0 % (60 ms)"),
             ("marchas_pulsando_050", "MARCHAS · 50 % (500 ms)"), ("marchas_pulsando_100", "MARCHAS · 100 % · destello")]
    cols, gw, gh = 4, 264, 320
    sheet = Image.new("RGB", (cols * gw + 24, 3 * gh + 24), (14, 14, 14))
    d = ImageDraw.Draw(sheet)
    for i, (k, cap) in enumerate(order):
        x, y = 24 + (i % cols) * gw, 24 + (i // cols) * gh
        sheet.paste(R.to_img(shots[k]), (x, y))
        d.rectangle([x - 1, y - 1, x + 240, y + 280], outline=(48, 48, 48))
        d.text((x, y + 288), cap, font=f, fill=(170, 170, 170))
    sheet.save(os.path.join(OUT, "..", "mockups_hoja.png"), optimize=True)

    # ---- comparación gris vs amarillo ----
    rows = [("50 %", lambda o: marchas(press=p_at(500, 1000), opt=o), lambda o: menu("encender", p_at(750, 1500), o)),
            ("100 %", lambda o: marchas(press=(1.0, 12.0), opt=o), lambda o: menu("encender", (1.0, 12.0), o)),
            ("destello", lambda o: marchas(press=FLASH, opt=o), lambda o: menu("encender", FLASH, o))]
    gw2 = 252
    cmp = Image.new("RGB", (24 + 4 * gw2, 60 + 3 * 316 + 200), (14, 14, 14))
    d = ImageDraw.Draw(cmp)
    fb = ImageFont.truetype(R.FONT_SEMI, 18)
    heads = ["A · gris #3A3A3A", "B · amarillo 30 %", "A · gris #3A3A3A", "B · amarillo 30 %"]
    for j, h in enumerate(heads):
        d.text((24 + j * gw2, 20), h, font=fb, fill=(200, 200, 200))
    for i, (cap, fm, fn) in enumerate(rows):
        y = 56 + i * 316
        for j, (fun, o) in enumerate([(fm, "gris"), (fm, "amarillo"), (fn, "gris"), (fn, "amarillo")]):
            img = R.to_img(fun(o))
            cmp.paste(img, (24 + j * gw2, y))
        d.text((24, y + 284), cap, font=f, fill=(150, 150, 150))
    # fila "de reojo": 100 % a escala angular de 40 cm vista en monitor + desenfoque periférico
    y = 56 + 3 * 316
    d.text((24, y), "De reojo (100 %, escala angular de 40 cm, desenfoque periférico)", font=f, fill=(150, 150, 150))
    for j, (fun, o) in enumerate([(rows[1][1], "gris"), (rows[1][1], "amarillo"), (rows[1][2], "gris"), (rows[1][2], "amarillo")]):
        img = R.to_img(fun(o)).resize((158, 184), Image.LANCZOS).filter(ImageFilter.GaussianBlur(1.6))
        cmp.paste(img, (24 + j * gw2, y + 24))
    cmp.save(os.path.join(OUT, "..", "sombra_comparacion.png"), optimize=True)

    # ---- contraste a 40 cm ----
    # ST7789 1,69" 240×280 ≈ 218 ppi → 96 px = 11,2 mm → a 40 cm = 1,6°.
    # En un monitor de ~96 ppi mirado a 60 cm el mismo ángulo son ~63 px.
    names = [("encender", R.WHITE), ("apagar", R.YELLOW), ("error", R.RED), ("listo", R.YELLOW)]
    con = Image.new("RGB", (24 + 4 * 150, 24 + 3 * 130 + 40), (0, 0, 0))
    d = ImageDraw.Draw(con)
    for j, (n, c) in enumerate(names):
        a = icon[n]
        rgb = np.zeros((96, 96, 3), np.float32) + np.array(c, np.float32) * a[..., None]
        im = Image.fromarray(rgb.astype(np.uint8))
        x = 24 + j * 150
        con.paste(im, (x, 24))
        con.paste(im.resize((63, 63), Image.LANCZOS), (x + 16, 24 + 130))
        con.paste(im.resize((63, 63), Image.LANCZOS).filter(ImageFilter.GaussianBlur(1.6)), (x + 16, 24 + 260))
    for i, cap in enumerate(["1:1 px del panel", "40 cm (63 px en monitor a 60 cm)", "de reojo (desenfoque 1,6 px)"]):
        d.text((24, 24 + i * 130 + 100 if i == 0 else 24 + i * 130 + 70), cap, font=f, fill=(140, 140, 140))
    con.save(os.path.join(OUT, "..", "contraste_40cm.png"), optimize=True)

    # ---- GIFs de referencia ----
    def gif(frames, name, ms):
        ims = [R.to_img(s).convert("P", palette=Image.ADAPTIVE, colors=64) for s in frames]
        ims[0].save(os.path.join(OUT, "..", name), save_all=True, append_images=ims[1:], duration=ms, loop=0,
                    optimize=True)

    def seq(screen_fn, T):
        fr = [screen_fn(None)] * 6
        for t in range(0, T + 1, 33):
            fr.append(screen_fn(p_at(t, T)) if t >= 60 else screen_fn(None))
        # destello: 60 ms subida, 180 ms bajada
        for u in [0.5, 1.0]:
            fr.append(screen_fn((1 + 0.8 * u, 12 + 4 * u)))
        for u in [0.18, 0.36, 0.55, 0.73, 0.9, 1.0]:
            k = (1 - u) ** 2
            fr.append(screen_fn((1.8 * k, 16.0)))
        fr += [screen_fn(None)] * 10
        return fr

    gif(seq(lambda p: marchas(press=p), 1000), "pulsacion_marchas.gif", 33)
    gif(seq(lambda p: menu("encender", p), 1500), "pulsacion_menu.gif", 33)
    # cancelado: soltó a 600 ms → 150 ms de desvanecimiento
    fr = [menu("encender")] * 4 + [menu("encender", p_at(t, 1500)) if t >= 60 else menu("encender")
                                    for t in range(0, 601, 33)]
    I0, R0 = p_at(600, 1500)
    fr += [menu("encender", (I0 * (1 - u) ** 2, R0)) for u in (0.22, 0.44, 0.66, 0.88)] + [menu("encender")] * 12
    gif(fr, "pulsacion_cancelada.gif", 33)
    import math
    # carga: cometa 80 ms/segmento + luz de esquinas que respira (0,6 ↔ 1,0, período 1920 ms)
    gif([menu("cargando", dots=2, frame=k % 12, amb=0.8 + 0.2 * math.cos(2 * math.pi * k / 24)) for k in range(24)], "carga.gif", 80)
    print("ok")

    # ---- listo y error ----
    # LISTO: el tilde aparece entero; destello del halo amarillo 2,0 → 0 en 400 ms (ease-out), R = 16
    fr = [menu("cargando", dots=5, frame=k % 12) for k in range(6)]
    fr += [menu("listo", (2.0 * (1 - u) ** 2, 16.0)) for u in [i / 12 for i in range(13)]]
    fr += [menu("listo")] * 30
    gif(fr, "listo.gif", 33)
    # ERROR: doble destello del halo rojo (120 ms prendido / 120 ms apagado ×2), después fijo
    fr = [menu("cargando", dots=1, frame=k % 12) for k in range(6)]
    for _ in range(2):
        fr += [menu("error", (1.6, 12.0))] * 4 + [menu("error")] * 4
    fr += [menu("error")] * 30
    gif(fr, "error.gif", 33)
    print("gifs ok")

    # ---- variantes del logo de fondo ----
    vs = [("centro", "Centro · 7 %"), ("esquina", "Esquina abajo-der. · 10 %"), ("esquina_sup", "Esquina arriba-izq. · 10 %"),
          ("", "Sin logo")]
    sts = ["encender", "apagar", "error"]
    lg = Image.new("RGB", (24 + 4 * 252, 40 + 3 * 300), (14, 14, 14))
    d = ImageDraw.Draw(lg)
    for j, (v, cap) in enumerate(vs):
        d.text((24 + j * 252, 12), cap, font=fb, fill=(200, 200, 200))
        for i, st in enumerate(sts):
            lg.paste(R.to_img(menu(st, logo=v)), (24 + j * 252, 40 + i * 300))
    lg.save(os.path.join(OUT, "..", "logo_fondo_variantes.png"), optimize=True)
