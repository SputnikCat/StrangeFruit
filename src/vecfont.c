#include "vecfont.h"
#include "beam.h"

#include <ctype.h>
#include <math.h>

// Each glyph is a list of polylines separated by spaces. A polyline is a run
// of digit pairs "xy" on a 4 wide by 6 tall grid, y pointing down.
static const char *GLYPHS[128] = {
	['A'] = "0602204246 0444",
	['B'] = "06003041423303 3344453606",
	['C'] = "40000646",
	['D'] = "00204244260600",
	['E'] = "40000646 0333",
	['F'] = "400006 0333",
	['G'] = "400006464323",
	['H'] = "0006 4046 0343",
	['I'] = "0040 2026 0646",
	['J'] = "40460604",
	['K'] = "0006 400346",
	['L'] = "000646",
	['M'] = "0600224046",
	['N'] = "06004640",
	['O'] = "0040460600",
	['P'] = "0600404303",
	['Q'] = "004044260600 2446",
	['R'] = "0600404303 1346",
	['S'] = "400003434606",
	['T'] = "0040 2026",
	['U'] = "00064640",
	['V'] = "002640",
	['W'] = "0006244640",
	['X'] = "0046 4006",
	['Y'] = "002240 2226",
	['Z'] = "00400646",
	['0'] = "103041453616050110",
	['1'] = "112026 1636",
	['2'] = "004043030646",
	['3'] = "00404606 0343",
	['4'] = "000343 4046",
	['5'] = "4000033344453606",
	['6'] = "400006464303",
	['7'] = "004026",
	['8'] = "0040460600 0343",
	['9'] = "430300404606",
	['.'] = "2526",
	[','] = "252617",
	[':'] = "2122 2425",
	['-'] = "1333",
	['+'] = "1333 2224",
	['/'] = "0640",
	['('] = "30121436",
	[')'] = "10323416",
	['%'] = "0640 0111 3545",
	['='] = "0242 0444",
	['<'] = "310335",
	['>'] = "114315",
	['_'] = "0646",
	['\''] = "2021",
	['"'] = "1011 3031",
	['!'] = "2024 2526",
	['?'] = "010040422324 2526",
	['['] = "30101636",
	[']'] = "10303616",
	['*'] = "2125 0244 0442",
	['^'] = "122032",
	['|'] = "2026",
	['#'] = "1016 3036 0242 0444",
	['~'] = "02112233",
	// math symbols on control codes, see vecfont.h
	[1] = "0314264060",		// square root, its bar runs on over the next glyph
	[2] = "0242 0444 3115",	// not equal
	[3] = "0141 1116 3136",	// pi
	[4] = "310234 0636",	// less than or equal
	[5] = "1234 1432",		// small multiply cross
	[6] = "130234 0636",	// greater than or equal
	[7] = "0213 2233 0415 2435",	// approximately equal
};

#define GLYPH_ADVANCE 6.0f

float VecTextWidth(const char *text, float size)
{
	float unit = size/6.0f;
	int count = 0;
	for (const char *c = text; *c; c++) count++;
	if (count == 0) return 0.0f;
	return ((count - 1)*GLYPH_ADVANCE + 4.0f)*unit;
}

void DrawVecText(const char *text, Vector2 pos, float size, float width, Color color)
{
	float unit = size/6.0f;
	float x = pos.x;

	for (const char *c = text; *c; c++, x += GLYPH_ADVANCE*unit)
	{
		int ch = toupper((unsigned char)*c);
		const char *g = (ch < 128)? GLYPHS[ch] : NULL;
		if (g == NULL) continue;

		// whole-pixel origin, so each copy of a letter rasterizes identically
		float ox = floorf(x + 0.5f), oy = floorf(pos.y + 0.5f);
		Vector2 last = { 0 };
		bool penDown = false;
		for (const char *p = g; *p; )
		{
			if (*p == ' ') { penDown = false; p++; continue; }

			Vector2 pt = { ox + (p[0] - '0')*unit, oy + (p[1] - '0')*unit };
			if (penDown) BeamLine(last, pt, width, color);
			last = pt;
			penDown = true;
			p += 2;
		}
	}
}

void DrawVecTextRight(const char *text, Vector2 rightTop, float size, float width, Color color)
{
	DrawVecText(text, (Vector2){ rightTop.x - VecTextWidth(text, size), rightTop.y }, size, width, color);
}
