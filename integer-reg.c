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

static size_t convert_to_loop(const mp_limb_t *limbs, size_t nsz,
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

void convert_to_1(mpz_t num, uint* repr, size_t sz)
{
	size_t nsz = mpz_size(num);
	assert(nsz * CONVERSION_FACTOR <= sz);
	nsz = convert_to_loop(mpz_limbs_read(num), nsz, repr, 0, sz);
}

void convert_to(mpz_t *nums, size_t count, uint *repr, size_t sz)
{
	size_t nsz = mpz_size(nums[0]), i, ix = 0;
	assert(nsz * CONVERSION_FACTOR * count <= sz);
	sz /= count; /* sz is now size of a single number */
	for (i = 0; i < count; i++)
		ix = convert_to_loop(mpz_limbs_read(nums[i]),nsz, repr,ix,sz);
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
	xt1 -= q * yt1;
	q = onediv(xt, carryh, yt);

	/* {ch1,ch,cl} = q*{yt,yt1} */
	fullmul(q, yt1, &cl, &ch);
	fullmul(q, yt, &cl1, &ch1);
	ch1 = addin(&ch, cl1);

	q -= (ch1 > carryh) || (ch > xt) || (cl > xt1);

	return q;
}

/**
 * Full multiplication.
 * op2 = op1 * op2 `mod` p
 * Use minvp and Montgomerry multiplication.
 * TODO: describe, check, clear
 */
void mul_full(uint op2[N], const uint op1[N], const uint p[N], uint minvp)
{

	uint carryl, carryh, ui, uiml, uimh, xiyl, xiyh, i, j, xiyil, xiyih;
	uint var1[N];

	for (i = 0; i < N; i++)
		var1[i] = 0;

	for (i = 0; i < N; i++) {
		fullmul(op1[i], op2[0], &xiyl, &xiyh);

		// computes: ui = (a0+xi*yo)*invp mod 2^32;
		ui = var1[0] + xiyl;
		ui *= minvp;

		fullmul(ui, p[0], &uiml, &uimh);

		// computes: carryl = (var0 + xi*y0 + ui* prime0) / 2^32
		// computes: carryh = (xi*y0)/ 2^32 + (ui* prime0)/ 2^32  + carryl
		carryh = add(&carryl, xiyl, uiml);
		carryh += addin(&carryl, var1[0]);
		carryl = addin(&carryh, xiyh);
		carryl += addin(&carryh, uimh);

		for (j = 0; j < N - 1; j++) {
			carryh = carryl + add(&var1[j], carryh, var1[j + 1]);
			fullmul(op1[i], op2[j + 1], &xiyil, &xiyih);
			fullmul(ui, p[j + 1], &uiml, &uimh);
			carryh += addin(&var1[j], xiyil);
			carryh += addin(&var1[j], uiml);
			carryl = addin(&carryh, xiyih);
			carryl += addin(&carryh, uimh);
		}

		var1[N-1] = carryh;
	}

	for (i = N - 1; i > 0 && !carryl; i--)
		if (var1[i] < p[i])
			break;
		else if (var1[i] > p[i])
			carryl = 1;

	if (carryl) {
		carryl = 0;
		for (i = 0; i < N; i++) {
			carryh = var1[i] - p[i] - carryl;
			carryl = var1[i] < carryh;
			var1[i] = carryh;
		}
	}

	for (i = 0; i < N; i++)
		op2[i] = var1[i];
}

/**
 * Returns a - q * p where q = floor((a<<16)/p)
 * One step in converting to Montgomery representation (a*base^N `mod` p).
 * To achieve full representation, must call this function 2N times.
 */
void mul_mont(uint a[N], const uint p[N])
{
	uint mulh, mull, q, sub, carryh;

	carryh = 0;
	for (uint i = 0; i < N; i++) {
		fullmul(65536, a[i], &mull, &mulh); // TODO: specialize?
		carryh = add(&a[i], mull, carryh);
		carryh += mulh;
	}

	q = divq(a, carryh, p);

	carryh = 0;
	for (uint i = 0; i < N; i++) {
		fullmul(q, p[i], &mull, &mulh);
		sub = a[i] - mull - carryh;
		carryh = a[i] < sub;
		carryh += mulh;
		a[i] = sub;
	}
}
