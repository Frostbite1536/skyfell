#!/usr/bin/env python3
"""mkit.py -- deterministic Impulse Tracker module generator (D-020).

Audio source of truth: this script. Outputs (generated, never hand-edited):
  src/data/generated/audio/sfx.it      6 SFX samples (jump, fire, portal-open,
                                       teleport, land, death), empty pattern.
  src/data/generated/audio/track01.it  Zone 1 ambient loop "The Gyre Hums".
                                       Its samples 1-6 DUPLICATE the SFX so
                                       spcLoadEffect(0..5) finds the same
                                       sounds whether it indexes the
                                       soundbank's first IT or the currently
                                       loaded module (the example reloads
                                       effects after every spcLoad -- both
                                       readings are live; D-020).

SNESMOD composition rules honored (pvsneslib_snesmod.txt, 4.5.0):
  instrument mode (header flag bit2), linear slides, old-effects OFF,
  <= 8 channels (music authored on 1-6; channel 8 is the driver's SFX
  voice), mono signed 8-bit samples, loop lengths multiple of 16, no echo
  (no [[SNESMOD]] song message -> no EDL ARAM tax), no envelopes/NNA.

Everything is synthesized -- no binary assets. LCG noise, fixed seed.
"""
import math
import os
import struct

RATE = 16000
OUT = os.path.join("src", "data", "generated", "audio")

# ---------------------------------------------------------------- synthesis

_lcg = 0x00C0FFEE


def _noise():
    global _lcg
    _lcg = (_lcg * 1103515245 + 12345) & 0x7FFFFFFF
    return (_lcg / 0x3FFFFFFF) - 1.0


def _quantize(f):
    """floats -1..1 -> signed 8-bit bytes, padded to a multiple of 16."""
    out = bytearray()
    for v in f:
        s = int(round(v * 112.0))
        s = max(-127, min(127, s))
        out.append(s & 0xFF)
    while len(out) % 16:
        out.append(0)
    return bytes(out)


def _sweep(dur, f0, f1, wave, env):
    """Phase-accumulated exponential sweep f0->f1 over dur seconds."""
    n = int(dur * RATE)
    k = math.log(f1 / f0) / n
    out = []
    ph = 0.0
    for i in range(n):
        f = f0 * math.exp(k * i)
        ph += f / RATE
        out.append(wave(ph, i / RATE) * env(i / n, i / RATE))
    return out


def sfx_jump():
    return _sweep(0.10, 250.0, 560.0,
                  lambda ph, t: 0.75 if (ph % 1.0) < 0.5 else -0.75,
                  lambda x, t: (1.0 - x) ** 1.5)


def sfx_fire():
    def wave(ph, t):
        saw = 2.0 * (ph % 1.0) - 1.0
        return saw * 0.8 + _noise() * 0.35 * math.exp(-t / 0.012)
    return _sweep(0.12, 1500.0, 250.0, wave, lambda x, t: (1.0 - x) ** 2.0)


def sfx_open():
    def wave(ph, t):
        return 0.62 * math.sin(2 * math.pi * ph) \
             + 0.30 * math.sin(4 * math.pi * ph)
    return _sweep(0.15, 380.0, 760.0, wave,
                  lambda x, t: math.sin(math.pi * x) ** 0.7)


def sfx_teleport():
    def wave(ph, t):
        return math.sin(2 * math.pi * ph) * (0.55 + 0.35 * math.sin(2 * math.pi * 30.0 * t))
    return _sweep(0.14, 260.0, 1500.0, wave, lambda x, t: (1.0 - x) ** 1.2)


def sfx_land():
    n = int(0.07 * RATE)
    out = []
    ph = 0.0
    for i in range(n):
        t = i / RATE
        ph += 105.0 / RATE
        tri = 2.0 * abs(2.0 * (ph % 1.0) - 1.0) - 1.0
        v = tri * 0.85 * (1.0 - i / n) ** 2.0
        if t < 0.004:
            v += _noise() * 0.5 * (1.0 - t / 0.004)
        out.append(v)
    return out


