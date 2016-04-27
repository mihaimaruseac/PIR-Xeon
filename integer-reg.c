#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <gmp.h>

#include "integer-reg.h"

#ifdef ALIGN
#include <malloc.h>
#endif

#define CONVERSION_FACTOR 2
#define MASK 0xffffffff
#define LOGBASE LIMB_SIZE
#define HMASK 0xffff
#define HLGBASE (LIMB_SIZE/2)

/* assumes mpz uses 64-bit limbs */
typedef unsigned long uint64;

/* number of limbs in one uint* number */
static uint N;

/* allow setting N only once */
static int set = 0;

inline void setN(uint newN)
{
	assert(!set);
	N = newN;
}

inline uint getN()
{
	return N;
}

/* conversions to/from mpz */

static size_t convert_from_mpz_loop(const mp_limb_t *limbs, size_t nsz,
		uint *repr, size_t ix, size_t sz)
{
	size_t i;
	uint64 elm;

	sz /= CONVERSION_FACTOR; /* CONVERSION_FACTOR writes per loop */
	for (i = 0; i < sz; i++) {
		elm = i < nsz ? limbs[i] : 0;
		repr[ix++] = elm & MASK;
		repr[ix++] = elm >> LOGBASE;
	}

	return ix;
}

void convert_from_mpz_1(mpz_t num, uint* repr, size_t sz)
{
	size_t nsz = mpz_size(num);
	assert(nsz * CONVERSION_FACTOR <= sz);
	nsz = convert_from_mpz_loop(mpz_limbs_read(num), nsz, repr, 0, sz);
}

void convert_from_mpz(mpz_t *nums, size_t count, uint *repr, size_t sz)
{
	size_t nsz = mpz_size(nums[0]), i, ix = 0;
	assert(nsz * CONVERSION_FACTOR * count <= sz);
	sz /= count; /* sz is now size of a single number */
	for (i = 0; i < count; i++)
		ix = convert_from_mpz_loop(mpz_limbs_read(nums[i]), nsz,
				repr, ix, sz);
}

void convert_to_mpz(mpz_t *nums, size_t count, uint *repr, size_t sz)
{
	size_t nsz, i, j, k = 0;

	sz /= count; /* sz is now size of a single number */
	nsz = sz / CONVERSION_FACTOR;

	for (i = 0; i < count; i++) {
		mp_limb_t *p = mpz_limbs_write(nums[i], nsz);
		for (j = 0; j < nsz; j++) {
			size_t a, b;
			a = repr[k++];
			b = repr[k++];
			p[j] = b << LOGBASE | a;
		}
		mpz_limbs_finish(nums[i], nsz);
	}
}

/* arithmetic */

/**
 * a = a + b
 * return carry
 */
static inline uint addin(uint *a, uint b)
{
	*a += b;
	return *a < b;
}

/**
 * a = b + d
 * return carry
 */
static inline uint add(uint *a, uint b, uint d)
{
	*a = b + d;
	return *a < b;
}

/**
 * {h_ab, l_ab} = a * b
 * Uses 64bits to store result then decomposes it to the two terms.
 */
#ifdef RESTRICT
static inline void fullmul(uint a, uint b, uint *restrict l_ab, uint *restrict h_ab)
#else
static inline void fullmul(uint a, uint b, uint *l_ab, uint *h_ab)
#endif
{
	uint64 p = (uint64)a * (uint64)b;
	*l_ab = p & MASK;
	*h_ab = (p >> LOGBASE) & MASK;
}

/**
 * return {ah, al} `div` p
 * Uses 64bits to store operand then truncates result to proper size.
 */
static inline uint onediv(uint al, uint ah, uint p)
{
	uint64 t = al | ((uint64)ah << LOGBASE);
	return ((uint)(t / ((uint64)p)));
}

/**
 * return {carryh, msb_var} `div` msb_p
 * Both inputs should have N full limbs.
 */
#ifdef RESTRICT
static inline uint divq(const uint *restrict var, uint carryh, const uint *restrict p)
#else
static inline uint divq(const uint var[N], uint carryh, const uint p[N])
#endif
{
	uint q, xt, xt1, yt, yt1, cl, ch, cl1, ch1;

	xt = var[N-1];
	xt1 = var[N-2];
	yt = p[N-1];
	yt1 = p[N-2];

	q = onediv(carryh, 0, yt);
	carryh -= q * yt;
	xt -= q * yt1;
	q = onediv(xt, carryh, yt);

	/* {ch1,ch,cl} = q*{yt,yt1} */
	fullmul(q, yt1, &cl, &ch);
	fullmul(q, yt, &cl1, &ch1);
	ch1 = addin(&ch, cl1);

	q -= (ch1 > carryh) || (ch > xt) || (cl > xt1);

	return q;
}

/**
 * Returns a - q * p where q = floor((a<<16)/p)
 * One step in converting to Montgomery representation (a*base^N `mod` p).
 * To achieve full representation, must call this function 2N times.
 */
