#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include <gmp.h>

#include "globals.h"
#include "server.h"

#ifndef BASE
#define BASE 10
#endif

#ifndef DUMPFILE
#define DUMPFILE "dump"
#endif

void server(size_t dbsize, const mpz_t prime, size_t minvp,
		size_t inplen, const mpz_t * const inp,
		size_t outlen, mpz_t *out)
{
	double total_time, time_per_mul, time_per_round, mmps;
	struct timeval st, en;
	size_t i, j;

	(void) minvp;

	gettimeofday(&st, NULL);
	for (i = 0; i < outlen; i++) {
		mpz_init_set_ui(out[i], 1);
		for (j = 0; j < inplen; j++) {
			mpz_mul(out[i], out[i], inp[j]);
			mpz_mod(out[i], out[i], prime);
		}
	}
	gettimeofday(&en, NULL);

	total_time = 1000 * time_diff(&st, &en); /* in ms */
	time_per_mul = total_time / dbsize;
	time_per_round = total_time / outlen;
	mmps = 0.001 / time_per_mul; /* in mmps */
	printf("Total time: %7.3lf ms\n", total_time);
	printf("Time/multp: %7.3lf ms\n", time_per_mul);
	printf("Time/round: %7.3lf ms\n", time_per_round);
	printf("Ops/second: %7.3lf mmps\n", mmps);
}

void dump_results(size_t outlen, const mpz_t * const out)
{
	FILE *f = fopen(DUMPFILE, "w");
	size_t i;

	if (!f) {
		perror("fopen");
		return;
	}

	for (i = 0; i < outlen; i++) {
		mpz_out_str(f, BASE, out[i]);
		fprintf(f, "\n");
	}

	fclose(f);
}
