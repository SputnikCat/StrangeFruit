/*
Prime engine. Works one small operation at a time so the screen can show
the arithmetic as it happens, slowly at first and then as a blur.

  TRIAL DIVISION  brute force: every n is divided by 2, 3, 5, 7 ... up to
                  floor(sqrt(n)). No divisor means n is prime.
  SIEVE           Eratosthenes over a fixed range: for each prime p, cross
                  out p*p, p*p+p, ... Once p*p passes the limit, whatever
                  survives is prime. Everything below p*p is confirmed as
                  soon as p comes up, so primes arrive in bursts.

One operation is one division (trial) or one cross-out / one survivor
check (sieve).
*/

#pragma once

#include <stdbool.h>

typedef enum PrimeMode
{
	MODE_TRIAL,
	MODE_SIEVE,
	MODE_COUNT
} PrimeMode;

enum
{
	CELL_UNKNOWN,		// not decided yet (sieve: still a candidate)
	CELL_PRIME,
	CELL_COMPOSITE,
	CELL_UNIT			// 1: neither prime nor composite
};

#define LOG_LINES	64
#define LOG_LEN		80

typedef struct PrimeEngine
{
	PrimeMode mode;
	int limit;				// highest n in this run
	unsigned char *cell;	// [limit + 1]
	float *flash;			// [limit + 1], 1 when something just happened to n, decays outside
	bool done;
	int cursor;				// the number the beam is on

	// trial division
	int n, bound, divisor;	// number under test, floor(sqrt(n)), next divisor to try
	bool testing;
	int lastDivisor, lastRemainder;	// the division just done
	int factor;				// smallest factor once n is found composite

	// sieve
	int p, m;				// sieving prime, next multiple to cross out
	int confirmed;			// every n below this is decided
	bool sweeping;			// p*p is past the limit: collecting survivors
	int sweep;				// next survivor to check

	// results
	int primeCount, largestPrime;
	int maxGap, maxGapAt;	// widest gap so far and the prime that closed it
	int twinCount;			// pairs (p, p + 2)
	long long ops;

	// what happened, newest last
	char log[LOG_LINES][LOG_LEN];
	int logHead, logCount;
	bool verbose;			// trial: log composites too (only sensible when slow)
} PrimeEngine;

void PrimesStart(PrimeEngine *e, PrimeMode mode, int limit);
void PrimesFree(PrimeEngine *e);
void PrimesStep(PrimeEngine *e, int ops);
int PrimesConfirmedTo(const PrimeEngine *e);		// every n up to this is decided
const char *PrimesLogLine(const PrimeEngine *e, int back);	// 0 = newest, NULL past the end
const char *PrimesModeName(PrimeMode mode);