#ifdef RESTRICT
void convert_to_mont(uint *restrict a, const uint *restrict p)
#else
void convert_to_mont(uint a[N], const uint p[N])
#endif
{
	uint mulh, mull, q, sub, carryh, i;

	carryh = 0;
	/**
	 * Description of the loop (in case there's a vectorization
	 * algorithm):
	 *
	 *  - carryh and vector a of N elements, all 32 bits
	 *  - represented as 16 bits halves
	 *  - 'a', '1-9' = contents that is getting shifted
	 *  - '0' = those bits are 0
	 *
	 *      car.. <--------a---------->
	 *  in: [0.0] [a.1|2.3|4.5|6.7|8.9] (carryh and then a, a has N elements, each 32 bits, represented here in 16bits halves)
	 * out: [0.a] [1.2|3.4|5.6|7.8|9.0] (carryh and then a, a has N elements, each 32 bits, represented here in 16bits halves)
	 */
	/* TODO: carryh prevents vectorization */
	for (i = 0; i < N; i++) {
		mull = (a[i] & HMASK) << HLGBASE;
		mulh = a[i] >> HLGBASE;
		carryh = mulh + add(&a[i], mull, carryh);
	}

	q = divq(a, carryh, p);

	carryh = 0;
	/* TODO: carryh prevents vectorization */
	/**
	 * a = a - qp
	 */
	for (i = 0; i < N; i++) {
		fullmul(q, p[i], &mull, &mulh);
		sub = a[i] - mull - carryh;
		carryh = mulh + (a[i] < sub);
		a[i] = sub;
	}
}

/**
 * Returns (2^LOGBASE)^N - p == Montgomery representation of 1.
 * Assumes p has full bits.
 */
uint* one_to_mont(const uint p[])
{
#ifdef ALIGN
	uint *ret = (uint*)_mm_malloc(N * sizeof(ret[0]), ALIGNBOUNDARY);
#else
	uint *ret = calloc(N, sizeof(ret[0]));
#endif
	uint i;

#ifdef ALIGN
#pragma vector aligned
#endif
	for (i = 0; i < N; i++)
		ret[i] = ~p[i];
	ret[0]++;

	return ret;
}

/**
 * Montgomery multiply op1 and op2 modulo p, keeping result in op2.
 * minvp is used to keep result in Montgomery representation.
 */
#ifdef RESTRICT
void mul_full(uint *restrict op2, const uint *restrict op1, const uint *restrict p, uint minvp)
#else
void mul_full(uint op2[N], const uint op1[N], const uint p[N], uint minvp)
#endif
{

	uint carryl, carryh, ui, uiml, uimh, xiyl, xiyh, i, j, xiyil, xiyih;
#ifdef ALIGN
	uint v[N] __attribute__((aligned(ALIGNBOUNDARY)));
	__assume_aligned(&v[0], ALIGNBOUNDARY);
#else
	uint v[N];
#endif
#ifdef STRRED
	uint a, b;
#endif

#ifdef ALIGN
#pragma vector aligned
#endif
	for (i = 0; i < N; i++)
		v[i] = 0;

	/* TODO: v prevents vectorization */
	for (i = 0; i < N; i++) {
		/** Step 1:
		 *    ui <- (v[0] + op1[i] * op2[0]) * minvp `mod` (2^LOGBASE)
		 */
		fullmul(op1[i], op2[0], &xiyl, &xiyh);
		ui = v[0] + xiyl;
		ui *= minvp;

		/** Step 2:
		 *    v <- (v + op1[i] * op2 + ui * p) / b
		 *
		 * Compute the sum, digit by digit in op2 and p
		 */
		fullmul(ui, p[0], &uiml, &uimh);
		carryh = add(&carryl, xiyl, uiml);
		carryh += addin(&carryl, v[0]);
		carryl = addin(&carryh, xiyh);
		carryl += addin(&carryh, uimh);
#ifdef STRRED
#if 1
		b = 0; /* b = j - 1, a will get to v[j-1] */
		/* TODO: carryl prevents vectorization */
		for (j = 1; j < N; j++) {
			carryh = carryl + add(&a, carryh, v[j]);
			fullmul(op1[i], op2[j], &xiyil, &xiyih);
			fullmul(ui, p[j], &uiml, &uimh);
			carryh += addin(&a, xiyil);
			carryh += addin(&a, uiml);
			carryl = addin(&carryh, xiyih);
			carryl += addin(&carryh, uimh);
			v[b++] = a;
		}
#else
		/* TODO: carryl prevents vectorization */
		for (j = 0; j < N - 1; j++) {
			carryh = carryl + add(&a, carryh, v[j + 1]);
			fullmul(op1[i], op2[j + 1], &xiyil, &xiyih);
			fullmul(ui, p[j + 1], &uiml, &uimh);
			carryh += addin(&a, xiyil);
			carryh += addin(&a, uiml);
			carryl = addin(&carryh, xiyih);
			carryl += addin(&carryh, uimh);
			v[j] = a;
		}
#endif
#else
		/* TODO: v, carryl prevent vectorization */
		for (j = 0; j < N - 1; j++) {
			carryh = carryl + add(&v[j], carryh, v[j + 1]);
			fullmul(op1[i], op2[j + 1], &xiyil, &xiyih);
			fullmul(ui, p[j + 1], &uiml, &uimh);
			carryh += addin(&v[j], xiyil);
			carryh += addin(&v[j], uiml);
			carryl = addin(&carryh, xiyih);
			carryl += addin(&carryh, uimh);
		}
#endif
		v[N-1] = carryh;
	}

	/* TODO: linearize as while (v[i] != p[i]).., remove branch */
	/* compare v with p */
	for (i = N - 1; i > 0 && !carryl; i--)
		if (v[i] < p[i])
			break;
		else if (v[i] > p[i])
			carryl = 1;

	/* if v > p then set v to v - p */
	if (carryl) {
		carryl = 0;
		/* TODO: carryl prevents vectorization */
		for (i = 0; i < N; i++) {
			carryh = v[i] - p[i] - carryl;
			carryl = v[i] < carryh;
			v[i] = carryh;
		}
	}

#ifdef ALIGN
	__assume_aligned(&op2[0], ALIGNBOUNDARY);
#pragma vector aligned
#endif
	/* result in v, copy to op2 */
	/* TODO: Insert a "#pragma loop count min(512)" statement right before the loop to parallelize the loop. */
	for (i = 0; i < N; i++)
		op2[i] = v[i];
}

