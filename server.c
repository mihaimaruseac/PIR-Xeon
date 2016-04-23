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
static void montgomerry(uint *inp, size_t inplen, const uint *prime)
{
	const size_t N = getN();
	const size_t mj = 2 * N;
	size_t i, j;

	for (i = 0; i < inplen; i++) {
		uint *p = &inp[N * i];
		for (j = 0; j < mj; j++)
			mul_mont(p, prime);
	}
}

static void multiply(uint *inp, size_t inplen,
		uint *out, size_t outlen,
		const uint *prime, size_t minvp)
{
	uint *m1 = one_to_mont(prime);
	const size_t N = getN();
	size_t i, j;

	printf("Computed once: [%u, %u]\n", m1[0], m1[1]);

	for (i = 0; i < outlen; i++) {
		uint *p = &out[N * i];
		/* set accumulator/out to Montgomery representation of 1 */
		for (j = 0; j < N; j++)
			p[j] = m1[j];

		/* multiply into out */
		for (j = 0; j < inplen; j++) {
			uint *q = &inp[N * j];
			printf("to multiply: [%u, %u]\n", q[0], q[1]);
			mul_full(p, q, prime, minvp);
			printf("now: [%u, %u]\n", p[0], p[1]);
		}

		/* convert out back from Montgomery */
		printf("final result: [%u, %u]\n", p[0], p[1]);
		// TODO
	}

	free(m1);
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

	gettimeofday(&st, NULL);

#if CU_CODE
	montgomerry(inp, inplen, prime);
	multiply(inp, inplen, out, outlen, prime, minvp);
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
