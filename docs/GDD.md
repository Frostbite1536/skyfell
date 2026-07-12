# RIFT: The Skyfell Engine — Game Design Document

*Working title. SNES ROM header name: `RIFT SKYFELL ENGINE` (19/21 chars).*

## Premise

A floating kingdom is held aloft by an ancient machine, the **Skyfell Engine**. It has just shattered — the land has broken into drifting islands and gravity has gone haywire. You are **Wren**, an apprentice riftwright who salvages a prototype **Rift Gun** that fires two linked portals (blue and gold). You descend through the guts of the broken Engine to rebuild it, and slowly uncover who sabotaged it.

Tone: colorful fantasy-tech (brass, verdigris, warm gold light) — characterful SNES vibrance, not sterile lab-white.

## Design Pillars

1. **Portals are physical.** Momentum is conserved: go in fast, come out fast. Every puzzle and fight trusts this one rule.
2. **Gravity is a puzzle piece.** In chamber rooms, portals redefine "down," and the world visibly rotates around you (Mode 7) while you stay upright.
3. **Honest 16-bit.** Every effect is something a 1994 cartridge could do. Constraints are design fuel, not apologies.
4. **Puzzle-flavored combat.** Enemies are solved more than shot: reflect, drop, fling.

## Core Mechanics

### The Rift Gun

| Rule | Spec |
|---|---|
| Portals | Exactly 1 blue + 1 gold worldwide; refiring a color moves it |
| Valid surfaces | Axis-aligned only (4 orientations), material-gated: **brass** (start) → **glass** (upgrade: portals on glass, shots pass through glass) → **magnet-plates** (upgrade: portals slide on rails mid-flight) |
| Opening size | 48px (3 metatiles); placement needs clearance, rejects entity-occupied cells |
| Shot | Straight-line 4px/f; fizzles on invalid surface |
| Teleport | Position+velocity transformed by (in,out) orientation pair; speed cap 6px/f; 8-frame re-entry cooldown; works for player, crates, enemy shots |
| Recall | Select removes both portals |

### Gravity Chambers (the marquee)

Special rooms inside the Engine's broken gyre rings. Exiting a chamber portal sets **gravity = −(exit surface's outward normal)**: step through a portal onto the left wall and that wall *becomes the floor* — the entire chamber smoothly rotates ~90° around you (Mode 7), you stay upright, physics resumes in the new frame. Standard rooms never change gravity; chambers are marked by their ring-and-gimbal look.

Chamber puzzle verbs: drop a crate "up" onto a switch, pour water sideways into a wheel, route a sunbeam around a corner, build slingshot loops to reach the "ceiling."

### Movement feel (initial tuning — all knobs in `src/game/tuning.h`)

Run max 1.5 px/f, jump apex ≈ 2 metatiles, gravity 0.1875 px/f², terminal 4 px/f, coyote 4f, jump buffer 4f. Fling preserves |v| up to 6 px/f — clearing a 10-metatile chasm off a good drop should feel *earned*.

## Controls (SNES pad)

| Input | Action |
|---|---|
| D-pad | Move / aim direction |
| B | Jump |
| Y | Fire BLUE portal |
| X | Fire GOLD portal |
| A | Grab/throw crate, interact |
| R (hold) | 8-way aim lock |
| L | Crouch / peek down |
| Start | Pause + map |
| Select | Recall both portals |

## Structure: Metroidvania

Portal materials gate the world like beam upgrades. Backtracking constantly reopens: a glass wall you stared through in hour one becomes a portal route in hour three.

| Zone | Theme | Teaches |
|---|---|---|
| 1. The Gantries | Shipyard scaffolds in the sky | Movement, portals, fling, crates, sentries |
| 2. Cistern of Winds | Water/steam works | Fluid + momentum routing |
| 3. The Foundry | Heat, rails | Magnet-plate moving portals |
| 4. Glasshouse Gardens | Overgrown solarium | Glass portals, light-beam puzzles |
| 5. The Core Vault | The Engine's heart | Chamber gauntlet → finale |
| Hub: Tinker's Roost | Wren's workshop | Save, story, upgrades |

## Enemies

- **Sentry** — fixed turret, straight shots. Solution: portal its own shot back.
- **Skitter** — ledge-walker; fling or crate-drop.
- **Gale Drone** — chamber floater; *radially symmetric sprite* (SNES sprites can't rotate — round enemies make chamber rotation seamless).
- **Brass Wasp** — dive-bomber (Zone 2+).
- **Custodian** — miniboss, shielded front; requires a behind-portal.
- **Keeper of the Core** — boss: a radially symmetric ring-machine bolted to the chamber's heart. Each phase it re-anchors to a new surface and the arena rotates — you re-solve "which way is up" under fire.

## Presentation

- **Video**: Mode 1 for standard play (2×4bpp BG + 2bpp HUD), Mode 7 for chambers. 256×224 NTSC, 60fps always.
- **Art**: 16×16 metatiles; player 16×32; palette story per zone (Gantries = brass/teal/gold dawn). Strict tile budgets + documented reuse maps per zone (`assets/README.md`) — big worlds from small sheets is the craft.
- **Audio**: SPC700/8 voices — 6 music, 2 reserved SFX. Impulse Tracker → SNESMOD. Zone themes lean warm and mechanical (music-box + low brass + air).
- **Accessibility**: teleport flash ≤2 frames and dim (photosensitivity), no >3Hz strobing; map + objective hint from the hub.

## Story beats (light touch)

Cold open: the shatter. Wren salvages the Rift Gun from her mentor's wrecked workshop. Each zone core recovered = one gyre ring re-lit + one clue. Reveal: the mentor sabotaged the Engine to keep the kingdom from weaponizing it; the Keeper is his unfinished guardian. Ending choice (v1.0 stretch): restore the Engine as it was, or rebuild it *open* — portals for everyone.

## Out of scope for v1.0

2P race mode (original pitch — kept as Future), 45° portals, PAL, seamless scrolling between rooms, MSU-1.

---
*See [ROADMAP.md](./ROADMAP.md) for build order, [ARCHITECTURE.md](./ARCHITECTURE.md) for how it's engineered, [INVARIANTS.md](./INVARIANTS.md) for the rules that keep it honest.*