/**
 * Convert from Montgomery.
 * Should be faster than calling mul_full(op2, 1, prime, minvp).
 */
#ifdef RESTRICT
void convert_from_mont(uint *restrict op2, const uint *restrict p, uint minvp)
#else
void convert_from_mont(uint op2[N], const uint p[N], uint minvp)
#endif
{

	uint carryl, carryh, ui, uiml, uimh, i, j;
#ifdef ALIGN
	uint v[N] __attribute__((aligned(ALIGNBOUNDARY)));
	__assume_aligned(&v[0], ALIGNBOUNDARY);
#else
	uint v[N];
#endif

#ifdef ALIGN
#pragma vector aligned
#endif
	for (i = 0; i < N; i++)
		v[i] = 0;

	/**
	 * LSBpart: op1[i] would be 1
	 */
	ui = op2[0] * minvp;
	fullmul(ui, p[0], &uiml, &uimh);
	carryh = add(&carryl, op2[0], uiml);
	carryl = addin(&carryh, uimh);
	/* TODO: v, carryl prevent vectorization */
	for (j = 0; j < N - 1; j++) {
		carryh = carryl + add(&v[j], carryh, v[j + 1]);
		fullmul(ui, p[j + 1], &uiml, &uimh);
		carryh += addin(&v[j], op2[j + 1]);
		carryh += addin(&v[j], uiml);
		carryl = addin(&carryh, uimh);
	}
	v[N-1] = carryh;

	/**
	 * Remaining part: op1[i] is 0
	 */
	/* TODO: v prevents vectorization */
	for (i = 1; i < N; i++) {
		ui = v[0] * minvp;
		fullmul(ui, p[0], &uiml, &uimh);
		carryh = addin(&uiml, v[0]);
		carryl = addin(&carryh, uimh);
		/* TODO: v, carryl prevent vectorization */
		for (j = 0; j < N - 1; j++) {
			carryh = carryl + add(&v[j], carryh, v[j + 1]);
			fullmul(ui, p[j + 1], &uiml, &uimh);
			carryh += addin(&v[j], uiml);
			carryl = addin(&carryh, uimh);
		}
		v[N-1] = carryh;
	}

	/* compare v with p */
	/* TODO: linearize as while (v[i] != p[i]).., remove branch */
	for (i = N - 1; i > 0 && !carryl; i--)
		if (v[i] < p[i])
			break;
		else if (v[i] > p[i])
			carryl = 1;

	/* if v > p then set v to v - p */
	if (carryl) {
		carryl = 0;
		/* TODO: carryl prevents vectorization */
		for (i = 0; i < N; i++) {
			carryh = v[i] - p[i] - carryl;
			carryl = v[i] < carryh;
			v[i] = carryh;
		}
	}

#ifdef ALIGN
	__assume_aligned(&op2[0], ALIGNBOUNDARY);
#pragma vector aligned
#endif
	/* result in v, copy to op2 */
	/* TODO: Insert a "#pragma loop count min(512)" statement right before the loop to parallelize the loop. */
	for (i = 0; i < N; i++)
		op2[i] = v[i];
}

void display_num(const uint n[])
{
	uint i;

	printf("[%u", n[0]);
	for (i = 1; i < N; i++)
		printf(", %u", n[i]);
	printf("]\n");
}
