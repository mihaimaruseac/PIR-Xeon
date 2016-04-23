#ifndef INTEGERREG_H__
#define INTEGERREG_H__

#define LIMB_SIZE 32

#ifdef DEBUG_IREG
#define debug_IR(msg, n)\
	do{\
		printf(msg);\
		display_num(n);\
	}while(0)
#else
#define debug_IR(msg, n)
#endif

struct mpz_t;

/**
 * call setN only once!
 */
void setN(uint newN);
uint getN();

/**
 * Converts one single mpz_t number to uint* representation.
 */
void convert_from_mpz_1(mpz_t num, uint* repr, size_t sz);

/**
 * Convert an array of mpz_t numbers into an array of uint*
 *
 * 	count:		number of mpz_t numbers
 * 	sz:		total size of repr array
 */
void convert_from_mpz(mpz_t *nums, size_t count, uint *repr, size_t sz);

/**
 * Convert to an array of mpz_t numbers from an array of uint*
 *
 * 	count:		number of mpz_t numbers
 * 	sz:		total size of repr array
 */
void convert_to_mpz(mpz_t *nums, size_t count, uint *repr, size_t sz);

/**
 * One step in the conversion to Montgomery representation (a*base^N `mod` p).
 * Must call this function 2N times to achieve full representation.
 */
void convert_to_mont(uint a[], const uint p[]);

/**
 * Returns number 1 in Montgomery representation.
 * Should be faster than calling mul_mon(1, p).
 */
uint* one_to_mont(const uint p[]);

/**
 * Montgomery multiply op1 and op2 modulo p, keeping result in op2.
 * minvp is used to keep result in Montgomery representation.
 */
void mul_full(uint op2[], const uint op1[], const uint p[], uint minvp);

/**
 * Convert from Montgomery.
 * Should be faster than calling mul_full(op2, 1, prime, minvp).
 */
void convert_from_mont(uint op2[], const uint p[], uint minvp);

/**
 * Debug purposes only.
 */
void display_num(const uint n[]);
#endif
