#ifndef CLIENT_H__
#define CLIENT_H__

struct mpz_t;

void get_client_query(size_t keysize, size_t query_length,
		gmp_randstate_t state, mpz_t prime, size_t *minvp,
		mpz_t *numbers);

#endif
