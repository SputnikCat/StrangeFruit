/*
Strange Fruit
Prime numbers on a mid-80s vector phosphor display.

Everything is laid out on a fixed 1080x1920 portrait canvas (phone video
shaped), then scaled to the window. The whole canvas is one phosphor screen:
  top:     title band with the striped sun and the scrolling grid
  middle:  the plot, module 01 is the Ulam spiral
  below:   readout rows, the math terminal, the rate and pi(n) graphs
  bottom:  key help and the playing track (H hides it for clean capture)
*/

#include "raylib.h"
#include "raymath.h"

#include "beam.h"
#include "music.h"
#include "primes.h"
#include "vecfont.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CANVAS_W 1080
#define CANVAS_H 1920

static const Rectangle SCREEN_BOX = { 16, 16, 1048, 1888 };

// phosphor coordinates (the inner screen, 1040 x 1880)
#define SCR_W		1040.0f
#define SCR_H		1880.0f
#define HEADER_H	236.0f
static const Rectangle PLOT_BOX = { 24, 262, 992, 992 };
static const Rectangle TERM_BOX = { 24, 1352, 992, 296 };
static const Rectangle RATE_BOX = { 24, 1674, 484, 130 };
static const Rectangle PI_BOX = { 532, 1674, 484, 130 };

#define DIM		0.30f
#define MID		0.55f
#define BRIGHT	0.92f
#define STROKE	2.2f

#define GRAPH_N			120		// samples in the rate trace
#define SAMPLE_DT		0.1f	// s between samples
#define TRAIL			48		// spiral path drawn behind the cursor
#define HOLD_TIME		9.0f	// s the finished picture stays up before the next run
#define RATE_MIN		1.0f
#define RATE_MAX		400000.0f
#define AUTO_START		2.0f	// ops/s when an auto run begins...
#define AUTO_DOUBLE		2.2f	// ...doubling every this many seconds
#define MUSIC_VOLUME	0.7f

static const int SIDES[] = { 101, 201, 301, 401 };
#define SIDE_COUNT ((int)(sizeof(SIDES)/sizeof(SIDES[0])))

typedef struct App
{
	PrimeEngine eng;
	PrimeMode mode;
	int sideIndex;
	short *sx, *sy;		// Ulam spiral coordinates of every n
	int coordLimit;

	float rate;			// ops/s
	bool autoRate;
	float autoTime;
	double budget;
	bool paused, showHelp, autoCycle, muted;
	float holdTime;
	float elapsed;
	float viewSide;		// spiral cells across the plot, eased

	float sampleTimer;
	int lastPrimeCount;
	long long lastOps;
	float primeRate[GRAPH_N];
	int graphHead;
	float opsPerSec;

	float liveTimer;		// text panels refresh at a readable pace, not every frame
	char live[3][LOG_LEN];
	char logShown[6][LOG_LEN];
	int shownCursor;
} App;

//----------------------------------------------------------------------------------
// Small drawing helpers (all inside BeamBegin/BeamEnd)
//----------------------------------------------------------------------------------

static Color Ph(float k) { return PhosphorColor(k); }

static void Text(const char *s, float x, float y, float size, float k)
{
	DrawVecText(s, (Vector2){ x, y }, size, fmaxf(1.6f, size*0.11f), Ph(k));
}

static void TextRight(const char *s, float x, float y, float size, float k)
{
	DrawVecTextRight(s, (Vector2){ x, y }, size, fmaxf(1.6f, size*0.11f), Ph(k));
}

static void TextCentered(const char *s, float cx, float y, float size, float k)
{
	Text(s, cx - VecTextWidth(s, size)*0.5f, y, size, k);
}

static void Line(float x0, float y0, float x1, float y1, float w, float k)
{
	BeamLine((Vector2){ x0, y0 }, (Vector2){ x1, y1 }, w, Ph(k));
}

static void Box(Rectangle r, float k)
{
	Line(r.x, r.y, r.x + r.width, r.y, STROKE, k);
	Line(r.x + r.width, r.y, r.x + r.width, r.y + r.height, STROKE, k);
	Line(r.x + r.width, r.y + r.height, r.x, r.y + r.height, STROKE, k);
	Line(r.x, r.y + r.height, r.x, r.y, STROKE, k);
}

