#include "src/core/rng.h"

static u32 rng_state;

void rng_seed(u32 seed)
{
    if (seed == 0)
        seed = 0x534B5946; /* "SKYF" — the documented default seed */
    rng_state = seed;
}

u32 rng_next(void)
{
    u32 x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}