def sfx_death():
    def wave(ph, t):
        saw = 2.0 * (ph % 1.0) - 1.0
        sq = 1.0 if (ph % 1.0) < 0.5 else -1.0
        trem = (1.0 + 0.3 * math.sin(2 * math.pi * 9.0 * t)) / 1.3
        return (saw * 0.6 + sq * 0.25) * trem
    return _sweep(0.26, 420.0, 75.0, wave, lambda x, t: (1.0 - x) ** 1.2)


def smp_musicbox():
    """Bright decaying pluck, fundamental C5 -> C5Speed = RATE."""
    n = 6400
    out = []
    w = 2 * math.pi * 523.25
    for i in range(n):
        t = i / RATE
        v = (math.sin(w * t) * math.exp(-t / 0.14)
             + 0.22 * math.sin(2 * w * t) * math.exp(-t / 0.05)
             + 0.30 * math.sin(3 * w * t) * math.exp(-t / 0.035))
        if t < 0.002:
            v *= t / 0.002
        out.append(v * 0.62)
    return out


def smp_bass():
    """One 128-sample looped cycle; mellow low-brass-ish partial stack.
    C5Speed = 128 * 523.25 so IT note pitch mapping stays true."""
    amps = [1.0, 0.45, 0.30, 0.15, 0.08]
    out = []
    for i in range(128):
        x = i / 128.0
        v = sum(a * math.sin(2 * math.pi * (h + 1) * x) for h, a in enumerate(amps))
        out.append(v * 0.42)
    return out


def smp_pad():
    """Seamless 3072-sample looped 'air': LCG noise, circular moving average."""
    n = 3072
    raw = [_noise() for _ in range(n)]
    win = 24
    acc = sum(raw[-win:])
    out = []
    for i in range(n):
        acc += raw[i] - raw[(i - win) % n]
        out.append(acc / win * 1.8)
    peak = max(abs(v) for v in out) or 1.0
    return [v / peak * 0.8 for v in out]


# ---------------------------------------------------------- IT file writing

def it_instrument(sample_1based):
    d = bytearray(554)
    d[0:4] = b"IMPI"
    d[0x14:0x16] = struct.pack("<H", 256)  # fadeout (note-off unused anyway)
    d[0x18] = 128                          # global volume
    d[0x19] = 32 + 128                     # default pan: off
    d[0x1C:0x1E] = struct.pack("<H", 0x0214)
    for i in range(120):                   # whole keyboard -> the one sample
        d[0x40 + 2 * i] = i
        d[0x40 + 2 * i + 1] = sample_1based
    return bytes(d)


def it_sample_header(pcm, c5speed, loop=None):
    d = bytearray(80)
    d[0:4] = b"IMPS"
    d[0x11] = 64                            # global volume
    d[0x12] = 1 | (16 if loop else 0)       # has data (+ loop)
    d[0x13] = 64                            # default volume
    d[0x2E] = 1                             # Cvt: signed PCM
    d[0x2F] = 32                            # default pan (bit7 off)
    d[0x30:0x34] = struct.pack("<I", len(pcm))
    lb, le = loop if loop else (0, 0)
    d[0x34:0x38] = struct.pack("<I", lb)
    d[0x38:0x3C] = struct.pack("<I", le)
    d[0x3C:0x40] = struct.pack("<I", c5speed)
    # sample pointer (0x48) patched at layout time
    return d


def it_pattern(rows, events):
    """events: {(row, ch): (note, ins_1based, vol_or_None)} -> packed bytes."""
    body = bytearray()
    for r in range(rows):
        for ch in range(8):
            ev = events.get((r, ch))
            if not ev:
                continue
            note, ins, vol = ev
            body.append(0x80 | (ch + 1))
            body.append(1 | 2 | (4 if vol is not None else 0))
            body.append(note)
            body.append(ins)
            if vol is not None:
                body.append(vol)
        body.append(0)
    return struct.pack("<HH4x", len(body), rows) + bytes(body)


