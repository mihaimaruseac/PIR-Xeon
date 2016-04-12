#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include <gmp.h>

#include "server.h"

#ifndef BASE
#define BASE 10
#endif

#ifndef DUMPFILE
#define DUMPFILE "dump"
#endif

void server(const mpz_t prime, size_t minvp,
		size_t inplen, const mpz_t * const inp,
		size_t outlen, mpz_t *out)
{
	size_t i, j;

	(void) minvp;

	for (i = 0; i < outlen; i++) {
		mpz_init_set_ui(out[i], 1);
		for (j = 0; j < inplen; j++) {
			mpz_mul(out[i], out[i], inp[j]);
			mpz_mod(out[i], out[i], prime);
		}
	}
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
