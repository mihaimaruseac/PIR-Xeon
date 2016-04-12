#ifndef SERVER_H__
#define SERVER_H__

struct mpz_t;

void server(size_t dbsize, const mpz_t prime, size_t minvp,
		size_t inplen, const mpz_t * const inp,
		size_t outlen, mpz_t *out);

void dump_results(size_t outlen, const mpz_t * const out);

#endif
