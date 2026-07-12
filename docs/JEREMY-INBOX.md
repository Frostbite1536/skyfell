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

### 3. Feel gates that only you can judge (accumulating during the run)
- Phase 1: run/jump feel — every knob in `src/game/tuning.h` (accel, friction, jump
  impulse, gravity, coyote/buffer frames). I tune to Mesen playback + test numbers;
  your hands decide.
- Phase 3 gate (ROADMAP): "the spin feels *good*" — subjective on purpose. Chamber
  rotation duration/easing knobs land in tuning.h; fallback design (instant 90° cut +
  tumble) stays on the shelf per D-004.
- Milestone A (after Phase 3.5) is **your** go/no-go review — I will not record the
  verdict for you; the demo build + a played-through checklist will be waiting.

---

## Resolved

*(nothing yet)*
