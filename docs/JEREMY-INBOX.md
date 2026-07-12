# JEREMY-INBOX — decisions parked for Jeremy

Rule (from prophet): the build does NOT proceed past an item here — it routes around
it and keeps going. Each item names what's blocked, the options, and a recommendation.
Cross out / annotate and the next session acts on it.

---

## Open

### 1. License (D-009 — blocks nothing until first public release)
MIT vs. all-rights-reserved for the code. Pure homebrew either way (no Nintendo code
or assets). Prophet precedent: still open there too. **Recommendation: MIT for the
code, "assets all rights reserved" split, decided before any itch.io page.**

### 2. Overnight run 2026-07-11 — standing authorizations I assumed
You said "work on all the Phases, operate autonomously." I am: building phase by
phase, gating each on `make test` on a clean build, **committing per unit and pushing
per phase** (the Phase 0 pattern you set up — remote + CI green as proof). If you'd
rather I stop pushing overnight, say so and I'll keep commits local.

### 3. INV-ENG-004 refinement made on judgment — please ratify
Teleport now revalidates only the EXIT portal (entry was just crossed; terrain
is immutable in Phase 2; the redundant check cost measured transit-frame lag).
Full reasoning in INVARIANTS.md's change log. **Revisit before magnet-plates
(Phase 5) — moving portals break the "terrain is immutable" premise.**

### 4. Feel gates that only you can judge (accumulating during the run)
- Phase 1: run/jump feel — every knob in `src/game/tuning.h` (accel, friction, jump
  impulse, gravity, coyote/buffer frames). Tuned to the GDD movement-feel spec
  (run 1.5 px/f, apex ~2.2 metatiles); your hands decide.
- Phase 2: the Rift Gun in YOUR hands (`make run`): R+dpad aim, Y fire, X toggle
  blue/gold, Select recall, A grab/throw the crate. Fling feel knobs: TP_SPEED_CAP,
  TP_EJECT_MIN, TP_COOLDOWN, SENTRY_RANGE/PERIOD, CRATE_PUSH/THROW_* (tuning.h).
  Known visual gap on purpose: portals are plain slits (shimmer + sprite caps are
  the Phase 3.5 polish pass).
- Phase 3 gate (ROADMAP): "the spin feels *good*" — subjective on purpose. Chamber
  rotation duration/easing knobs land in tuning.h; fallback design (instant 90° cut +
  tumble) stays on the shelf per D-004.
- Milestone A (after Phase 3.5) is **your** go/no-go review — I will not record the
  verdict for you; the demo build + a played-through checklist will be waiting.

---

## Resolved

*(nothing yet)*
