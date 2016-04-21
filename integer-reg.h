#ifndef INTEGERREG_H__
#define INTEGERREG_H__

#define LIMB_SIZE 32

struct mpz_t;

/**
 * Converts one single mpz_t number to uint* representation.
 */
void convert1(mpz_t num, uint* repr, size_t sz);

#endif
