#ifndef GLOBALS_H__
#define GLOBALS_H__

struct gmp_randstate_t;

/**
 * Initializes the random number generator of GMP library. To be called in
 * every `main` function.
 */
void initialize_random(gmp_randstate_t state, int bytes);

#endif
