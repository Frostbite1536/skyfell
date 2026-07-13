# JEREMY-INBOX — decisions parked for Jeremy

Rule (from prophet): the build does NOT proceed past an item here — it routes around
it and keeps going. Each item names what's blocked, the options, and a recommendation.
Cross out / annotate and the next session acts on it.

---

## Open

### 2. Overnight run 2026-07-11 — standing authorizations I assumed
You said "work on all the Phases, operate autonomously." I am: building phase by
phase, gating each on `make test` on a clean build, **committing per unit and pushing
per phase** (the Phase 0 pattern you set up — remote + CI green as proof). If you'd
rather I stop pushing overnight, say so and I'll keep commits local.

### 4. Feel gates that only you can judge (accumulating during the run)
- Phase 1: run/jump feel — every knob in `src/game/tuning.h` (accel, friction, jump
  impulse, gravity, coyote/buffer frames). Tuned to the GDD movement-feel spec
  (run 1.5 px/f, apex ~2.2 metatiles); your hands decide.
- Phase 2: the Rift Gun in YOUR hands (`make run`): R+dpad aim (Wren stands
  still while aiming — your 2026-07-12 note), **Y fires BLUE, X fires GOLD**
  (D-013 — the old fire+toggle read as "can't shoot the second portal"),
  Select recall, A grab/throw the crate. Fling feel knobs: TP_SPEED_CAP,
  TP_EJECT_MIN, TP_COOLDOWN, SENTRY_RANGE/PERIOD, CRATE_PUSH/THROW_*
  (tuning.h). Known visual gap on purpose: portals are plain slits (shimmer +
  sprite caps are the Phase 3.5 polish pass).
- The sentry shaft is now REACHABLE (your question — it was half-purposeful:
  a test arena; room01 is the test hall until Phase 3.5's authored Zone 1
  rooms). Route: climb the rung ladder to the long gantry, up to the row-18
  walkway, walk right across the (now shorter) brass tower's top, drop in.
  Pre-place a portal pair (gold on the floor pad BEFORE you drop) to get
  back out — the shaft walls are taller than your jump.
- Phase 3 gate (ROADMAP): "the spin feels *good*" — subjective on purpose, and
  the full loop is now IN YOUR HANDS: the Rift Gun fires inside the chamber
  (same D-013 controls — R aim-lock, Y blue / X gold, Select recall, A
  grab/drop; aim is what you see on screen). Fire onto the brass strips (all
  four walls), step through, and the room rotates around you. The spin knob is
  ROT_STEP in tuning.h (2 = 90° in 32 frames; bigger = snappier). Watch the
  round drone — it hides during the spin on purpose (sprites can't rotate).
  Fallback design (instant 90° cut + tumble) stays on the shelf per D-004.
  **The chamber is now also the Phase 3 puzzle (D-015)**: the crate sits on a
  ledge you can't jump to — flip gravity to shake it loose, carry it (A), get
  gravity UP, and drop it (A) on the green rune pad in the ceiling; the door
  beside the pad corner slides open — walk in. Two reorientations minimum.
  In the chamber, A-drop is a gentle straight drop (no throw) on purpose —
  say the word if you want the full toss back. **The chamber now has a real
  door (D-017): stand in the doorway at the bottom of the hall's pit and
  press UP.** Doors everywhere are UP-to-enter; the hall's west doorway leads
  to a second room, and solving the chamber puzzle walks you out through its
  exit recess back to the hall. (The old "pre-place a portal to escape the
  shaft" note is obsolete once you use the doors.)
- **Zone 1 is IN (new, D-022)** — the demo is now title → seven authored
  rooms (movement → portals → the big fling → crates → the sentry shaft →
  synthesis → the gate) → the chamber → end card → title. Your playthrough
  is the real gate: door graph + loads are machine-verified, and I'm
  scripting the route solves next, but FEEL (launch heights, gap widths,
  the crate hop, sentry timing) is yours. Design choices to veto if they
  read wrong in the hands: rooms 05/08's gaps are one-way eastward valves
  (you can always door back the long way); room06's plateau needs the
  crate hop (1.6px of apex margin — the second crate is there if stacking
  feels better); the end card loops to the title now, not the hall.
- **Audio (new, D-020)**: the game now has music + 6 SFX — all synthesized
  (a music-box-and-bass ambient loop, "The Gyre Hums"; jump/fire/portal-open/
  teleport/land/death effects). Your ears are the gate I can't run: `make run`
  and listen. Knobs if anything grates: per-SFX volume/pitch tables at the top
  of `src/audio/sound.c`, music volume = `spcSetModuleVolume(180)` there too,
  the composition itself = `tools/audio/mkit.py` (melody/bass tables at the
  bottom). Real tracked music is Phase 6 — a musician's .it file drops into
  the Makefile's AUDIOFILES unchanged.
- Milestone A (after Phase 3.5) is **your** go/no-go review — I will not record the
  verdict for you; the demo build + a played-through checklist will be waiting.

---

## Resolved

- **Phase 3 spin-feel gate + overall look (2026-07-12, post units A-D)**:
  "The feel of the game is good, it looks good so far." — Phase 3's
  subjective gate is GREEN; tuning stays as shipped (ROT_STEP=2 etc.).
  Feel gates in item 4 stay listed as knobs for future rounds, but nothing
  is blocked on them now.
- **License (2026-07-12)**: ALL RIGHTS RESERVED for now, may open-source
  later — D-009 updated, README updated.
- **INV-ENG-004 exit-only teleport revalidation (2026-07-12)**: ratified
  ("sounds good") — INVARIANTS change log approval recorded. Still revisit
  at magnet-plates (Phase 5).
- **Controls (2026-07-12 playtest)**: Y=blue / X=gold two-button fire +
  stationary aim-lock shipped (D-013).
