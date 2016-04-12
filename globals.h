#ifndef GLOBALS_H__
#define GLOBALS_H__

struct gmp_randstate_t;
struct timeval;

/**
 * Initializes the random number generator of GMP library. To be called in
 * every `main` function.
 */
void initialize_random(gmp_randstate_t state, int bytes);

double time_diff(const struct timeval *st, const struct timeval *en);

#endif
