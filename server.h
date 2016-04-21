#ifndef SERVER_H__
#define SERVER_H__

struct mpz_t;

#ifdef CU_CODE
#define number_t uint*
#else
#define number_t mpz_t
#endif

void server(size_t dbsize, const number_t prime, size_t minvp,
		size_t inplen, const number_t * const inp,
		size_t outlen, number_t *out);

void dump_results(size_t outlen, const mpz_t * const out);

#endif
