#!/usr/bin/env python3
"""Génère les assets du personnage de test (PNG RGBA), sans dépendance externe.

Produit :
  - examples/01_perso2d/assets/hero_sheet.png : spritesheet animée
        ligne 0 = idle (2 frames), ligne 1 = walk (4 frames), frames de 48x64
  - examples/01_perso2d/assets/hero.png       : une seule frame (repli statique)

Remplace-les par de vrais assets quand tu veux (voir README / sources d'assets).
"""
import os
import struct
import zlib

FW, FH = 48, 64  # taille d'une frame

OUTLINE = (30, 30, 40, 255)
SKIN = (255, 220, 180, 255)
BODY = (230, 60, 60, 255)
BODY_DARK = (180, 40, 40, 255)
FEET = (60, 50, 60, 255)


def make_buf(w, h):
    return [[(0, 0, 0, 0) for _ in range(w)] for _ in range(h)]


def put(buf, x, y, rgba):
    if 0 <= y < len(buf) and 0 <= x < len(buf[0]):
        buf[y][x] = rgba


def disc(buf, cx, cy, r, rgba):
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                put(buf, x, y, rgba)


def rect(buf, x0, y0, x1, y1, rgba):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            put(buf, x, y, rgba)


def draw_char(buf, ox, oy, leg_phase, bob):
    """Dessine le personnage dans la cellule de coin (ox, oy).
    leg_phase : -1 / 0 / +1 (un pied en avant). bob : lift vertical du buste."""
    cx = ox + 24
    vb = bob
    # Corps (contour, couleur, ombrage bas)
    rect(buf, ox + 11, oy + 27 - vb, ox + 37, oy + 57 - vb, OUTLINE)
    rect(buf, ox + 13, oy + 29 - vb, ox + 35, oy + 55 - vb, BODY)
    rect(buf, ox + 13, oy + 45 - vb, ox + 35, oy + 55 - vb, BODY_DARK)
    # Pieds (animés horizontalement)
    rect(buf, ox + 15 + leg_phase * 3, oy + 56, ox + 22 + leg_phase * 3, oy + 60, FEET)
    rect(buf, ox + 26 - leg_phase * 3, oy + 56, ox + 33 - leg_phase * 3, oy + 60, FEET)
    # Tête
    disc(buf, cx, oy + 17 - vb, 13, OUTLINE)
    disc(buf, cx, oy + 17 - vb, 11, SKIN)
    disc(buf, ox + 20, oy + 16 - vb, 2, OUTLINE)
    disc(buf, ox + 28, oy + 16 - vb, 2, OUTLINE)
    rect(buf, ox + 21, oy + 22 - vb, ox + 27, oy + 22 - vb, OUTLINE)


def write_png(buf, path):
    h = len(buf)
    w = len(buf[0])
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filtre 0 par scanline
        for x in range(w):
            raw.extend(buf[y][x])

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        c += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        return c

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)
    print("écrit :", path, f"({w}x{h})")


# --- Spritesheet : 4 colonnes x 2 lignes -------------------------------------
COLS, ROWS = 4, 2
sheet = make_buf(COLS * FW, ROWS * FH)

# Ligne 0 : idle (léger souffle). (leg_phase, bob)
idle = [(0, 0), (0, 1)]
for i, (lp, bob) in enumerate(idle):
    draw_char(sheet, i * FW, 0 * FH, lp, bob)

# Ligne 1 : walk (cycle de 4 frames, pieds qui alternent).
walk = [(0, 1), (1, 0), (0, 1), (-1, 0)]
for i, (lp, bob) in enumerate(walk):
    draw_char(sheet, i * FW, 1 * FH, lp, bob)

here = os.path.dirname(os.path.abspath(__file__))
assets = os.path.normpath(os.path.join(here, "..", "examples", "01_perso2d", "assets"))
write_png(sheet, os.path.join(assets, "hero_sheet.png"))

