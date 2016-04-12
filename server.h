#ifndef SERVER_H__
#define SERVER_H__

struct mpz_t;

void server(const mpz_t prime, size_t minvp,
		size_t inplen, const mpz_t * const inp,
		size_t outlen, mpz_t *out);

#endif