// box with its title set into the top edge, the way 80s lab software framed its panes
static void Frame(Rectangle r, const char *title, float size)
{
	float tw = VecTextWidth(title, size);
	float gapL = r.x + 14.0f, gapR = gapL + tw + 16.0f;
	float top = r.y, bottom = r.y + r.height, left = r.x, right = r.x + r.width;
	Line(left, top, gapL, top, STROKE, DIM);
	Line(gapR, top, right, top, STROKE, DIM);
	Line(right, top, right, bottom, STROKE, DIM);
	Line(right, bottom, left, bottom, STROKE, DIM);
	Line(left, bottom, left, top, STROKE, DIM);
	// little corner brackets, brighter
	float c = 10.0f;
	Line(left, bottom - c, left + 0.01f, bottom, STROKE, MID); Line(left, bottom, left + c, bottom, STROKE, MID);
	Line(right - c, bottom, right, bottom, STROKE, MID); Line(right, bottom, right, bottom - c, STROKE, MID);
	Text(title, gapL + 8.0f, top - size*0.5f, size, MID);
}

// 1234 -> "1234", 12345 -> "12.3K", 1234567 -> "1.23M"
static const char *Short(double v)
{
	if (v >= 1e6) return TextFormat("%.2fM", v/1e6);
	if (v >= 1e4) return TextFormat("%.1fK", v/1e3);
	return TextFormat("%.0f", v);
}

//----------------------------------------------------------------------------------
// Ulam spiral
//----------------------------------------------------------------------------------

// n = 1 at the centre, 2 to its right, then counter-clockwise outward
static void UlamXY(int n, int *x, int *y)
{
	if (n <= 1) { *x = 0; *y = 0; return; }
	int k = (int)ceil((sqrt((double)n) - 1.0)/2.0);
	int t = 2*k + 1;
	int m = t*t;
	t -= 1;
	if (n >= m - t) { *x = k - (m - n); *y = -k; return; }
	m -= t;
	if (n >= m - t) { *x = -k; *y = -k + (m - n); return; }
	m -= t;
	if (n >= m - t) { *x = -k + (m - n); *y = k; return; }
	*x = k; *y = k - (m - n - t);
}

static void BuildCoords(App *a, int limit)
{
	if (a->coordLimit == limit) return;
	free(a->sx);
	free(a->sy);
	a->sx = malloc(sizeof(short)*(limit + 1));
	a->sy = malloc(sizeof(short)*(limit + 1));
	for (int n = 1; n <= limit; n++)
	{
		int x, y;
		UlamXY(n, &x, &y);
		a->sx[n] = (short)x;
		a->sy[n] = (short)-y;	// screen y points down; keep 2 right, 3 above it
	}
	a->sx[0] = a->sy[0] = 0;
	a->coordLimit = limit;
}

// ring side that holds n, with a cell of margin
static float SideFor(int n)
{
	int k = (int)ceil((sqrt((double)fmax(n, 1)) - 1.0)/2.0);
	return (float)(2*k + 3);
}

