#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include <gmp.h>

#include "client.h"

static char* get_query_numbers_filename(size_t keysize)
{
	char *fname = NULL;
	asprintf(&fname, "numberfiles/numbers_%lu_bits", keysize);
	return fname;
}

/**
 * Tries to read query_length numbers from fname, including the prime.
 * Returns -1 if file doesn't exist or is invalid. Returns the number of
 * numbers read, otherwise.
 */
static int try_read(const char* fname, size_t keysize, size_t query_length,
		mpz_t prime, size_t *minvp, mpz_t *numbers)
{
	int read = -1;
	size_t ks, i;
	FILE *f;

	f = fopen(fname, "r");
	if (!f)
		goto end;

	fscanf(f, "%lu", &ks);
	if (ks != keysize) {
		fprintf(stderr, "Invalid keysize in file\n");
		goto end;
	}

	read++;
	mpz_init(prime);
	if (!mpz_inp_str(prime, f, 10))
		goto end;
	fscanf(f, "%lu", minvp);

	for (i = 0; i < query_length; i++, read++) {
		if (!mpz_inp_str(numbers[i], f, 10))
			goto end;
	}

end:
	if (f) fclose(f);
	return read;
}

static void write_back(const char* fname, size_t keysize, size_t query_length,
		mpz_t prime, size_t minvp, const mpz_t *numbers)
{
	size_t i;
	FILE *f;

	f = fopen(fname, "w");
	if (!f)
		return; /* silently leave */

	fprintf(f, "%lu\n", keysize);
	mpz_out_str(f, 10, prime);
	fprintf(f, "\n");
	fprintf(f, "%lu\n", minvp);

	for (i = 0; i < query_length; i++) {
		mpz_out_str(f, 10, numbers[i]);
		fprintf(f, "\n");
	}

	fclose(f);
}

static void generate_prime(size_t keysize, mpz_t prime, size_t *minvp)
{
	mpz_t number, aux;

	mpz_init(prime);
	mpz_ui_pow_ui(prime, 2, keysize - 1);
	mpz_nextprime(prime, prime);

	mpz_init(number);
	mpz_init(aux);
	mpz_ui_pow_ui(aux, 2, 32);
	mpz_cdiv_r(number, prime, aux); /* number = prime mod 2^32 */
	mpz_sub(number, aux, number); /* number = 2^32 - last_limb_of_prime */
	mpz_invert(number, number, aux); /* number * number_old = 1 mod aux */
	*minvp = mpz_get_ui(number);

	mpz_clear(number);
	mpz_clear(aux);
}

static void generate_numbers(size_t num, size_t query_length,
		gmp_randstate_t state, mpz_t prime, mpz_t *numbers)
{
	while (num < query_length)
		mpz_urandomm(numbers[num++], state, prime);
}

void get_client_query(size_t keysize, size_t query_length,
		gmp_randstate_t state, mpz_t prime, size_t *minvp,
		mpz_t *numbers)
{
	char *fname = get_query_numbers_filename(keysize);
	int num, need_write=0;

	num = try_read(fname, keysize, query_length, prime, minvp, numbers);
	if (num < 0) {
		fprintf(stderr, "File invalid/missing\n");
		fprintf(stderr, "Generating new prime..");
		generate_prime(keysize, prime, minvp);
		fprintf(stderr, "OK\n");
		num = 0; /* force regeneration of numbers */
		need_write = 1; /* save everything back to file */
	}

	if ((size_t)num < query_length) {
		fprintf(stderr, "Have %d / %lu numbers\n", num, query_length);
		fprintf(stderr, "Generating missing numbers..");
		generate_numbers(num, query_length, state, prime, numbers);
		fprintf(stderr, "OK\n");
		need_write = 1; /* save everything back to file */
	}

	if (need_write)
		write_back(fname, keysize, query_length,
				prime, *minvp, numbers);

	free(fname);
}
