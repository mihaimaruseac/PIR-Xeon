#ifndef INTEGERREG_H__
#define INTEGERREG_H__

#define LIMB_SIZE 32

struct mpz_t;

/**
 * Converts one single mpz_t number to uint* representation.
 */
void convert_to_1(mpz_t num, uint* repr, size_t sz);

/**
 * Convert an array of mpz_t numbers into an array of uint*
 */
void convert_to(mpz_t *nums, size_t count, uint *repr, size_t sz);

#endif