static void DrawSpiral(const App *a, Rectangle r)
{
	const PrimeEngine *e = &a->eng;
	float side = a->viewSide;
	float s = r.width/side;		// pixels per cell
	Vector2 c = { r.x + r.width*0.5f, r.y + r.height*0.5f };
	float half = side*0.5f;

	// axes through 1
	Line(r.x + 6, c.y, r.x + r.width - 6, c.y, 1.4f, 0.07f);
	Line(c.x, r.y + 6, c.x, r.y + r.height - 6, 1.4f, 0.07f);

	// scale ticks and coordinate range on the edges
	for (int i = 1; i < 20; i++)
	{
		float t = i/20.0f;
		float len = (i%5 == 0)? 10.0f : 5.0f;
		float k = (i%5 == 0)? MID : DIM;
		float x = r.x + r.width*t, y = r.y + r.height*t;
		Line(x, r.y, x, r.y + len, 1.6f, k);
		Line(x, r.y + r.height, x, r.y + r.height - len, 1.6f, k);
		Line(r.x, y, r.x + len, y, 1.6f, k);
		Line(r.x + r.width, y, r.x + r.width - len, y, 1.6f, k);
	}
	int edge = (int)floorf(half);
	Text(TextFormat("-%d", edge), r.x + 14, r.y + r.height - 30, 13, DIM);
	TextRight(TextFormat("+%d", edge), r.x + r.width - 14, r.y + r.height - 30, 13, DIM);

	int limit = e->limit;
	float dot = Clamp(s*0.45f, 3.0f, 11.0f);
	bool showCandidates = e->mode == MODE_SIEVE && s >= 3.0f;
	int shown = (e->mode == MODE_TRIAL)? e->cursor : limit;

	// faint spiral path while the cells are big enough to follow it
	if (s >= 10.0f && shown > 1)
	{
		float k = Clamp((s - 10.0f)/30.0f, 0.0f, 1.0f)*0.10f + 0.04f;
		for (int n = 2; n <= shown; n++)
		{
			if (fabsf(a->sx[n]) > half || fabsf(a->sy[n]) > half) continue;
			Vector2 p0 = { c.x + a->sx[n - 1]*s, c.y + a->sy[n - 1]*s };
			Vector2 p1 = { c.x + a->sx[n]*s, c.y + a->sy[n]*s };
			BeamSegment(p0, p1, 1.4f, Ph(k), false, false);
		}
	}

	for (int n = 1; n <= limit; n++)
	{
		unsigned char cell = e->cell[n];
		float f = e->flash[n];
		if (cell == CELL_UNKNOWN && !showCandidates) continue;
		if (cell == CELL_COMPOSITE && f < 0.02f) continue;
		if (fabsf(a->sx[n]) > half || fabsf(a->sy[n]) > half) continue;
		Vector2 p = { c.x + a->sx[n]*s, c.y + a->sy[n]*s };

		switch (cell)
		{
		case CELL_PRIME:
			BeamPoint(p, dot*(1.0f + f*0.9f), Ph(0.80f + 0.20f*f));
			break;
		case CELL_COMPOSITE:
			BeamPoint(p, dot*(0.6f + f*0.8f), Ph(f*0.55f));
			break;
		case CELL_UNIT:
		{
			float h = fmaxf(dot, 4.0f);
			Box((Rectangle){ p.x - h, p.y - h, h*2, h*2 }, 0.5f + 0.5f*f);
			break;
		}
		default:
			BeamPoint(p, dot*0.6f, Ph(0.20f));
			break;
		}
	}

	// the numbers themselves while there's room to read them
	if (s >= 34.0f)
	{
		float size = Clamp(s*0.2f, 9.0f, 20.0f);
		float fade = Clamp((s - 34.0f)/20.0f, 0.0f, 1.0f);
		for (int n = 1; n <= shown; n++)
		{
			if (fabsf(a->sx[n]) > half - 0.4f || fabsf(a->sy[n]) > half - 0.4f) continue;
			float k = (e->cell[n] == CELL_PRIME)? 0.75f : 0.22f;
			const char *t = TextFormat("%d", n);
			float x = c.x + a->sx[n]*s, y = c.y + a->sy[n]*s;
			TextCentered(t, x, y + dot + 6.0f, size, k*fade);
		}
	}

	// the beam's path behind the cursor, and the cursor itself
	int cur = e->cursor;
	if (cur >= 1 && cur <= limit)
	{
		for (int k = 1; k < TRAIL && cur - k >= 1; k++)
		{
			int n = cur - k + 1;
			Vector2 p0 = { c.x + a->sx[n - 1]*s, c.y + a->sy[n - 1]*s };
			Vector2 p1 = { c.x + a->sx[n]*s, c.y + a->sy[n]*s };
			BeamSegment(p0, p1, 2.0f, Ph(0.5f*(1.0f - (float)k/TRAIL)), false, k == 1);
		}
		Vector2 p = { c.x + a->sx[cur]*s, c.y + a->sy[cur]*s };
		if (fabsf(a->sx[cur]) <= half && fabsf(a->sy[cur]) <= half && !e->done)
		{
			Line(r.x + 2, p.y, r.x + r.width - 2, p.y, 1.3f, 0.10f);
			Line(p.x, r.y + 2, p.x, r.y + r.height - 2, 1.3f, 0.10f);
			float h = fmaxf(dot*1.6f, 9.0f);
			Box((Rectangle){ p.x - h, p.y - h, h*2, h*2 }, BRIGHT);
		}
	}
}

//----------------------------------------------------------------------------------
// Title band: striped sun on a scrolling grid
//----------------------------------------------------------------------------------

