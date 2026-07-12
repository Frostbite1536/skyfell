/* Seeded xorshift32 — the game's only randomness source (INV-ENG-002:
 * world state = f(inputs, seed, frame) and nothing else; the replay test
 * depends on it). */
#ifndef SKYFELL_CORE_RNG_H
#define SKYFELL_CORE_RNG_H

#include <snes.h>

void rng_seed(u32 seed); /* 0 is remapped: xorshift state must be nonzero */
u32 rng_next(void);

#endif
