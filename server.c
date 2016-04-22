#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <gmp.h>

#ifdef HAVEOMP
#include <omp.h>
#endif

#include "globals.h"
#include "server.h"

#ifdef CU_CODE
#include "integer-reg.h"
#endif

#ifndef BASE
#define BASE 10
#endif

#ifndef DUMPFILE
#define DUMPFILE "dump"
#endif

#ifdef CU_CODE
/**
 * Converts each input number in inp to Montgomery representation, once.
 */
static void montgomerry(uint *inp, size_t inplen, size_t sz, uint *prime)
{
	size_t i, j, mj;
	uint *p = inp;

	mj = 2 * getN();
	for (i = 0; i < inplen; i++) {
		for (j = 0; j < mj; j++)
			mul_mont(p, prime);
	}
}

#if 0
__global__ void multiply(const __restrict__ uint* Tinput, __restrict__ uint* Toutput)
{
	uint a[N];
	uint b[N];
	uint var1[N];
	uint i;

#pragma unroll
	for (i = 0; i < N; i++)
		b[i] = var1[i] = 0;
	b[0] = var1[i] = 1;

#pragma unroll
	for (i = 0; i < 2*N; i++)
		mulMont(b, constPrime);

#pragma unroll
	for (i = tid(); i < constSize; i += gridDim.x * blockDim.x) {
		globalLoad(a, Tinput, tid(), constSize);
		mulFull(b, a, constPrime, constMinvp);
	}

	mulFull(b, var1, constPrime, constMinvp);

	globalStore(Toutput, b, tid(), gridDim.x * blockDim.x);
}
#endif
#else
#ifdef LLIMPL
static void low_level_work_kernel(const mp_limb_t *prime, mp_size_t numlen,
		size_t minvp, size_t inplen, const mp_limb_t * const * inp,
		size_t outlen, mp_limb_t **out)
{
	size_t i, j, sz = 2 * numlen;
	mp_limb_t* scratch = calloc(sz, sizeof(scratch[0]));
	mp_limb_t* quot = calloc(sz, sizeof(scratch[0]));

	(void) minvp;
	for (i = 0; i < outlen; i++) {
		for (j = 0; j < inplen; j++) {
			mpn_mul_n(scratch, out[i], inp[j], numlen);
			mpn_tdiv_qr(quot, out[i], 0, scratch, sz, prime, numlen);
		}
	}

	free(scratch);
	free(quot);
}

static void low_level_impl(const mpz_t prime, size_t minvp,
		size_t inplen, const mpz_t * const inp,
		size_t outlen, mpz_t *out)
{
	size_t i, sz = mpz_size(prime);

	const mp_limb_t** inputs = calloc(inplen, sizeof(inputs[0]));
	for (i = 0; i< inplen; i++)
		inputs[i] = mpz_limbs_read(inp[i]);

	mp_limb_t** outputs = calloc(outlen, sizeof(outputs[0]));
	for (i = 0; i < outlen; i++)
		outputs[i] = mpz_limbs_modify(out[i], sz);

	low_level_work_kernel(mpz_limbs_read(prime), sz,
			minvp, inplen, inputs,
			outlen, outputs);

	for (i = 0; i < outlen; i++)
		mpz_limbs_finish(out[i], sz);

	free(outputs);
	free(inputs);
}

#else

static void naive_impl(const mpz_t prime, size_t minvp,
		size_t inplen, const mpz_t * const inp,
		size_t outlen, mpz_t *out)
{
	size_t i, j;

	(void) minvp;
#ifdef HAVEOMP
#pragma omp parallel for
#endif
	for (i = 0; i < outlen; i++) {
		mpz_init_set_ui(out[i], 1);
		for (j = 0; j < inplen; j++) {
			mpz_mul(out[i], out[i], inp[j]);
			mpz_mod(out[i], out[i], prime);
		}
	}
}

#endif
#endif

#ifdef CU_CODE
void server(size_t dbsize, const uint *prime, size_t minvp,
		size_t inplen, uint *inp,
		size_t outlen, uint *out)
#else
void server(size_t dbsize, const mpz_t prime, size_t minvp,
		size_t inplen, const mpz_t * const inp,
		size_t outlen, mpz_t *out)
#endif
{
	double total_time, time_per_mul, time_per_round, mmps;
	struct timeval st, en;
#if CU_CODE
	size_t sz = getN();
#endif

	gettimeofday(&st, NULL);

#if CU_CODE
	montgomerry(inp, inplen, sz, prime);

	// TODO
	(void)out;

	(void)outlen;
	(void)minvp;
	uint a[] = {1,2,3,4}; /* 316912650112397582603894390785 */
	uint b[] = {5,6,7,8}; /* 633825300243241909290088267781 */
#else
#ifdef LLIMPL
	low_level_impl(prime, minvp, inplen, inp, outlen, out);
#else
	naive_impl(prime, minvp, inplen, inp, outlen, out);
#endif
#endif

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
