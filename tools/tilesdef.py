"""tilesdef.py — the single source of truth for skyfell's BG tileset ids,
collision/material attributes, and the ASCII map legend.

Imported by tools/art/mktiles.py (draws the tiles in this order) and
tools/mkmaps.py (turns ASCII metatile grids into Tiled .tmj for tmx2snes).
Attribute word (D-012): low byte = collision class, high byte = material.
"""

# collision classes (low byte)
COL_EMPTY = 0x00
COL_SOLID = 0x01

# materials (high byte) — ARCHITECTURE.md's set; portals attach to brass (D-003)
MAT_PLAIN = 0x00
MAT_BRASS = 0x01
MAT_GLASS = 0x02
MAT_MAGNET = 0x03
MAT_HAZARD = 0x04
MAT_WATER = 0x05


def attr(col, mat):
    return (mat << 8) | col


# Metatiles drawn by mktiles.py, 4 tiles each (tl,tr,bl,br), in THIS order.
# Tile 0 is the empty tile; metatile n's tl tile id = 1 + 4*n.
METAS = [
    ("hull", attr(COL_SOLID, MAT_PLAIN)),    # tiles 1-4: beveled hull block
    ("hull2", attr(COL_SOLID, MAT_PLAIN)),   # tiles 5-8: hull interior fill
    ("brass", attr(COL_SOLID, MAT_BRASS)),   # tiles 9-12: portal-attachable
    ("girder", attr(COL_SOLID, MAT_PLAIN)),  # tiles 13-16: platform beam
    ("pipe", attr(COL_EMPTY, MAT_PLAIN)),    # tiles 17-20: decor
    ("window", attr(COL_EMPTY, MAT_PLAIN)),  # tiles 21-24: decor
]
TILE_STAR1 = 25  # decor singles (drawn after the metas)
TILE_STAR2 = 26
# Portal BG tiles (Phase 2). Vertical = on a wall (opening runs down the
# wall face); horizontal = on floor/ceiling. Caps mirror via map-word flips.
# All portal tiles are collision-EMPTY: the opening is walk-through, the
# teleport plane logic lives in portal.c.
TILE_PV_CAP_B = 27   # blue vertical cap (bottom cap = vflip)
TILE_PV_BODY_B = 28
TILE_PH_CAP_B = 29   # blue horizontal cap (right cap = hflip)
TILE_PH_BODY_B = 30
TILE_PV_CAP_G = 31   # gold set
TILE_PV_BODY_G = 32
TILE_PH_CAP_G = 33
TILE_PH_BODY_G = 34
# Door metatile (D-017): a walk-through doorway VISUAL — transitions are
# trigger rects hand-tabled in main.c. Appended AFTER the portal tiles
# (their ids 27-34 are pinned in portal.c).
TILE_DOOR = 35       # 4 tiles, 35-38 (tl,tr,bl,br), collision EMPTY
TILE_COUNT = 39


def meta_quad(name):
    for i, (n, _) in enumerate(METAS):
        if n == name:
            base = 1 + 4 * i
            return (base, base + 1, base + 2, base + 3)
    raise KeyError(name)


def tile_attrs():
    """attribute u16 per tile id, 0..TILE_COUNT-1"""
    at = [0] * TILE_COUNT
    for i, (_, a) in enumerate(METAS):
        base = 1 + 4 * i
        for t in range(base, base + 4):
            at[t] = a
    return at


# ASCII map legend: char -> tile quad (tl,tr,bl,br)
LEGEND = {
    ".": (0, 0, 0, 0),
    "#": meta_quad("hull"),
    "%": meta_quad("hull2"),
    "B": meta_quad("brass"),
    "-": meta_quad("girder"),
    "|": meta_quad("pipe"),
    "o": meta_quad("window"),
    "D": (TILE_DOOR, TILE_DOOR + 1, TILE_DOOR + 2, TILE_DOOR + 3),
    "*": (TILE_STAR1, 0, 0, TILE_STAR2),
}
