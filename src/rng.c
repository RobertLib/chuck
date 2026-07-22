#include "rng.h"

#include <assert.h>

void rng_seed(Rng *rng, uint64_t seed)
{
    /* xorshift64* cannot advance from an all-zero state. */
    rng->state = seed == 0 ? UINT64_C(0x9e3779b97f4a7c15) : seed;
}

uint32_t rng_next(Rng *rng)
{
    uint64_t value = rng->state;
    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    rng->state = value;
    return (uint32_t)((value * UINT64_C(0x2545f4914f6cdd1d)) >> 32);
}

int rng_range(Rng *rng, int upper_bound)
{
    assert(upper_bound > 0);
    uint32_t bound = (uint32_t)upper_bound;
    uint32_t threshold = (uint32_t)(-bound) % bound;
    uint32_t value;
    do
    {
        value = rng_next(rng);
    } while (value < threshold);
    return (int)(value % bound);
}

float rng_unit(Rng *rng)
{
    return (float)(rng_next(rng) >> 8) * (1.0f / 16777216.0f);
}
