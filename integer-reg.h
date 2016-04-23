#ifndef INTEGERREG_H__
#define INTEGERREG_H__

#define LIMB_SIZE 32

struct mpz_t;

/**
 * call setN only once!
 */
void setN(uint newN);
uint getN();

/**
 * Converts one single mpz_t number to uint* representation.
 */
void convert_to_1(mpz_t num, uint* repr, size_t sz);

/**
 * Convert an array of mpz_t numbers into an array of uint*
 */
void convert_to(mpz_t *nums, size_t count, uint *repr, size_t sz);

/**
 * One step in the conversion to Montgomery representation (a*base^N `mod` p).
 * Must call this function 2N times to achieve full representation.
 */
void mul_mont(uint a[], const uint p[]);

/**
 * Returns number 1 in Montgomery representation. Should be faster than
 * calling mul_mon.
 */
uint* one_to_mont(const uint p[]);
#endif
