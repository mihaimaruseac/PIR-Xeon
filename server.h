#ifndef SERVER_H__
#define SERVER_H__

struct mpz_t;

#ifdef CU_CODE
void server(size_t dbsize, const uint *prime, size_t minvp,
		size_t inplen, uint *inp,
		size_t outlen, uint *out);
#else
void server(size_t dbsize, const mpz_t prime, size_t minvp,
		size_t inplen, const mpz_t * const inp,
		size_t outlen, mpz_t *out);
#endif

void dump_results(size_t outlen, const mpz_t * const out);

#endif
