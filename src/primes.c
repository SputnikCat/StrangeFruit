#include "primes.h"
#include "vecfont.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ISqrt(int n)
{
	int r = (int)sqrt((double)n);
	while (r*r > n) r--;
	while ((r + 1)*(r + 1) <= n) r++;
	return r;
}

static void Log(PrimeEngine *e, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(e->log[e->logHead], LOG_LEN, fmt, args);
	va_end(args);
	e->logHead = (e->logHead + 1)%LOG_LINES;
	if (e->logCount < LOG_LINES) e->logCount++;
}

const char *PrimesLogLine(const PrimeEngine *e, int back)
{
	if (back < 0 || back >= e->logCount) return NULL;
	return e->log[(e->logHead - 1 - back + LOG_LINES)%LOG_LINES];
}

const char *PrimesModeName(PrimeMode mode)
{
	return (mode == MODE_SIEVE)? "SIEVE OF ERATOSTHENES" : "TRIAL DIVISION";
}

// n is prime: light it and keep the running statistics
static void Found(PrimeEngine *e, int n)
{
	e->cell[n] = CELL_PRIME;
	e->flash[n] = 1.0f;
	e->primeCount++;
	if (e->largestPrime > 0)
	{
		int gap = n - e->largestPrime;
		if (gap > e->maxGap) { e->maxGap = gap; e->maxGapAt = n; }
		if (gap == 2) e->twinCount++;
	}
	e->largestPrime = n;
}

void PrimesFree(PrimeEngine *e)
{
	free(e->cell);
	free(e->flash);
	e->cell = NULL;
	e->flash = NULL;
}

// sieve: p has come up, so every candidate below p*p is prime
static void ConfirmBelow(PrimeEngine *e, int x)
{
	int found = 0;
	for (int n = e->confirmed; n < x && n <= e->limit; n++)
		if (e->cell[n] == CELL_UNKNOWN) { Found(e, n); found++; }
	if (found > 0) Log(e, "%d NEW PRIMES CONFIRMED BELOW %d", found, x);
	if (x > e->confirmed) e->confirmed = x;
}

static void StartSieving(PrimeEngine *e, int p)
{
	e->p = p;
	e->m = p*p;
	ConfirmBelow(e, p*p);
	Log(e, "P = %d : CROSS OUT %d" VF_X "K FROM %d", p, p, p*p);
}

void PrimesStart(PrimeEngine *e, PrimeMode mode, int limit)
{
	PrimesFree(e);
	memset(e, 0, sizeof(*e));
	e->mode = mode;
	e->limit = limit;
	e->cell = calloc(limit + 1, 1);
	e->flash = calloc(limit + 1, sizeof(float));

	if (mode == MODE_TRIAL)
	{
		Log(e, "TRIAL DIVISION  N = 1 .. %d", limit);
		Log(e, "N IS PRIME IF NO D " VF_LE " " VF_SQRT "N DIVIDES IT");
	}
	else
	{
		e->cell[1] = CELL_UNIT;
		e->confirmed = 2;
		e->cursor = 1;
		Log(e, "SIEVE OF ERATOSTHENES  LIMIT %d", limit);
		Log(e, "ALL N " VF_GE " 2 START AS CANDIDATES");
		StartSieving(e, 2);
	}
}

int PrimesConfirmedTo(const PrimeEngine *e)
{
	if (e->mode == MODE_TRIAL) return e->testing? e->n - 1 : e->n;
	if (e->sweeping) return e->sweep - 1;
	return e->confirmed - 1;
}

static void TrialOp(PrimeEngine *e)
{
	if (!e->testing)
	{
		if (e->n >= e->limit)
		{
			e->done = true;
			Log(e, "SCAN COMPLETE : " VF_PI "(%d) = %d", e->limit, e->primeCount);
			return;
		}
		e->n++;
		e->cursor = e->n;
		if (e->n == 1)
		{
			e->cell[1] = CELL_UNIT;
			e->flash[1] = 1.0f;
			Log(e, "1 IS A UNIT : NEITHER PRIME NOR COMPOSITE");
			return;
		}
		e->bound = ISqrt(e->n);
		e->divisor = 2;
		e->testing = true;
		e->lastDivisor = 0;
	}

	int n = e->n;
	if (e->divisor > e->bound)
	{
		Found(e, n);
		e->testing = false;
		if (e->bound < 2) Log(e, "%d : NOTHING TO TRY  =>  PRIME #%d", n, e->primeCount);
		else Log(e, "%d : NO DIVISOR " VF_LE " %d  =>  PRIME #%d", n, e->bound, e->primeCount);
		return;
	}

	int r = n%e->divisor;
	e->lastDivisor = e->divisor;
	e->lastRemainder = r;
	if (r == 0)
	{
		e->cell[n] = CELL_COMPOSITE;
		e->flash[n] = 0.7f;
		e->factor = e->divisor;
		e->testing = false;
		if (e->verbose) Log(e, "%d = %d " VF_X " %d  COMPOSITE", n, e->divisor, n/e->divisor);
		return;
	}
	e->divisor = (e->divisor == 2)? 3 : e->divisor + 2;
}

static void SieveOp(PrimeEngine *e)
{
	if (e->sweeping)
	{
		if (e->sweep > e->limit)
		{
			e->done = true;
			Log(e, "SIEVE COMPLETE : " VF_PI "(%d) = %d", e->limit, e->primeCount);
			return;
		}
		int n = e->sweep++;
		e->cursor = n;
		if (e->cell[n] == CELL_UNKNOWN) Found(e, n);
		return;
	}

	if (e->m <= e->limit)
	{
		int m = e->m;
		e->cursor = m;
		if (e->cell[m] == CELL_UNKNOWN)
		{
			e->cell[m] = CELL_COMPOSITE;
			e->flash[m] = 1.0f;
		}
		e->m += e->p;
		return;
	}

	// done with p: the next survivor is the next prime
	int p = e->p + 1;
	while (p <= e->limit && e->cell[p] != CELL_PRIME) p++;
	if (p*p > e->limit)
	{
		e->p = p;
		e->sweeping = true;
		e->sweep = e->confirmed;
		Log(e, "P = %d : P^2 = %d > %d", p, p*p, e->limit);
		Log(e, "NOTHING LEFT TO CROSS : SURVIVORS ARE PRIME");
		return;
	}
	StartSieving(e, p);
}

void PrimesStep(PrimeEngine *e, int ops)
{
	for (int i = 0; i < ops && !e->done; i++)
	{
		if (e->mode == MODE_TRIAL) TrialOp(e);
		else SieveOp(e);
		e->ops++;
	}
}
