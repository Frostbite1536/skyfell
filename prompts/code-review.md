# Code review — Skyfell

Multi-pass review of the current diff/PR. Severities and focus order come from `/REVIEW.md`; rules from `docs/INVARIANTS.md`. Read changed files in full, not just hunks.

Pass 1 — **Invariants**: walk INVARIANTS.md against the change; any violation is Critical.
Pass 2 — **Hardware/compiler traps**: REVIEW.md §2 checklist (16-bit int, banks, byte/word DMA math, vblank).
Pass 3 — **Cross-boundary**: new fields traced through every producer/consumer (converter → ROM → engine → save → debug block → Lua harness).
Pass 4 — **Tests**: behavior covered by a Lua test or golden replay? Golden changes have a DECISIONS.md entry? Runner output pasted and green?

Report: findings ranked Critical → Nit → Pre-existing, each with file:line + concrete failure scenario. State explicitly what you verified by running versus what you inferred by reading.
