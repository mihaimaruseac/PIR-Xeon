#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gmp.h>

#include "client.h"
#include "globals.h"
#include "server.h"

#ifdef CU_CODE
#include "integer-reg.h"
#endif

#ifndef DEBUG_RESULTS
#define DEBUG_RESULTS 0
#endif

/* default key size: 1024 bits */
#define KEYDEFAULT 1024

/* options as string */
#define OPTSTR "n:k:m:"

/* Command line arguments */
static struct {
	/* database size (n) */
	int db_size;
	/* number of operands in query (k) */
	int query_length;
	/* keysize, defaut KEYDEFAULT */
	int keysize;
} args;

static void usage(const char *prg)
{
	fprintf(stderr, "Usage: %s [OPTIONS]\n", prg);
	fprintf(stderr, "\n");
	fprintf(stderr, "OPTIONS:\n");
	fprintf(stderr, "\t-n sz\tsize of the database (in bits)\n");
	fprintf(stderr, "\t-k ops\tnumber of operands in query from user\n");
	fprintf(stderr, "\t-m keysize (default %d\n", KEYDEFAULT);
	fprintf(stderr, "\n");
	fprintf(stderr, "CONSTRAINTS:\n");
	fprintf(stderr, "\tsz is a multiple of ops\n");
	exit(EXIT_FAILURE);
}

static void parse_arguments(int argc, char **argv)
{
	char extra;
	int opt, i;

	printf("Called with: argc=%d\n", argc);
	for (i = 0; i < argc; i++)
		printf("%s ", argv[i]);
	printf("\n");

	args.keysize = KEYDEFAULT;
	args.db_size = -1;
	args.query_length = -1;

	while((opt = getopt(argc, argv, OPTSTR)) != -1)
		switch(opt) {
		case 'n':
			if (sscanf(optarg, "%d%c", &args.db_size, &extra) != 1)
				usage(argv[0]);
			break;
		case 'k':
			if (sscanf(optarg, "%d%c", &args.query_length, &extra) != 1)
				usage(argv[0]);
			break;
		case 'm':
			if (sscanf(optarg, "%d%c", &args.keysize, &extra) != 1)
				usage(argv[0]);
			break;
		default: usage(argv[0]);
		}

	/* consistency checks */
	if (optind != argc)
		usage(argv[0]); /* extra arguments */

	if (args.db_size < 0 || args.query_length < 0) {
		fprintf(stderr, "Missing/Invalid -n or -k value\n");
		usage(argv[0]);
	}

	if (args.db_size % args.query_length != 0) {
		fprintf(stderr, "Database size is not multiple of ops\n");
		usage(argv[0]);
	}
}

int main(int argc, char **argv)
{
#ifdef CU_CODE
	//TODO: vars for uint* representation
	uint *_prime, *_inp, *_out;
	uint sz, isz, osz;
#endif

	mpz_t prime, *numbers, *results;
	size_t minvp, num_outputs, j;
	gmp_randstate_t state;
	int i;

	parse_arguments(argc, argv);

	numbers = calloc(args.query_length, sizeof(numbers[0]));
	if (!numbers) {
		fprintf(stderr, "Cannot allocate memory for client numbers!\n");
		exit(EXIT_FAILURE);
	}

	num_outputs = args.db_size / args.query_length;
	results = calloc(num_outputs, sizeof(results[0]));
	if (!results) {
		fprintf(stderr, "Cannot allocate memory for server results!\n");
		exit(EXIT_FAILURE);
	}

	initialize_random(state, args.db_size);
	get_client_query((size_t)args.keysize, (size_t)args.query_length,
			state, prime, &minvp, numbers);
	printf("%d %lu %d\n", mp_bits_per_limb, mpz_size(prime), args.keysize / mp_bits_per_limb);
	printf("%lu %lu %lu %lu\n", sizeof(int), sizeof(long), sizeof(long long), sizeof(void*));

#ifdef CU_CODE
	sz = args.keysize / LIMB_SIZE;
	_prime = calloc(sz, sizeof(_prime[0]));
	convert_from_mpz_1(prime, _prime, sz);

	isz = sz * args.query_length;
	_inp = calloc(isz, sizeof(_inp[0]));
	convert_from_mpz(numbers, args.query_length, _inp, isz);

	osz = sz * num_outputs;
	_out = calloc(osz, sizeof(_out[0]));

	setN(sz);
	printf("Numbers have %u limbs\n", getN());
	server(args.db_size, _prime, minvp,
			args.query_length, _inp,
			num_outputs, _out);
#else
	server(args.db_size, prime, minvp, args.query_length,
			(const mpz_t *)numbers, num_outputs, results);
#endif

#if DEBUG_RESULTS
#ifdef CU_CODE
	printf("Result: "); display_num(_out);
	convert_to_mpz(results, num_outputs, _out, osz);
#endif
	dump_results(num_outputs, (const mpz_t *)results);
#endif

	for (i = 0; i < args.query_length; i++)
		mpz_clear(numbers[i]);

	for (j = 0; j < num_outputs; j++)
		mpz_clear(results[j]);

	gmp_randclear(state);
	mpz_clear(prime);
	free(numbers);
	free(results);

#ifdef CU_CODE
	free(_prime);
	free(_inp);
	free(_out);
#endif

	exit(EXIT_SUCCESS);
}
