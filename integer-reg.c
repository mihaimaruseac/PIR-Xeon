#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <gmp.h>

#include "integer-reg.h"

#define MASK 0xffffffff

/* assumes mpz uses 64-bit limbs */
typedef unsigned long uint64;

void convert1(mpz_t num, uint* repr, size_t sz)
{
	const mp_limb_t *elems = mpz_limbs_read(num);
	size_t i, nsz, j;
	uint64 elm;

	nsz = mpz_size(num);
	assert(nsz * 2 <= sz);

	for (i = 0, j = 0; j < sz; i++) {
		elm = i < nsz ? elems[i] : 0;
		repr[j++] = elm & MASK;
		repr[j++] = elm >> LIMB_SIZE;
	}

	for (i = 0; i < sz; i++)
		printf("%u\n", repr[i]);
}