static void DrawHeader(const App *a)
{
	float horizon = 178.0f, bottom = HEADER_H - 6.0f;
	float cx = SCR_W*0.5f;

	// sun: chords above the horizon, cut by gaps that widen toward the bottom
	float R = 120.0f;
	for (float y = horizon - R + 3.0f; y < horizon - 2.0f; y += 5.0f)
	{
		float d = horizon - y;		// height above the horizon
		float band = fmodf(d + a->elapsed*6.0f, 26.0f);
		float cut = Clamp((60.0f - d)/60.0f, 0.0f, 1.0f)*14.0f;
		if (band < cut) continue;
		float dy = y - horizon;
		float w = sqrtf(fmaxf(R*R - dy*dy, 0.0f));
		Line(cx - w, y, cx + w, y, 2.0f, 0.10f + 0.10f*(d/R));
	}

	// grid floor: lines rushing toward the viewer, rays to the vanishing point
	Line(0, horizon, SCR_W, horizon, 2.0f, 0.45f);
	for (int i = 0; i < 9; i++)
	{
		float z = fmodf(i + a->elapsed*0.9f, 9.0f) + 1.0f;	// 1 near .. 10 far
		float y = horizon + (bottom - horizon)*(1.0f/z);
		if (y > bottom) continue;
		Line(0, y, SCR_W, y, 1.6f, 0.08f + 0.30f/z);
	}
	for (int i = -14; i <= 14; i++)
	{
		float xb = cx + i*90.0f;
		Line(cx + i*6.0f, horizon, xb, bottom, 1.5f, 0.14f);
	}

	TextCentered("STRANGE FRUIT", cx, 26, 64, 1.0f);
	TextCentered("PRIME FIELD ANALYZER  REV 1.86", cx, 116, 18, MID);
	Text("MODULE 01", 24, 112, 13, DIM);
	Text("ULAM SPIRAL", 24, 132, 13, MID);
	int secs = (int)a->elapsed;
	TextRight(TextFormat("T+ %02d:%02d", secs/60, secs%60), SCR_W - 24, 112, 13, MID);
	TextRight(TextFormat("OPS %s", Short((double)a->eng.ops)), SCR_W - 24, 132, 13, DIM);
	Line(0, HEADER_H, SCR_W, HEADER_H, STROKE, MID);
}

//----------------------------------------------------------------------------------
// Readouts, terminal, graphs
//----------------------------------------------------------------------------------

static void BuildLive(App *a)
{
	const PrimeEngine *e = &a->eng;
	char (*L)[LOG_LEN] = a->live;
	a->shownCursor = e->cursor;
	for (int i = 0; i < 6; i++)
	{
		const char *s = PrimesLogLine(e, i);
		snprintf(a->logShown[i], LOG_LEN, "%s", s? s : "");
	}
	if (e->mode == MODE_TRIAL)
	{
		int n = e->n;
		if (n < 2)
		{
			snprintf(L[0], LOG_LEN, "TEST  N = %d", n);
			snprintf(L[1], LOG_LEN, "START AT 1, WALK THE SPIRAL OUTWARD");
			snprintf(L[2], LOG_LEN, " ");
			return;
		}
		snprintf(L[0], LOG_LEN, "TEST  N = %d", n);
		snprintf(L[1], LOG_LEN, VF_SQRT "%d " VF_APPROX " %.2f  TRY D = 2..%d", n, sqrt((double)n), e->bound);
		if (e->testing && e->lastDivisor > 0)
			snprintf(L[2], LOG_LEN, "%d MOD %d = %d  " VF_NE " 0", n, e->lastDivisor, e->lastRemainder);
		else if (e->testing)
			snprintf(L[2], LOG_LEN, "%d MOD ...", n);
		else if (e->cell[n] == CELL_PRIME)
			snprintf(L[2], LOG_LEN, "NO DIVISOR FOUND  =>  %d IS PRIME", n);
		else
			snprintf(L[2], LOG_LEN, "%d = %d " VF_X " %d  =>  COMPOSITE", n, e->factor, n/e->factor);
	}
	else
	{
		snprintf(L[0], LOG_LEN, "SIEVE  N " VF_LE " %d   P = %d", e->limit, e->p);
		if (e->sweeping)
		{
			snprintf(L[1], LOG_LEN, "P^2 = %d > %d : STOP CROSSING", e->p*e->p, e->limit);
			snprintf(L[2], LOG_LEN, "SURVIVOR CHECK  N = %d", e->cursor);
		}
		else
		{
			snprintf(L[1], LOG_LEN, "P^2 = %d " VF_LE " %d : KEEP CROSSING", e->p*e->p, e->limit);
			int m = e->cursor;
			snprintf(L[2], LOG_LEN, "CROSS OUT %d " VF_X " %d = %d", e->p, m/e->p, m);
		}
	}
}

