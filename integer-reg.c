#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <gmp.h>

#include "integer-reg.h"

#define CONVERSION_FACTOR 2
#define MASK 0xffffffff
#define LOGBASE LIMB_SIZE

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
static inline void fullmul(uint a, uint b, uint *l_ab, uint *h_ab)
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
static inline uint divq(const uint var[N], uint carryh, const uint p[N])
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
void convert_to_mont(uint a[N], const uint p[N])
{
	uint mulh, mull, q, sub, carryh, i;

	carryh = 0;
	for (i = 0; i < N; i++) {
		fullmul(65536, a[i], &mull, &mulh); // TODO: specialize?
		carryh = add(&a[i], mull, carryh);
		carryh += mulh;
	}

	q = divq(a, carryh, p);

	carryh = 0;
	for (i = 0; i < N; i++) {
		fullmul(q, p[i], &mull, &mulh);
		sub = a[i] - mull - carryh;
		carryh = a[i] < sub;
		carryh += mulh;
		a[i] = sub;
	}
}

/**
 * Returns (2^LOGBASE)^N - p == Montgomery representation of 1.
 * Assumes p has full bits.
 */
uint* one_to_mont(const uint p[])
{
	uint *ret = calloc(N, sizeof(ret[0]));
	uint i;

	for (i = 0; i < N; i++)
		ret[i] = ~p[i];
	ret[0]++;

	return ret;
}

/**
 * Montgomery multiply op1 and op2 modulo p, keeping result in op2.
 * minvp is used to keep result in Montgomery representation.
 */
void mul_full(uint op2[N], const uint op1[N], const uint p[N], uint minvp)
{

	uint carryl, carryh, ui, uiml, uimh, xiyl, xiyh, i, j, xiyil, xiyih;
	uint v[N];

	for (i = 0; i < N; i++)
		v[i] = 0;

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
		for (j = 0; j < N - 1; j++) {
			carryh = carryl + add(&v[j], carryh, v[j + 1]);
			fullmul(op1[i], op2[j + 1], &xiyil, &xiyih);
			fullmul(ui, p[j + 1], &uiml, &uimh);
			carryh += addin(&v[j], xiyil);
			carryh += addin(&v[j], uiml);
			carryl = addin(&carryh, xiyih);
			carryl += addin(&carryh, uimh);
		}
		v[N-1] = carryh;
	}

	/* compare v with p */
	for (i = N - 1; i > 0 && !carryl; i--)
		if (v[i] < p[i])
			break;
		else if (v[i] > p[i])
			carryl = 1;

	/* if v > p then set v to v - p */
	if (carryl) {
		carryl = 0;
		for (i = 0; i < N; i++) {
			carryh = v[i] - p[i] - carryl;
			carryl = v[i] < carryh;
			v[i] = carryh;
		}
	}

	/* result in v, copy to op2 */
	for (i = 0; i < N; i++)
		op2[i] = v[i];
}

/**
 * Convert from Montgomery.
 * Should be faster than calling mul_full(op2, 1, prime, minvp).
 */
void convert_from_mont(uint op2[N], const uint p[N], uint minvp)
{

	uint carryl, carryh, ui, uiml, uimh, i, j;
	uint v[N];

	for (i = 0; i < N; i++)
		v[i] = 0;

	/**
	 * LSBpart: op1[i] would be 1
	 */
	/* Step 1 */
	ui = v[0] + op2[0];
	ui *= minvp;

	/* Step 2 */
	fullmul(ui, p[0], &uiml, &uimh);
	carryh = add(&carryl, op2[0], uiml);
	carryh += addin(&carryl, v[0]);
	carryl = addin(&carryh, uimh);
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
	 * Strength reduction for both steps.
	 */
	ui = v[0] * minvp;
	fullmul(ui, p[0], &uiml, &uimh);
	carryh = addin(&uiml, v[0]);
	carryl = addin(&carryh, uimh);
	for (i = 1; i < N; i++) {
		for (j = 0; j < N - 1; j++) {
			carryh = carryl + add(&v[j], carryh, v[j + 1]);
			fullmul(ui, p[j + 1], &uiml, &uimh);
			carryh += addin(&v[j], uiml);
			carryl = addin(&carryh, uimh);
		}
		v[N-1] = carryh;
	}

	/* compare v with p */
	for (i = N - 1; i > 0 && !carryl; i--)
		if (v[i] < p[i])
			break;
		else if (v[i] > p[i])
			carryl = 1;

	/* if v > p then set v to v - p */
	if (carryl) {
		carryl = 0;
		for (i = 0; i < N; i++) {
			carryh = v[i] - p[i] - carryl;
			carryl = v[i] < carryh;
			v[i] = carryh;
		}
	}

	/* result in v, copy to op2 */
	for (i = 0; i < N; i++)
		op2[i] = v[i];
}

void display_num(const uint n[])
{
	printf("[%u", n[0]);
	for (uint i = 1; i < N; i++)
		printf(", %u", n[i]);
	printf("]\n");
}