#ifndef GLOBALS_H__
#define GLOBALS_H__

struct gmp_randstate_t;
struct timespec;

/**
 * Initializes the random number generator of GMP library. To be called in
 * every `main` function.
 */
void initialize_random(gmp_randstate_t state, int bytes);

double time_diff(const struct timespec *st, const struct timespec *en);

#endif
