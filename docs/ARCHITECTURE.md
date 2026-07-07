# Architecture — RIFT: The Skyfell Engine

System design for a SNES cartridge game. The "stack" is the console itself; there are no external services. Our dependencies are build-time tools, pinned by version.

## Bird's-eye view

```
 ┌────────────────────────── build time (Windows/Linux) ──────────────────────────┐
 │  assets/*.png ──gfx4snes──▶ tiles+pal    assets/*.tmx ──tmx2snes/mapc──▶ maps  │
 │  assets/music/*.it ──smconv──▶ soundbank  assets/sfx/*.wav ──snesbrr──▶ BRR    │
 │  src/*.c ──816-tcc──▶ asm ──816-opt──▶ ──wla-dx──▶ build/skyfell.sfc           │
 └─────────────────────────────────────────────────────────────────────────────────┘
 ┌────────────────────────────── run time (SNES) ──────────────────────────────────┐
 │  65816 @3.58MHz (FastROM)                    SPC700 + S-DSP (64KB ARAM)         │
 │  ┌─ Main loop ────────────┐  ┌─ NMI (vblank) ─────────┐  ┌─ SNESMOD driver ──┐ │
 │  │ input → sim → build     │  │ OAM DMA, VRAM queue    │  │ music + SFX,      │ │
 │  │ OAM shadow + VRAM queue │─▶│ drain, CGRAM, scroll,  │  │ runs independent  │ │
 │  │ then WAI for NMI        │  │ Mode7 matrix, joypad   │  │ of main CPU       │ │
 │  └─────────────────────────┘  └────────────────────────┘  └───────────────────┘ │
 └─────────────────────────────────────────────────────────────────────────────────┘
```

**Cardinal rule** (INV-HW-001): game code never touches PPU registers/VRAM/CGRAM/OAM directly during active display — everything goes through the **vblank queue**, drained by the NMI handler. This one discipline prevents 90% of classic SNES corruption bugs.

## Modules (`src/`)

| Module | Responsibility |
|---|---|
| `core/` | boot/init, NMI handler, vblank queue, DMA helpers, fixed-point math + sin/cos LUTs, RNG (seeded), joypad |
| `game/player.c` | FSM, physics, aiming |
| `game/portal.c` | shot, placement rules, teleport transform LUT, portal rendering |
| `game/gravity.c` | gravity frame, chamber rotation state machine, Mode 7 matrix driver |
| `game/entity.c` | 16-slot pool, function-pointer update table; enemies as entity types |
| `game/room.c` | metatile grid, collision queries, room load, door transitions, camera + BG streaming |
| `game/save.c` | SRAM slots, checksums |
| `ui/` | HUD (BG3 or sprites in chambers), title, file select, pause map |
| `audio/` | SNESMOD glue, SFX priority table |
| `data/` | **generated** — converter outputs linked into banks (never hand-edited) |

Hot paths (collision inner loop, OAM build) may drop to assembly *after* profiling shows tcc816 output is the bottleneck — not before (D-001).

## Memory maps

### WRAM
| Range | Use |
|---|---|
| `$0000–00FF` | direct page: engine hot state |
| `$0100–01FF` | stack |
| `$0200–043F` | OAM shadow (544B) |
| `$1F00–1FFF` | **debug/test block** (TEST builds; INV-TEST-001) — magic `0x51FE`, frame, player pos/vel, gravity, portals, room id, warp-request mailbox for Lua |
| `$2000–…` | entity pool, room state, portal state, save staging |
| `$7F0000–…` | room metatile cache, decompression scratch |

### VRAM — Mode 1 rooms (bytes)
| Range | Use |
|---|---|
| `$0000–3FFF` | BG1/BG2 char, 4bpp (512 tiles) |
| `$4000–5FFF` | OBJ char (16KB); player's current frame streamed ~256B/vblank |
| `$6000–6FFF` | BG1 + BG2 maps (64×32 each) |
| `$7000–7FFF` | BG3 HUD map + 2bpp tiles |

### VRAM — Mode 7 chambers (bytes)
| Range | Use |
|---|---|
| `$0000–7FFF` | Mode 7 interleaved map(low)/char(high) — hardware-fixed layout |
| `$8000–BFFF` | OBJ char |
| `$C000–FFFF` | free / chamber HUD sprites |

Layout swap happens only behind force-blank door transitions.

### CGRAM
Mode 1: palettes 0–7 BG, 8–15 OBJ. Mode 7 chambers: tile colors **0–127 only**, sprites keep 128–255 (INV-HW-006).

### ROM (LoROM + FastROM, 512KB start)
| Banks | Use |
|---|---|
| 0–1 | code (hot code bank 0), LUTs |
| 2–3 | soundbank (SNESMOD) |
| 4+ | per-zone assets (tiles/maps/metadata); uncompressed until ROM pressure says otherwise |

SRAM 8KB: 3 save slots, versioned + checksummed.

## Key flows

**Frame** (60Hz hard): read pad (auto-joypad) → player FSM/physics → portal + entity updates → collision → camera → build OAM shadow + queue VRAM writes → `WAI`. NMI drains queues (≤4KB/frame budget, INV-HW-002), sets scroll/matrix, flips frame counter.

**Teleport**: overlap test vs portal plane → look up (in-orient, out-orient) in 16-entry transform table → rewrite pos/vel → cooldown timer → queue flash (2-frame dim) + SFX.

**Chamber rotation**: trigger (exit portal) → freeze sim → interpolate θ via LUT, write M7A–D each frame (8 register writes, trivial) with pivot at player → snap to 0/90/180/270 → swap gravity vector + input mapping → unfreeze. World data never rotates; a 90°-swap transform maps world→screen for sprites.

**Room load**: fade → force-blank → DMA tileset/map/palettes → spawn entities from room data → fade in.

## Data formats (owned by us)

- **Metatile**: 4 tile ids + collision byte + material byte (empty/solid/brass/glass/magnet/hazard/water). Emitted by the map converter with a checksum the TEST build validates (INV-ENG-005).
- **Room**: header (size, music, type Mode1/chamber), metatile grid, entity spawn list, door table.
- **Debug block**: append-only layout, documented in `tests/README.md`.

## Failure modes & external deps

No runtime externals. Build-time deps pinned: PVSnesLib **4.5.0**, MesenCE (version recorded in CONTINUATION.md on install). Tool upgrades are deliberate decisions (DECISIONS.md), never drive-by. Emulator accuracy risk: gate on MesenCE, spot-check on ares near release.

## Performance & size budgets

~59.5k CPU cycles/frame at FastROM. Sim ≤60% of frame (measure via Mesen profiler + scanline check, recorded per phase). VRAM/ARAM/tile budgets in INVARIANTS. Files ≤ ~500 lines; one module per system.

## Design decisions

Recorded with alternatives in [DECISIONS.md](./DECISIONS.md): C-over-asm (D-001), MesenCE (D-002), axis-aligned portals (D-003), Mode 7 confined to chambers (D-004), fixed-world/rotating-camera gravity model + round chamber enemies (D-005), LoROM+FastROM (D-006), SafeVibeCoding doc structure with N/A tiers skipped (D-007), NTSC-only v1 (D-008), license TBD (D-009).

---
**Last Updated**: 2026-07-07 (planning — subject to Phase 0/1 reality)
