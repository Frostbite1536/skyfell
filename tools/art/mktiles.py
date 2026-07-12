"""mktiles.py — deterministic BG tileset generator (prophet's generated-art
pattern: SNES-native output from pure stdlib Python, no image tools in the
build loop). Emits into src/data/generated/:
  tiles.chr  4bpp planar tiles (tilesdef.TILE_COUNT * 32 bytes)
  tiles.pal  16 BGR555 colors (32 bytes)
  tiles.map  identity tile remap for tmx2snes (u16 per tile)
  tiles.png  the tileset sheet, 16 tiles/row — for humans and future Tiled use
Everything is fixed-function drawing: same input -> byte-identical output.
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from tilesdef import TILE_COUNT  # noqa: E402

GEN = os.path.join(os.path.dirname(__file__), "..", "..", "src", "data", "generated")

# palette (index -> #RRGGBB); 0 is the transparent slot
PAL = [
    0x0A0E1E,  # 0 transparent slot = CGRAM 0 = the BACKDROP: deep navy sky
    0x111830,  # 1 navy accent
    0x16203C,  # 2 navy glass
    0x2A3A55,  # 3 slate shadow
    0x46587A,  # 4 slate mid
    0x66799C,  # 5 slate light
    0x8FA2BF,  # 6 slate highlight
    0x3A2E1E,  # 7 brass shadow
    0x7A5A22,  # 8 brass mid
    0xB08A30,  # 9 brass light
    0xE0C060,  # 10 brass highlight
    0x1E2A26,  # 11 pipe dark
    0x35514A,  # 12 pipe green
    0x0D111C,  # 13 outline
    0xC7D4E8,  # 14 bright edge
    0x804060,  # 15 spare
]


def bgr555(rgb):
    r, g, b = (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)


class Canvas16:
    """one 16x16 metatile painting surface"""

    def __init__(self, fill=0):
        self.p = [[fill] * 16 for _ in range(16)]

    def px(self, x, y, c):
        if 0 <= x < 16 and 0 <= y < 16:
            self.p[y][x] = c

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.px(x, y, c)

    def hline(self, x0, x1, y, c):
        self.rect(x0, y, x1, y, c)

    def vline(self, x, y0, y1, c):
        self.rect(x, y0, x, y1, c)

    def quads(self):
        """split into 4 8x8 tiles: tl, tr, bl, br"""
        out = []
        for oy in (0, 8):
            for ox in (0, 8):
                out.append([[self.p[oy + y][ox + x] for x in range(8)] for y in range(8)])
        return out


def t_empty():
    return [[0] * 8 for _ in range(8)]


def mt_hull():
    c = Canvas16(4)
    c.rect(0, 0, 15, 0, 6)
    c.rect(0, 1, 15, 1, 5)
    c.vline(0, 0, 15, 6)
    c.vline(1, 1, 14, 5)
    c.hline(0, 15, 15, 13)
    c.hline(1, 15, 14, 3)
    c.vline(15, 0, 15, 13)
    c.vline(14, 1, 14, 3)
    for rx, ry in ((3, 3), (12, 3), (3, 12), (12, 12)):
        c.px(rx, ry, 3)
        c.px(rx + 1, ry, 3)
        c.px(rx, ry + 1, 3)
    return c


def mt_hull2():
    c = Canvas16(3)
    for y in range(16):
        for x in range(16):
            if (x + y) % 8 == 0:
                c.px(x, y, 13)
    c.hline(0, 15, 7, 13)
    c.hline(0, 15, 8, 4)
    return c


def mt_brass():
    c = Canvas16(8)
    c.rect(0, 0, 15, 0, 10)
    c.rect(0, 1, 15, 1, 9)
    c.vline(0, 0, 15, 9)
    c.hline(0, 15, 15, 7)
    c.vline(15, 0, 15, 7)
    c.rect(3, 4, 12, 11, 9)
    c.rect(4, 5, 11, 10, 8)
    c.hline(4, 11, 5, 10)
    for rx, ry in ((1, 1), (14, 1), (1, 14), (14, 14)):
        c.px(rx, ry, 7)
    return c


def mt_girder():
    c = Canvas16(0)
    c.hline(0, 15, 0, 14)
    c.rect(0, 1, 15, 2, 5)
    c.hline(0, 15, 3, 3)
    # X-lattice under the beam
    for x in range(16):
        c.px(x, 4 + (x % 8), 3)
        c.px(x, 11 - (x % 8), 3)
    c.hline(0, 15, 12, 3)
    return c


def mt_pipe():
    c = Canvas16(0)
    c.rect(5, 0, 10, 15, 12)
    c.vline(5, 0, 15, 11)
    c.vline(10, 0, 15, 11)
    c.vline(7, 0, 15, 14)
    c.rect(3, 6, 12, 7, 11)
    c.hline(3, 12, 6, 12)
    return c


def mt_window():
    c = Canvas16(0)
    c.rect(2, 1, 13, 14, 13)
    c.rect(3, 2, 12, 13, 2)
    c.vline(3, 2, 13, 3)
    c.hline(3, 12, 2, 3)
    c.px(6, 5, 14)
    c.px(10, 9, 14)
    c.px(5, 11, 6)
    return c


def t_star(kind):
    t = [[0] * 8 for _ in range(8)]
    if kind == 1:
        t[2][3] = 14
        t[1][3] = 6
        t[3][3] = 6
        t[2][2] = 6
        t[2][4] = 6
    else:
        t[5][5] = 14
        t[6][2] = 6
    return t


def tile_to_4bpp(t):
    """8x8 palette-index rows -> SNES 4bpp planar (32 bytes)"""
    out = bytearray(32)
    for y in range(8):
        b0 = b1 = b2 = b3 = 0
        for x in range(8):
            v = t[y][x]
            bit = 7 - x
            b0 |= ((v >> 0) & 1) << bit
            b1 |= ((v >> 1) & 1) << bit
            b2 |= ((v >> 2) & 1) << bit
            b3 |= ((v >> 3) & 1) << bit
        out[y * 2] = b0
        out[y * 2 + 1] = b1
        out[16 + y * 2] = b2
        out[16 + y * 2 + 1] = b3
    return bytes(out)


def write_png(path, w, h, rgb_rows):
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + row for row in rgb_rows)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main():
    os.makedirs(GEN, exist_ok=True)

    tiles = [t_empty()]
    for c in (mt_hull(), mt_hull2(), mt_brass(), mt_girder(), mt_pipe(), mt_window()):
        tiles.extend(c.quads())
    tiles.append(t_star(1))
    tiles.append(t_star(2))
    assert len(tiles) == TILE_COUNT, len(tiles)

    with open(os.path.join(GEN, "tiles.chr"), "wb") as f:
        for t in tiles:
            f.write(tile_to_4bpp(t))

    with open(os.path.join(GEN, "tiles.pal"), "wb") as f:
        for rgb in PAL:
            f.write(struct.pack("<H", bgr555(rgb)))

    # identity remap for tmx2snes (we never dedup: ids are authored)
    with open(os.path.join(GEN, "tiles.map"), "wb") as f:
        for i in range(TILE_COUNT):
            f.write(struct.pack("<H", i))

    # sheet PNG: 16 tiles per row (Tiled 'columns' in mkmaps.py must match)
    cols, rows = 16, (TILE_COUNT + 15) // 16
    w, h = cols * 8, rows * 8
    pix = [[PAL[0]] * w for _ in range(h)]
    for i, t in enumerate(tiles):
        ox, oy = (i % cols) * 8, (i // cols) * 8
        for y in range(8):
            for x in range(8):
                pix[oy + y][ox + x] = PAL[t[y][x]]
    rgb_rows = []
    for y in range(h):
        row = bytearray()
        for x in range(w):
            v = pix[y][x]
            row += bytes(((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF))
        rgb_rows.append(bytes(row))
    write_png(os.path.join(GEN, "tiles.png"), w, h, rgb_rows)

    print("mktiles: %d tiles -> tiles.chr/pal/map/png" % len(tiles))


if __name__ == "__main__":
    main()
