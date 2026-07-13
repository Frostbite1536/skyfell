"""mkmaps.py — ASCII metatile grids -> Tiled .tmj for tmx2snes (D-011/D-012).

Reads every assets/maps/*.txt (64x32 metatile chars, legend in tilesdef.py),
emits src/data/generated/maps/<name>.tmj — a minimal Tiled JSON map (8x8
tiles, one tilelayer named after the room, embedded tileset with per-tile
attribute/palette/priority string properties, exactly the shape tmx2snes
4.5.0's cute_tiled parser consumes; template = the pvsneslib likemario map).
Deterministic: sorted keys, no timestamps.

Room METADATA (D-021) is authored as @-header lines BEFORE the grid and
emitted as a <name>.meta.json sidecar (roomglue.py turns the sidecars into
the generated runtime tables):
  @entry X Y                       player entry px (box top-left) — required
  @door X0 X1 Y0 Y1 -> dest X Y    UP-to-enter trigger rect (px, inclusive)
                                   -> dest room name ("chamber" allowed) +
                                   arrival entry px (ignored for chamber)
  @spawn kind X Y                  kind: crate | sentry_l | sentry_r
"""
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from tilesdef import LEGEND, TILE_COUNT, tile_attrs  # noqa: E402

ROOT = os.path.join(os.path.dirname(__file__), "..")
MAPS = os.path.join(ROOT, "assets", "maps")
OUT = os.path.join(ROOT, "src", "data", "generated", "maps")

META_W, META_H = 64, 32
W, H = META_W * 2, META_H * 2  # 8x8 tile grid


def build_tileset():
    attrs = tile_attrs()
    tiles = []
    for i in range(TILE_COUNT):
        tiles.append({
            "id": i,
            "properties": [
                {"name": "attribute", "type": "string", "value": "%x" % attrs[i]},
                {"name": "palette", "type": "string", "value": "0"},
                {"name": "priority", "type": "string", "value": "0"},
            ],
        })
    return {
        "columns": 16,
        "firstgid": 1,
        "image": "tiles.png",
        "imageheight": ((TILE_COUNT + 15) // 16) * 8,
        "imagewidth": 128,
        "margin": 0,
        "name": "tiles",
        "spacing": 0,
        "tilecount": TILE_COUNT,
        "tileheight": 8,
        "tiles": tiles,
        "tilewidth": 8,
    }


def parse_meta(name, lines):
    meta = {"entry": None, "doors": [], "spawns": []}
    grid = []
    for ln in lines:
        if not ln.startswith("@"):
            grid.append(ln)
            continue
        tok = ln.split()
        if tok[0] == "@entry":
            meta["entry"] = [int(tok[1]), int(tok[2])]
        elif tok[0] == "@door":
            assert len(tok) == 9 and tok[5] == "->", "%s: bad @door: %s" % (name, ln)
            meta["doors"].append({"rect": [int(t) for t in tok[1:5]],
                                  "dest": tok[6],
                                  "x": int(tok[7]), "y": int(tok[8])})
        elif tok[0] == "@spawn":
            assert tok[1] in ("crate", "sentry_l", "sentry_r"), \
                "%s: unknown spawn kind %s" % (name, tok[1])
            meta["spawns"].append({"kind": tok[1], "x": int(tok[2]), "y": int(tok[3])})
        else:
            raise AssertionError("%s: unknown metadata line: %s" % (name, ln))
    assert meta["entry"] is not None, "%s: @entry is required (D-021)" % name
    return meta, grid


def convert(txt_path):
    name = os.path.splitext(os.path.basename(txt_path))[0]
    with open(txt_path) as f:
        lines = [ln.rstrip("\n") for ln in f if ln.strip("\n") != ""]
    meta, lines = parse_meta(name, lines)
    with open(os.path.join(OUT, name + ".meta.json"), "w", newline="\n") as f:
        json.dump(meta, f, sort_keys=True, separators=(",", ":"))
    assert len(lines) == META_H, "%s: want %d rows, got %d" % (name, META_H, len(lines))
    data = [0] * (W * H)
    for my, ln in enumerate(lines):
        assert len(ln) == META_W, "%s row %d: want %d cols, got %d" % (name, my, META_W, len(ln))
        for mx, ch in enumerate(ln):
            tl, tr, bl, br = LEGEND[ch]
            ty, tx = my * 2, mx * 2
            data[ty * W + tx] = tl + 1        # Tiled gid = tile id + firstgid
            data[ty * W + tx + 1] = tr + 1
            data[(ty + 1) * W + tx] = bl + 1
            data[(ty + 1) * W + tx + 1] = br + 1

    tmj = {
        "compressionlevel": -1,
        "height": H,
        "infinite": False,
        "layers": [{
            "data": data,
            "height": H,
            "id": 1,
            "name": name,
            "opacity": 1,
            "type": "tilelayer",
            "visible": True,
            "width": W,
            "x": 0,
            "y": 0,
        }],
        "nextlayerid": 2,
        "nextobjectid": 1,
        "orientation": "orthogonal",
        "renderorder": "right-down",
        "tiledversion": "1.10.2",
        "tileheight": 8,
        "tilesets": [build_tileset()],
        "tilewidth": 8,
        "type": "map",
        "version": "1.10",
        "width": W,
    }
    out = os.path.join(OUT, name + ".tmj")
    with open(out, "w", newline="\n") as f:
        json.dump(tmj, f, sort_keys=True, separators=(",", ":"))
    print("mkmaps: %s -> %s (%dx%d tiles)" % (os.path.basename(txt_path), out, W, H))


def main():
    os.makedirs(OUT, exist_ok=True)
    # room grids only — chamber*.txt belongs to mkchamber.py (Mode 7 world)
    srcs = sorted(glob.glob(os.path.join(MAPS, "room*.txt")))
    assert srcs, "no assets/maps/room*.txt found"
    for s in srcs:
        convert(s)


if __name__ == "__main__":
    main()
