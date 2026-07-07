# tools/ — build glue & test runner (Python 3, stdlib + Pillow only)

Planned inventory (created in the phase that needs them):

- `run_tests.py` (Phase 0) — discovers `tests/lua/test_*.lua`, invokes MesenCE testrunner per case, collects `artifacts/`, prints the pass/fail table (the project's gate output)
- `mapc.py` (Phase 0 fallback, only if the `tmx2snes` spike can't carry material metadata) — Tiled JSON → engine room binary + checksum
- `art/` (Phase 1+) — deterministic Pillow scripts for generated pixel art; committed, reviewable, re-runnable
- `trk.py` (Phase 6, optional per D-open) — score DSL → minimal Impulse Tracker `.it` writer feeding `smconv`
- `budget.py` (Phase 3.5) — asserts tile/ARAM/ROM budgets from build outputs (INV-HW-006, INV-AUD-001)

Rules: pinned toolchain binaries live outside the repo (`PVSNESLIB_HOME`, MesenCE install) — nothing vendored into `tools/bin/` (gitignored as a backstop). Converters must be deterministic: same input → byte-identical output, or golden tests get flaky.