static void DrawReadout(const App *a)
{
	const PrimeEngine *e = &a->eng;
	float y = PLOT_BOX.y + PLOT_BOX.height + 18.0f;
	int cur = a->shownCursor;
	int x = (cur >= 1)? a->sx[cur] : 0, yy = (cur >= 1)? -a->sy[cur] : 0;
	int to = PrimesConfirmedTo(e);
	float density = (to >= 2)? 100.0f*e->primeCount/(float)to : 0.0f;

	Text(TextFormat("N %d", cur), 24, y, 18, BRIGHT);
	Text(TextFormat("X %+d  Y %+d", x, yy), 250, y, 18, MID);
	TextRight(TextFormat(VF_PI "(N) %d", e->primeCount), SCR_W - 24, y, 18, BRIGHT);
	Text(TextFormat("DENSITY %.2f%%", density), 560, y, 18, MID);

	y += 34.0f;
	Text(PrimesModeName(e->mode), 24, y, 14, MID);
	const char *spd = a->paused? "PAUSED" : TextFormat("%s OP/S%s", Short(a->rate), a->autoRate? " AUTO" : "");
	Text(spd, 360, y, 14, a->paused? BRIGHT : DIM);
	TextRight(TextFormat("MAX GAP %d AT %d   TWINS %d", e->maxGap, e->maxGapAt, e->twinCount), SCR_W - 24, y, 14, DIM);
}

static void DrawTerminal(const App *a)
{
	Rectangle r = TERM_BOX;
	Frame(r, "MATH TERMINAL", 14);

	// live equation, big
	float y = r.y + 22.0f;
	for (int i = 0; i < 3; i++)
	{
		Text(a->live[i], r.x + 18, y, 22, (i == 2)? 1.0f : 0.8f);
		y += 34.0f;
	}
	Line(r.x + 12, y - 4, r.x + r.width - 12, y - 4, 1.5f, 0.18f);

	// log, newest at the bottom, older lines dimmer
	const int rows = 6;
	float lineH = 22.0f;
	float base = r.y + r.height - 16.0f - lineH;
	for (int i = 0; i < rows; i++)
	{
		const char *s = a->logShown[i];
		if (!s[0]) break;
		float k = 0.62f - i*0.08f;
		Text(TextFormat("> %s", s), r.x + 18, base - i*lineH, 15, k);
	}
	// blinking block cursor
	if (fmodf(a->elapsed, 1.0f) < 0.5f)
	{
		const char *s = a->logShown[0];
		float x = r.x + 18 + (s[0]? VecTextWidth(TextFormat("> %s", s), 15) + 10.0f : 0.0f);
		Line(x, base + 16, x + 10, base + 16, 3.0f, BRIGHT);
	}
}

static float NiceCeil(float v)
{
	if (v <= 1.0f) return 1.0f;
	float p = powf(10.0f, floorf(log10f(v)));
	float m = v/p;
	float n = (m <= 1.0f)? 1.0f : (m <= 2.0f)? 2.0f : (m <= 5.0f)? 5.0f : 10.0f;
	return n*p;
}

static void Grid(Rectangle r)
{
	for (int i = 1; i < 4; i++) Line(r.x, r.y + r.height*i/4.0f, r.x + r.width, r.y + r.height*i/4.0f, 1.2f, 0.07f);
	for (int i = 1; i < 8; i++) Line(r.x + r.width*i/8.0f, r.y, r.x + r.width*i/8.0f, r.y + r.height, 1.2f, 0.07f);
}

static void DrawRateGraph(const App *a)
{
	Rectangle r = RATE_BOX;
	Frame(r, "PRIMES / SEC", 13);
	Rectangle g = { r.x + 12, r.y + 34, r.width - 24, r.height - 46 };
	Grid(g);

	float hi = 0.0f;
	for (int i = 0; i < GRAPH_N; i++) hi = fmaxf(hi, a->primeRate[i]);
	hi = NiceCeil(hi*1.1f);

	Vector2 last = { 0 };
	for (int i = 0; i < GRAPH_N; i++)
	{
		int s = (a->graphHead + i)%GRAPH_N;
		Vector2 p = { g.x + g.width*i/(float)(GRAPH_N - 1), g.y + g.height*(1.0f - a->primeRate[s]/hi) };
		if (i > 0) BeamSegment(last, p, 2.0f, Ph(BRIGHT), i == 1, true);
		last = p;
	}
	float now = a->primeRate[(a->graphHead + GRAPH_N - 1)%GRAPH_N];
	TextRight(Short(now), r.x + r.width - 12, r.y + 14, 15, BRIGHT);
	Text(TextFormat("MAX %s", Short(hi)), g.x + 4, g.y + 4, 11, DIM);
}

