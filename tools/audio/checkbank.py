#!/usr/bin/env python3
"""checkbank.py -- INV-AUD-001 build-time ARAM budget assert.

Parses smconv's generated soundbank.h and asserts that the resident ARAM
worst case fits: the effects module (sfx.it, always loaded first for
spcLoadEffect) plus the LARGEST music module must stay within the 58KB
module budget snesmod leaves after its ~6KB driver (the effectsandmusic
example's documented rule: "each combination of a music + effect must be
lower than 58K"). Also warns when the ROM-side .bnk approaches its single
dedicated 32KB LoROM bank (D-020 pins bank 5).

Usage: checkbank.py path/to/soundbank.h
"""
import os
import re
import sys

BUDGET = 58 * 1024
BANK_SIZE = 0x8000


def main():
    h = sys.argv[1]
    text = open(h, "r").read()
    sizes = dict(re.findall(r"#define\s+MOD_(\w+?)_SIZE\s+(\d+)", text))
    sizes = {k: int(v) for k, v in sizes.items()}
    if "SFX" not in sizes:
        print("INV-AUD-001 FAIL: no MOD_SFX in %s (effects IT must be first)" % h)
        return 1
    music = {k: v for k, v in sizes.items() if k != "SFX"}
    worst = sizes["SFX"] + (max(music.values()) if music else 0)
    print("INV-AUD-001: sfx=%d + largest module=%d -> %d / %d ARAM"
          % (sizes["SFX"], max(music.values()) if music else 0, worst, BUDGET))
    if worst > BUDGET:
        print("INV-AUD-001 FAIL: soundbank combination exceeds the ARAM budget")
        return 1
    bnk = os.path.splitext(h)[0] + ".bnk"
    if os.path.exists(bnk):
        sz = os.path.getsize(bnk)
        print("INV-AUD-001: soundbank.bnk = %d bytes (bank = %d)" % (sz, BANK_SIZE))
        if sz > BANK_SIZE:
            print("INV-AUD-001 FAIL: .bnk exceeds its dedicated 32KB LoROM bank")
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
