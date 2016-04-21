#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <gmp.h>

#include "integer-reg.h"

#define CONVERSION_FACTOR 2
#define MASK 0xffffffff

/* assumes mpz uses 64-bit limbs */
typedef unsigned long uint64;

static size_t convert_to_loop(const mp_limb_t *limbs, size_t nsz,
		uint *repr, size_t ix, size_t sz)
{
	size_t i;
	uint64 elm;

	sz /= CONVERSION_FACTOR; /* CONVERSION_FACTOR writes per loop */
	for (i = 0; i < sz; i++) {
		elm = i < nsz ? limbs[i] : 0;
		repr[ix++] = elm & MASK;
		repr[ix++] = elm >> LIMB_SIZE;
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