// pi(x) against Gauss's estimate x / ln x, over the range of this run
static void DrawPiGraph(const App *a)
{
	Rectangle r = PI_BOX;
	Frame(r, VF_PI "(N)  VS  N / LN N", 13);
	Rectangle g = { r.x + 12, r.y + 34, r.width - 24, r.height - 46 };
	Grid(g);
	const PrimeEngine *e = &a->eng;
	int limit = e->limit;
	float top = 1.2f*limit/logf((float)limit);

	// estimate, dashed
	const int N = 60;
	for (int i = 1; i < N; i += 2)
	{
		float x0 = fmaxf(limit*(float)i/N, 3.0f), x1 = limit*(float)(i + 1)/N;
		Vector2 p0 = { g.x + g.width*x0/limit, g.y + g.height*(1.0f - x0/logf(x0)/top) };
		Vector2 p1 = { g.x + g.width*x1/limit, g.y + g.height*(1.0f - x1/logf(x1)/top) };
		BeamLine(p0, p1, 1.6f, Ph(0.32f));
	}

	// pi(x), as far as it is known
	int to = PrimesConfirmedTo(e);
	const int S = 160;
	int count = 0, n = 1;
	Vector2 last = { g.x, g.y + g.height };
	for (int i = 1; i <= S; i++)
	{
		int x = (int)((double)limit*i/S);
		if (x > to) break;
		for (; n <= x; n++) if (e->cell[n] == CELL_PRIME) count++;
		Vector2 p = { g.x + g.width*x/(float)limit, g.y + g.height*(1.0f - count/top) };
		BeamSegment(last, p, 2.0f, Ph(BRIGHT), i == 1, true);
		last = p;
	}
	if (to >= 3)
	{
		float est = to/logf((float)to);
		TextRight(TextFormat("RATIO %.3f", e->primeCount/est), r.x + r.width - 12, r.y + 14, 13, BRIGHT);
	}
}

static void DrawFooter(const App *a)
{
	float y = SCR_H - 58.0f;
	if (a->showHelp)
	{
		TextCentered("1 TRIAL  2 SIEVE  UP/DN SPEED  A AUTO  [ ] GRID  SPACE PAUSE", SCR_W*0.5f, y, 13, DIM);
		TextCentered("R RESTART  C CYCLE  N TRACK  M MUTE  H HIDE  F11 FULL  ESC QUIT", SCR_W*0.5f, y + 22, 13, DIM);
	}
	const char *track = MusicTrackName();
	if (track && !a->muted)
	{
		char name[48];
		snprintf(name, sizeof(name), "%s", GetFileNameWithoutExt(track));
		TextCentered(TextFormat("TRACK %d/%d  %s", MusicTrackNumber(), MusicTrackCount(), name), SCR_W*0.5f, y + (a->showHelp? 44 : 30), 11, 0.22f);
	}
}

static void DrawDone(const App *a)
{
	const PrimeEngine *e = &a->eng;
	if (!e->done) return;
	float k = 0.6f + 0.4f*(fmodf(a->holdTime, 1.2f) < 0.8f);
	Rectangle b = { PLOT_BOX.x + PLOT_BOX.width*0.5f - 260, PLOT_BOX.y + 24, 520, 64 };
	Box(b, MID);
	TextCentered(TextFormat(VF_PI "(%d) = %d", e->limit, e->primeCount), b.x + b.width*0.5f, b.y + 18, 28, k);
}

//----------------------------------------------------------------------------------
// Runs
//----------------------------------------------------------------------------------

static void StartRun(App *a, PrimeMode mode)
{
	a->mode = mode;
	int side = SIDES[a->sideIndex];
	int limit = side*side;
	BuildCoords(a, limit);
	PrimesStart(&a->eng, mode, limit);
	a->budget = 0.0;
	a->holdTime = 0.0f;
	a->autoTime = 0.0f;
	if (a->autoRate) a->rate = AUTO_START;
	a->viewSide = (mode == MODE_TRIAL)? 5.0f : (float)side + 1.0f;
	a->lastPrimeCount = 0;
	a->lastOps = 0;
	memset(a->primeRate, 0, sizeof(a->primeRate));
	BuildLive(a);
}