def write_it(path, songname, orders, instruments, samples, patterns,
             speed=6, tempo=125):
    """samples: list of (header_bytearray, pcm_bytes)."""
    orders = list(orders) + [255]
    hdr = bytearray(192)
    hdr[0:4] = b"IMPM"
    hdr[4:30] = songname.encode("ascii")[:26].ljust(26, b"\x00")
    hdr[30] = 4
    hdr[31] = 16
    struct.pack_into("<HHHH", hdr, 32, len(orders), len(instruments),
                     len(samples), len(patterns))
    struct.pack_into("<HH", hdr, 40, 0x0214, 0x0214)
    struct.pack_into("<H", hdr, 44, 1 | 4 | 8)  # stereo | instruments | linear
    hdr[48] = 128                               # global volume
    hdr[49] = 48                                # mix volume
    hdr[50] = speed
    hdr[51] = tempo
    hdr[52] = 128                               # pan separation
    for c in range(64):
        hdr[64 + c] = 32 if c < 8 else 32 + 128
        hdr[128 + c] = 64

    pos = 192 + len(orders) + 4 * (len(instruments) + len(samples) + len(patterns))
    ins_off = []
    for ins in instruments:
        ins_off.append(pos)
        pos += len(ins)
    smp_off = []
    for h, _ in samples:
        smp_off.append(pos)
        pos += len(h)
    pat_off = []
    for p in patterns:
        pat_off.append(pos)
        pos += len(p)
    blobs = []
    for h, pcm in samples:
        struct.pack_into("<I", h, 0x48, pos)
        blobs.append(pcm)
        pos += len(pcm)

    with open(path, "wb") as f:
        f.write(hdr)
        f.write(bytes(orders))
        for o in ins_off + smp_off + pat_off:
            f.write(struct.pack("<I", o))
        for ins in instruments:
            f.write(ins)
        for h, _ in samples:
            f.write(h)
        for p in patterns:
            f.write(p)
        for b in blobs:
            f.write(b)


# ------------------------------------------------------------- composition

BASS_C5 = 128 * 523  # looped 128-sample cycle: note pitch mapping stays true

MELODY_P0 = [(0, 76), (4, 72), (8, 69), (14, 71),
             (16, 69), (20, 65), (24, 72),
             (32, 67), (36, 76), (40, 74),
             (48, 71), (52, 74), (56, 67)]
MELODY_P1 = [(0, 69), (6, 72), (12, 76),
             (16, 72), (22, 69), (28, 65),
             (32, 76), (38, 67), (44, 72),
             (48, 74), (54, 71), (60, 69)]
BASS_LINE = [(0, 33), (16, 29), (32, 36), (48, 31)]  # A2 F2 C3 G2


def music_pattern(melody):
    ev = {}
    for row, note in BASS_LINE:
        ev[(row, 0)] = (note, 2, 36)
    for row, note in melody:
        ev[(row, 1)] = (note, 1, 46)
        if row + 3 < 64:                       # music-box shimmer echo
            ev[(row + 3, 2)] = (note, 1, 18)
    ev[(0, 3)] = (60, 3, 9)                    # air pad, once per pattern
    return it_pattern(64, ev)


def main():
    os.makedirs(OUT, exist_ok=True)

    sfx = [sfx_jump(), sfx_fire(), sfx_open(), sfx_teleport(),
           sfx_land(), sfx_death()]
    sfx_pcm = [_quantize(s) for s in sfx]

    # sfx.it: 6 samples, 6 instruments, one empty pattern
    write_it(os.path.join(OUT, "sfx.it"), "skyfell sfx",
             orders=[0],
             instruments=[it_instrument(i + 1) for i in range(6)],
             samples=[(it_sample_header(p, RATE), p) for p in sfx_pcm],
             patterns=[it_pattern(64, {})])

    # track01.it: samples 1-6 = the SFX duplicates (D-020), 7-9 = music
    music = [(_quantize(smp_musicbox()), RATE, None),
             (_quantize(smp_bass()), BASS_C5, (0, 128)),
             (_quantize(smp_pad()), RATE, (0, 3072))]
    samples = [(it_sample_header(p, RATE), p) for p in sfx_pcm]
    samples += [(it_sample_header(p, c5, lp), p) for p, c5, lp in music]
    write_it(os.path.join(OUT, "track01.it"), "the gyre hums",
             orders=[0, 1],
             instruments=[it_instrument(7), it_instrument(8), it_instrument(9)],
             samples=samples,
             patterns=[music_pattern(MELODY_P0), music_pattern(MELODY_P1)])

    print("mkit: wrote sfx.it (%d SFX) + track01.it" % len(sfx))


if __name__ == "__main__":
    main()
