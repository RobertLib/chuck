#ifndef CHUCK_RNG_H
#define CHUCK_RNG_H

#include <stdint.h>

typedef struct
{
    uint64_t state;
} Rng;

void rng_seed(Rng *rng, uint64_t seed);
uint32_t rng_next(Rng *rng);
int rng_range(Rng *rng, int upper_bound);
float rng_unit(Rng *rng);

#endif /* CHUCK_RNG_H */
