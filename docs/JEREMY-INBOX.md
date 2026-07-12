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
- Phase 3 gate (ROADMAP): "the spin feels *good*" — subjective on purpose. Chamber
  rotation duration/easing knobs land in tuning.h; fallback design (instant 90° cut +
  tumble) stays on the shelf per D-004.
- Milestone A (after Phase 3.5) is **your** go/no-go review — I will not record the
  verdict for you; the demo build + a played-through checklist will be waiting.

---

## Resolved

- **License (2026-07-12)**: ALL RIGHTS RESERVED for now, may open-source
  later — D-009 updated, README updated.
- **INV-ENG-004 exit-only teleport revalidation (2026-07-12)**: ratified
  ("sounds good") — INVARIANTS change log approval recorded. Still revisit
  at magnet-plates (Phase 5).
- **Controls (2026-07-12 playtest)**: Y=blue / X=gold two-button fire +
  stationary aim-lock shipped (D-013).
