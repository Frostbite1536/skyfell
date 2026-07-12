"""mkobj.py — Wren, the apprentice riftwright: deterministic 16x32 player
sprite frames -> SNES OBJ 4bpp name-table sheet + palette.

Sheet layout (name table, 16 tiles/row): 8 frames side by side. Frame f's
TOP 16x16 half = tiles f*2, f*2+1, f*2+16, f*2+17 (rows 0-1); BOTTOM half =
32+f*2 ... (rows 2-3). player.c renders two stacked 16x16 small sprites with
base tiles f*2 (top) and 32+f*2 (bottom).

Frames: 0 idle, 1 idle-bob, 2..5 run cycle, 6 jump, 7 fall.
Entity band (rows 4-5): entity i's 16x16 base name = 64 + i*2.
Drone band (rows 6-7): the Gale Drone's two spin frames at names 96/98 —
RADIALLY SYMMETRIC (INV-HW-004/D-005: OBJ can't rotate; the chamber screen
does, so a round drone reads right at every angle).
Outputs: src/data/generated/obj.chr (128 tiles, 4KB), obj.pal (32B), obj.png.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from art.mktiles import bgr555, tile_to_4bpp, write_png  # noqa: E402

GEN = os.path.join(os.path.dirname(__file__), "..", "..", "src", "data", "generated")

PAL = [
    0x000000,  # 0 transparent
    0x101018,  # 1 outline
    0xE8B890,  # 2 skin
    0xB8845C,  # 3 skin shadow
    0x2E8C8C,  # 4 jacket teal
    0x4EC4B8,  # 5 jacket light
    0x1E5E62,  # 6 jacket dark
    0x2A3040,  # 7 pants
    0x8A6A28,  # 8 boot brass
    0xC89A38,  # 9 boot light
    0x78E0FF,  # 10 goggle glass
    0x7A3A22,  # 11 hair auburn
    0x503620,  # 12 strap/belt
    0xF0F0F0,  # 13 white glint
    0x94ACC4,  # 14 buckle steel
    0xC84040,  # 15 enemy red
]

W, H = 16, 32


class C:
    def __init__(self):
        self.p = [[0] * W for _ in range(H)]

    def px(self, x, y, c):
        if 0 <= x < W and 0 <= y < H:
            self.p[y][x] = c

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.px(x, y, c)


def head_torso(c, bob, arms):
    """arms: 'down' | 'up'"""
    b = bob
    # hair
    c.rect(4, 1 + b, 11, 3 + b, 11)
    c.px(3, 2 + b, 11)
    c.px(12, 3 + b, 11)
    # goggles strap + lenses (up on the forehead)
    c.rect(3, 4 + b, 12, 4 + b, 12)
    c.rect(4, 4 + b, 5, 5 + b, 10)
    c.rect(9, 4 + b, 10, 5 + b, 10)
    c.px(4, 4 + b, 13)
    c.px(9, 4 + b, 13)
    # face
    c.rect(4, 5 + b, 11, 8 + b, 2)
    c.rect(4, 5 + b, 3, 5 + b, 2)
    c.px(6, 6 + b, 1)   # eyes
    c.px(10, 6 + b, 1)
    c.rect(4, 8 + b, 11, 8 + b, 3)  # jaw shade
    # outline head sides
    c.px(3, 5 + b, 1)
    c.px(12, 5 + b, 1)
    # torso: teal jacket
    c.rect(4, 9 + b, 11, 17, 4)
    c.rect(4, 9 + b, 11, 10 + b, 5)      # collar
    c.rect(8, 10 + b, 8, 16, 6)          # zip line
    c.rect(4, 16, 11, 16, 6)             # hem shade
    c.rect(4, 17, 11, 17, 12)            # belt
    c.px(8, 17, 14)                      # buckle
    if arms == "down":
        c.rect(2, 10 + b, 3, 15, 4)
        c.rect(12, 10 + b, 13, 15, 4)
        c.rect(2, 16, 3, 16, 2)          # hands
        c.rect(12, 16, 13, 16, 2)
        c.px(2, 10 + b, 6)
        c.px(13, 10 + b, 6)
    else:  # up
        c.rect(1, 6 + b, 2, 11, 4)
        c.rect(13, 6 + b, 14, 11, 4)
        c.rect(1, 5 + b, 2, 5 + b, 2)    # hands above
        c.rect(13, 5 + b, 14, 5 + b, 2)


def leg(c, x, ytop, ybot, boot_dx=0):
    c.rect(x, ytop, x + 2, ybot, 7)
    c.rect(x + boot_dx, ybot + 1, x + 2 + boot_dx, ybot + 3, 8)
    c.rect(x + boot_dx, ybot + 1, x + 2 + boot_dx, ybot + 1, 9)


def frame(kind):
    c = C()
    if kind == "idle":
        head_torso(c, 0, "down")
        leg(c, 4, 18, 27)
        leg(c, 9, 18, 27)
    elif kind == "idle2":
        head_torso(c, 1, "down")
        leg(c, 4, 18, 27)
        leg(c, 9, 18, 27)
    elif kind == "run1":  # left forward, right back
        head_torso(c, 0, "down")
        leg(c, 3, 18, 26, -1)
        leg(c, 10, 18, 24, 2)
    elif kind == "run2":  # passing
        head_torso(c, 1, "down")
        leg(c, 5, 18, 27)
        leg(c, 9, 18, 25, 1)
    elif kind == "run3":  # right forward, left back
        head_torso(c, 0, "down")
        leg(c, 10, 18, 26, 1)
        leg(c, 3, 18, 24, -2)
    elif kind == "run4":  # passing
        head_torso(c, 1, "down")
        leg(c, 8, 18, 27)
        leg(c, 5, 18, 25, -1)
    elif kind == "jump":  # tucked, arms up
        head_torso(c, 0, "up")
        leg(c, 4, 18, 23)
        leg(c, 9, 18, 22, 1)
    elif kind == "fall":  # spread, arms up
        head_torso(c, 1, "up")
        leg(c, 3, 18, 26, -1)
        leg(c, 10, 18, 25, 1)
    return c


FRAMES = ["idle", "idle2", "run1", "run2", "run3", "run4", "jump", "fall"]


# --- Phase 2 entity sprites: one 16x16 each, name-table rows 4-5.
# Entity i's base tile name = 64 + i*2 (entity.c's ENT_ART table order).
class E16:
    def __init__(self):
        self.p = [[0] * 16 for _ in range(16)]

    def px(self, x, y, c):
        if 0 <= x < 16 and 0 <= y < 16:
            self.p[y][x] = c

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.px(x, y, c)

    def orb(self, rim, core):
        for y in range(16):
            for x in range(16):
                d = (x - 7) * (x - 7) + (y - 7) * (y - 7)
                if d <= 9:
                    self.p[y][x] = core
                elif d <= 20:
                    self.p[y][x] = rim


def e_crate():
    c = E16()
    c.rect(1, 1, 14, 14, 12)          # wood
    c.rect(2, 2, 13, 13, 8)
    c.rect(2, 7, 13, 8, 9)            # mid band
    c.rect(1, 1, 14, 1, 9)
    for x in (1, 14):
        c.rect(x, 1, x, 14, 12)
    c.rect(1, 14, 14, 14, 1)
    c.px(2, 2, 13)
    return c


def e_sentry(dead=False):
    c = E16()
    c.rect(2, 13, 13, 15, 7)          # base
    c.rect(3, 8, 12, 12, 14)          # housing
    c.rect(4, 5, 11, 7, 4)            # dome
    c.rect(5, 4, 10, 4, 5)
    if dead:
        c.px(6, 8, 1)                 # cracked, eye out
        c.px(9, 10, 1)
        c.px(7, 6, 1)
    else:
        c.rect(6, 8, 9, 10, 1)        # eye socket
        c.rect(7, 9, 8, 9, 15)        # the red eye
    c.rect(2, 15, 13, 15, 1)
    return c


def e_shot(gold):
    c = E16()
    c.orb(9 if gold else 10, 10 if gold else 13)
    return c


def e_sshot():
    c = E16()
    c.orb(15, 13)
    return c


def e_reticle():
    c = E16()
    for dx, dy in ((0, 0), (1, 0), (0, 1)):
        for cx, cy in ((3, 3), (12, 3), (3, 12), (12, 12)):
            c.px(cx + (dx if cx < 8 else -dx), cy + (dy if cy < 8 else -dy), 13)
    c.px(7, 7, 13)
    c.px(8, 8, 13)
    return c


def e_drone(alt):
    """The Gale Drone: brass ring fan, glass core, red eye. C4-symmetric
    around (7,7) so the rotated chamber can never show it 'wrong' — the two
    frames alternate vane positions (cardinal vs diagonal) to read as spin."""
    c = E16()
    for y in range(16):
        for x in range(16):
            d = (x - 7) * (x - 7) + (y - 7) * (y - 7)
            if 33 <= d <= 52:
                c.px(x, y, 8)      # brass ring
            elif d <= 6:
                c.px(x, y, 10)     # glass core
            elif d <= 12:
                c.px(x, y, 6)      # dark seat
    if alt:
        vanes = ((4, 4, 5, 5), (10, 4, 9, 5), (10, 10, 9, 9), (4, 10, 5, 9))
    else:
        vanes = ((7, 2, 7, 3), (12, 7, 11, 7), (7, 12, 7, 11), (2, 7, 3, 7))
    for (x0, y0, x1, y1) in vanes:
        c.px(x0, y0, 9)            # vane tip (bright brass)
        c.px(x1, y1, 9)
    c.px(7, 7, 15)                 # the eye
    return c


ENTS = [e_crate(), e_sentry(False), e_sentry(True),
        e_shot(False), e_shot(True), e_sshot(), e_reticle()]
DRONES = [e_drone(False), e_drone(True)]


def main():
    os.makedirs(GEN, exist_ok=True)
    # name table: 16 tiles x 8 rows. Player frame f: top half at col f*2
    # rows 0-1, bottom half rows 2-3. Entity i: 16x16 at col i*2 rows 4-5
    # (base name 64 + i*2). Drone frame j: col j*2 rows 6-7 (name 96 + j*2).
    sheet = [[0] * 128 for _ in range(64)]
    for f, kind in enumerate(FRAMES):
        c = frame(kind)
        for y in range(16):
            for x in range(16):
                sheet[y][f * 16 + x] = c.p[y][x]          # top half rows 0-15
                sheet[16 + y][f * 16 + x] = c.p[16 + y][x]  # bottom rows 16-31
    for i, e in enumerate(ENTS):
        for y in range(16):
            for x in range(16):
                sheet[32 + y][i * 16 + x] = e.p[y][x]
    for j, e in enumerate(DRONES):
        for y in range(16):
            for x in range(16):
                sheet[48 + y][j * 16 + x] = e.p[y][x]
    # cut into 8x8 tiles in NAME-TABLE order (16 tiles/row, 8 rows)
    tiles = []
    for trow in range(8):
        for tcol in range(16):
            t = [[sheet[trow * 8 + y][tcol * 8 + x] for x in range(8)] for y in range(8)]
            tiles.append(t)
    assert len(tiles) == 128

    with open(os.path.join(GEN, "obj.chr"), "wb") as f:
        for t in tiles:
            f.write(tile_to_4bpp(t))
    with open(os.path.join(GEN, "obj.pal"), "wb") as f:
        for rgb in PAL:
            f.write(struct.pack("<H", bgr555(rgb)))

    # preview PNG (magenta backdrop so transparency reads)
    rows = []
    for y in range(64):
        row = bytearray()
        for x in range(128):
            v = PAL[sheet[y][x]] if sheet[y][x] else 0x552255
            row += bytes(((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF))
        rows.append(bytes(row))
    write_png(os.path.join(GEN, "obj.png"), 128, 64, rows)
    print("mkobj: 8 frames + 7 entities + 2 drone frames -> obj.chr/pal/png")


if __name__ == "__main__":
    main()