# --- Frame unique (repli statique) -------------------------------------------
single = make_buf(FW, FH)
draw_char(single, 0, 0, 0, 0)
write_png(single, os.path.join(assets, "hero.png"))


# --- Sprites pour l'éditeur (vrais visuels au lieu de rectangles) ------------
def gen_player():
    b = make_buf(40, 56)
    OUT, SKIN = (30, 30, 40, 255), (255, 220, 180, 255)
    BODY, DARK = (70, 130, 220, 255), (45, 95, 170, 255)
    rect(b, 7, 22, 33, 52, OUT); rect(b, 9, 24, 31, 50, BODY); rect(b, 9, 40, 31, 50, DARK)
    rect(b, 11, 51, 18, 55, (40, 40, 50, 255)); rect(b, 22, 51, 29, 55, (40, 40, 50, 255))
    disc(b, 20, 14, 12, OUT); disc(b, 20, 14, 10, SKIN)
    disc(b, 16, 13, 2, OUT); disc(b, 24, 13, 2, OUT)
    write_png(b, os.path.join(assets, "player.png"))


def gen_enemy():
    b = make_buf(40, 40)
    OUT, BODY, DARK = (40, 10, 10, 255), (220, 70, 70, 255), (170, 40, 40, 255)
    disc(b, 20, 22, 17, OUT); disc(b, 20, 22, 15, BODY); disc(b, 20, 27, 9, DARK)
    disc(b, 14, 18, 5, (255, 255, 255, 255)); disc(b, 14, 19, 2, OUT)
    disc(b, 26, 18, 5, (255, 255, 255, 255)); disc(b, 26, 19, 2, OUT)
    rect(b, 9, 11, 17, 13, OUT); rect(b, 23, 11, 31, 13, OUT)  # sourcils fâchés
    write_png(b, os.path.join(assets, "enemy.png"))


def gen_coin():
    b = make_buf(28, 28)
    OUT, GOLD, LIGHT = (150, 110, 0, 255), (253, 203, 0, 255), (255, 235, 130, 255)
    disc(b, 14, 14, 13, OUT); disc(b, 14, 14, 11, GOLD); disc(b, 14, 14, 6, LIGHT)
    rect(b, 9, 6, 11, 12, (255, 255, 255, 210))  # reflet
    write_png(b, os.path.join(assets, "coin.png"))


def gen_flag():
    b = make_buf(40, 90)
    OUT, POLE, FLAG = (40, 40, 50, 255), (190, 190, 200, 255), (120, 230, 120, 255)
    rect(b, 6, 4, 11, 88, OUT); rect(b, 7, 6, 10, 86, POLE)   # mât
    rect(b, 11, 8, 37, 32, OUT); rect(b, 12, 10, 35, 30, FLAG)  # drapeau
    rect(b, 2, 84, 30, 89, (110, 80, 50, 255))                  # socle
    write_png(b, os.path.join(assets, "flag.png"))


def gen_crate():
    b = make_buf(42, 42)
    OUT, WOOD, DARK = (60, 40, 20, 255), (190, 140, 80, 255), (150, 105, 60, 255)
    rect(b, 1, 1, 40, 40, OUT); rect(b, 3, 3, 38, 38, WOOD)
    rect(b, 3, 19, 38, 22, DARK); rect(b, 19, 3, 22, 38, DARK)  # planches en croix
    write_png(b, os.path.join(assets, "crate.png"))


def gen_tile():
    b = make_buf(64, 64)
    OUT, DIRT = (60, 40, 20, 255), (155, 105, 65, 255)
    GRASS, GDARK = (85, 185, 95, 255), (60, 150, 75, 255)
    rect(b, 0, 0, 63, 63, OUT)
    rect(b, 1, 1, 62, 62, DIRT)
    rect(b, 1, 1, 62, 13, GRASS)     # herbe sur le dessus
    rect(b, 1, 12, 62, 16, GDARK)
    write_png(b, os.path.join(assets, "tile.png"))


gen_player(); gen_enemy(); gen_coin(); gen_flag(); gen_crate(); gen_tile()
