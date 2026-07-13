# JEREMY-INBOX — decisions parked for Jeremy

Rule (from prophet): the build does NOT proceed past an item here — it routes around
it and keeps going. Each item names what's blocked, the options, and a recommendation.
Cross out / annotate and the next session acts on it.

---

## Open

### ★ MILESTONE A — your go/no-go (the build is waiting)

`make run` builds and launches the release ROM. The demo is complete:
**title → Zone 1's seven rooms → the gravity chamber → end card → title.**
Every route is machine-proven solvable (22/22 emulator tests, the full
walkthrough pad-driven); what only you can judge is whether it's FUN.
The played-through checklist:

1. Title: START drops you into room03. Run right, hop the girders,
   cross the little pit. (Movement feel — final check.)
2. Room04: gold (X) flat at the far pillar from the plateau edge, drop
   in the pit, blue (Y) at your feet on the brass, hop in — you pop out
   high over the far plateau. **Select-recall before walking to the
   door** or the pillar portal bounces you back (that's by design —
   veto it if it reads as a bug in the hands).
3. Room05: climb the tower stairs, drop east, crawl the gap (fall in,
   jump out). Optional: the portal fling across — blue on the floor pad,
   gold high on the tower face (fired up-left from east of the gap).
4. Room06: grab the crate (A), carry it to the tall wall, throw it (A),
   hop crate → plateau. (The hop clears by 1.6px — tell me if it ever
   feels like a miss you "should" have made.)
5. Room07: over the walkway, drop BEHIND the sentry (it faces away).
   Watch its shots. Blue on the gold tower face beside you, gold on the
   pillar it's shooting at (fire down-right from the tower top before
   dropping, or jump-fire from the shaft). Then GET OUT of the corridor
   — climb the pillar steps — and watch it die to its own shot.
6. Room08: everything at once — tower, gap, fling, crate, plateau.
7. Room09: climb the switchbacks, UP at the gate — the chamber.
8. The chamber puzzle (you know this one) → THE END CARD → START loops
   to the title.
9. Audio: the whole run has music + 6 SFX. Anything that grates, name it.

**Record your verdict here: continue / re-scope / stop.** (ROADMAP's
Milestone A gate — I will not fill it in for you.)

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
- **Zone 1 is IN and route-proven (D-022/D-022b)** — the demo is title →
  seven rooms (movement → portals → the fling crossing → crates → the
  sentry shaft → synthesis → the gate) → the chamber → end card → title,
  and the ENTIRE route is now pad-driven green in test_zone1_solve_a/b
  (every crux solved by scripted inputs; POS_SET only on stair climbs).
  FEEL is yours. Two design calls to veto or bless:
  - **room07's sentry faces AWAY from you** (blind-spot setup: watch its
    shots, place both portals safely, then get out of the corridor before
    its own shot comes home — expect ONE surprising death the first time;
    respawn is the room entry, 3 seconds away).
  - **rooms 05/08: the fling lands in/past the crawlable gap** rather
    than a clean far-shore touchdown — after portal setup you can't
    re-climb the tower, so the jump-in fling is the available one (it
    still crosses; room08's carries ~280px). The cinematic full-power
    tower-drop fling exists in the physics — say the word and I'll add
    an east-side re-climb path so players can earn it.
  Also: room04 teaches Select-recall (its exit door was moved off the
  pillar so a live portal can't ping-pong you back — leftover portals
  near doors are real gameplay now).
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
