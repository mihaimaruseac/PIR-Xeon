#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gmp.h>

#include "client.h"
#include "globals.h"
#include "server.h"

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
}

int main(int argc, char **argv)
{
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
	if (!numbers) {
		fprintf(stderr, "Cannot allocate memory for server results!\n");
		exit(EXIT_FAILURE);
	}

	initialize_random(state, args.db_size);
	get_client_query((size_t)args.keysize, (size_t)args.query_length,
			state, prime, &minvp, numbers);
	server(prime, minvp, args.query_length, numbers, num_outputs, results);

	for (i = 0; i < args.query_length; i++)
		mpz_clear(numbers[i]);

	for (j = 0; j < num_outputs; j++)
		mpz_clear(results[j]);

	gmp_randclear(state);
	mpz_clear(prime);
	free(numbers);
	free(results);

	exit(EXIT_SUCCESS);
}
