#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "globals.h"

/**
 * Reads `bytes` bytes from /dev/urandom to initialize random number generator
 * state.
 */
static char *read_from_devurandom(int bytes)
{
	char *buff = malloc(bytes);

	FILE *f = fopen("/dev/urandom", "r");
	char *p = buff;
	size_t read;

	while (bytes) {
		read = fread(p, 1, bytes, f);
		p += read;
		bytes -= read;
	}
	fclose(f);

	return buff;
}

void initialize_random(gmp_randstate_t state, int bytes)
{
	void *buff = read_from_devurandom(bytes);
	mpz_t s;

	gmp_randinit_default(state);
	mpz_init(s);
	mpz_import(s, bytes, 1, 1, 0, 0, buff);
	gmp_randseed(state, s);
	mpz_clear(s);
	free(buff);
}

#define  NANOSECONDS 1000000000.0
double time_diff(const struct timespec *st, const struct timespec *en)
{
	double t1 = st->tv_sec + st->tv_nsec / NANOSECONDS;
	double t2 = en->tv_sec + en->tv_nsec / NANOSECONDS;
	return t2 - t1;
}
#undef MICROSECONDS