static void Update(App *a, float dt)
{
	PrimeEngine *e = &a->eng;

	if (IsKeyPressed(KEY_ONE)) StartRun(a, MODE_TRIAL);
	if (IsKeyPressed(KEY_TWO)) StartRun(a, MODE_SIEVE);
	if (IsKeyPressed(KEY_R)) StartRun(a, a->mode);
	if (IsKeyPressed(KEY_SPACE)) a->paused = !a->paused;
	if (IsKeyPressed(KEY_H)) a->showHelp = !a->showHelp;
	if (IsKeyPressed(KEY_C)) a->autoCycle = !a->autoCycle;
	if (IsKeyPressed(KEY_N)) MusicNext();
	if (IsKeyPressed(KEY_M)) a->muted = !a->muted;
	if (IsKeyPressed(KEY_A)) { a->autoRate = !a->autoRate; a->autoTime = log2f(fmaxf(a->rate/AUTO_START, 1.0f))*AUTO_DOUBLE; }
	if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) { a->autoRate = false; a->rate = fminf(a->rate*2.0f, RATE_MAX); }
	if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) { a->autoRate = false; a->rate = fmaxf(a->rate*0.5f, RATE_MIN); }
	if (IsKeyPressed(KEY_LEFT_BRACKET) && a->sideIndex > 0) { a->sideIndex--; StartRun(a, a->mode); }
	if (IsKeyPressed(KEY_RIGHT_BRACKET) && a->sideIndex < SIDE_COUNT - 1) { a->sideIndex++; StartRun(a, a->mode); }

	a->elapsed += dt;

	if (!a->paused && !e->done)
	{
		if (a->autoRate)
		{
			a->autoTime += dt;
			a->rate = fminf(AUTO_START*powf(2.0f, a->autoTime/AUTO_DOUBLE), RATE_MAX);
		}
		e->verbose = a->rate < 300.0f;
		a->budget += a->rate*dt;
		int ops = (int)fmin(a->budget, 60000.0);
		a->budget -= ops;
		if (a->budget > a->rate) a->budget = 0.0;	// don't bank a backlog
		PrimesStep(e, ops);
	}
	if (e->done)
	{
		a->holdTime += dt;
		if (a->autoCycle && a->holdTime > HOLD_TIME) StartRun(a, (a->mode == MODE_TRIAL)? MODE_SIEVE : MODE_TRIAL);
	}

	// glow fades
	float fade = expf(-dt*2.5f);
	for (int n = 0; n <= e->limit; n++) e->flash[n] *= fade;

	// the plot widens as the spiral grows
	float target = (e->mode == MODE_TRIAL)? fmaxf(SideFor(e->cursor), 5.0f) : (float)SIDES[a->sideIndex] + 1.0f;
	a->viewSide += (target - a->viewSide)*(1.0f - expf(-dt*1.8f));

	// rate samples
	a->sampleTimer += dt;
	if (a->sampleTimer >= SAMPLE_DT)
	{
		a->primeRate[a->graphHead] = (e->primeCount - a->lastPrimeCount)/a->sampleTimer;
		a->graphHead = (a->graphHead + 1)%GRAPH_N;
		a->lastPrimeCount = e->primeCount;
		a->lastOps = e->ops;
		a->sampleTimer = 0.0f;
	}

	// the live equation holds still long enough to read once things get fast
	a->liveTimer -= dt;
	if (a->rate < 30.0f || a->liveTimer <= 0.0f)
	{
		BuildLive(a);
		a->liveTimer = 1.0f/10.0f;
	}
}

static void DrawScreen(const App *a)
{
	DrawHeader(a);
	Frame(PLOT_BOX, "ULAM SPIRAL", 14);
	DrawSpiral(a, PLOT_BOX);
	DrawDone(a);
	DrawReadout(a);
	DrawTerminal(a);
	DrawRateGraph(a);
	DrawPiGraph(a);
	DrawFooter(a);
}

//----------------------------------------------------------------------------------
// Window
//----------------------------------------------------------------------------------

// window centred on the monitor, shaped to the portrait canvas
static void FitWindow(void)
{
	int m = GetCurrentMonitor();
	int mw = GetMonitorWidth(m), mh = GetMonitorHeight(m);
	float scale = fminf(1.0f, fminf(mw*0.9f/CANVAS_W, mh*0.9f/CANVAS_H));
	int ww = (int)(CANVAS_W*scale), wh = (int)(CANVAS_H*scale);
	SetWindowSize(ww, wh);
	SetWindowPosition((mw - ww)/2, (mh - wh)/2);
}

