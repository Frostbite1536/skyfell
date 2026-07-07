# src/ — game code (C on PVSnesLib 4.5.0 + targeted 65816 asm)

Module layout (populated starting Phase 0/1 — see docs/ARCHITECTURE.md for responsibilities and memory maps):

```
core/    boot, NMI + vblank queue, DMA helpers, fixed-point math, sin/cos LUTs, RNG, joypad
game/    player.c portal.c gravity.c entity.c room.c save.c camera.c tuning.h
ui/      hud.c title.c fileselect.c pausemap.c
audio/   snesmod glue, sfx priority table
data/    GENERATED converter output only — never hand-edit
```

House rules (full list in /CLAUDE.md): fixed-point only (no floats), explicit-width types (`int` is 16-bit under tcc816), all tunables in `game/tuning.h`, PPU access only via the vblank queue, files ≤ ~500 lines.