int main(int argc, char **argv)
{
	static App app = { 0 };
	app.showHelp = true;
	app.autoCycle = true;
	app.autoRate = true;
	app.rate = AUTO_START;
	app.sideIndex = 1;
	PrimeMode startMode = MODE_TRIAL;
	bool fullscreen = false;
	float shotAt = -1.0f;		// --shot SECONDS FILE: save the canvas then quit (for checking the look)
	const char *shotFile = NULL;
	for (int i = 1; i < argc; i++)
	{
		if (TextIsEqual(argv[i], "--sieve")) startMode = MODE_SIEVE;
		else if (TextIsEqual(argv[i], "--fullscreen")) fullscreen = true;
		else if (TextIsEqual(argv[i], "--fast")) { app.autoRate = false; app.rate = RATE_MAX; }
		else if (TextIsEqual(argv[i], "--rate") && i + 1 < argc) { app.autoRate = false; app.rate = (float)atof(argv[++i]); }
		else if (TextIsEqual(argv[i], "--shot") && i + 2 < argc) { shotAt = (float)atof(argv[++i]); shotFile = argv[++i]; }
	}

	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
	InitWindow(CANVAS_W/2, CANVAS_H/2, "Strange Fruit");
	SetExitKey(KEY_ESCAPE);
	if (fullscreen) ToggleBorderlessWindowed();
	else FitWindow();

	InitAudioDevice();
	BeamInit();
	MusicInit();

	RenderTexture2D canvas = LoadRenderTexture(CANVAS_W, CANVAS_H);
	SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
	Rectangle screenInner = { SCREEN_BOX.x + 4, SCREEN_BOX.y + 4, SCREEN_BOX.width - 8, SCREEN_BOX.height - 8 };
	Phosphor phosphor = PhosphorCreate((int)screenInner.width, (int)screenInner.height);

	StartRun(&app, startMode);

	while (!WindowShouldClose())
	{
		float dt = fminf(GetFrameTime(), 0.1f);
		if (IsKeyPressed(KEY_F11))
		{
			ToggleBorderlessWindowed();
			if (!IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) FitWindow();
		}

		Update(&app, dt);
		MusicUpdate(app.muted? 0.0f : MUSIC_VOLUME, dt);

		PhosphorBeginLines(&phosphor);
		DrawScreen(&app);
		PhosphorEndLines(&phosphor, dt, 0.035f);

		BeginTextureMode(canvas);
		ClearBackground(BLACK);
		PhosphorDraw(&phosphor, screenInner, 1.0f);
		BeamBegin();
		BeamLine((Vector2){ SCREEN_BOX.x, SCREEN_BOX.y }, (Vector2){ SCREEN_BOX.x + SCREEN_BOX.width, SCREEN_BOX.y }, STROKE, Ph(DIM));
		BeamLine((Vector2){ SCREEN_BOX.x + SCREEN_BOX.width, SCREEN_BOX.y }, (Vector2){ SCREEN_BOX.x + SCREEN_BOX.width, SCREEN_BOX.y + SCREEN_BOX.height }, STROKE, Ph(DIM));
		BeamLine((Vector2){ SCREEN_BOX.x + SCREEN_BOX.width, SCREEN_BOX.y + SCREEN_BOX.height }, (Vector2){ SCREEN_BOX.x, SCREEN_BOX.y + SCREEN_BOX.height }, STROKE, Ph(DIM));
		BeamLine((Vector2){ SCREEN_BOX.x, SCREEN_BOX.y + SCREEN_BOX.height }, (Vector2){ SCREEN_BOX.x, SCREEN_BOX.y }, STROKE, Ph(DIM));
		BeamEnd();
		EndTextureMode();

		if (shotFile && app.elapsed >= shotAt)
		{
			Image img = LoadImageFromTexture(canvas.texture);
			ImageFlipVertical(&img);
			ExportImage(img, shotFile);
			UnloadImage(img);
			break;
		}

		// letterbox the portrait canvas into whatever shape the window is
		float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
		float scale = fminf(sw/CANVAS_W, sh/CANVAS_H);
		Rectangle dest = { (sw - CANVAS_W*scale)*0.5f, (sh - CANVAS_H*scale)*0.5f, CANVAS_W*scale, CANVAS_H*scale };

		BeginDrawing();
		ClearBackground(BLACK);
		DrawTexturePro(canvas.texture, (Rectangle){ 0, 0, CANVAS_W, -CANVAS_H }, dest, Vector2Zero(), 0.0f, WHITE);
		EndDrawing();
	}

	PrimesFree(&app.eng);
	free(app.sx);
	free(app.sy);
	PhosphorDestroy(&phosphor);
	UnloadRenderTexture(canvas);
	MusicUnload();
	BeamUnload();
	CloseAudioDevice();
	CloseWindow();
	return 0;
}
