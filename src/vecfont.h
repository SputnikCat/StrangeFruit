/*
Vector stroke font.
Every glyph is a handful of line segments on a 4x6 grid, the way early
vector arcade hardware drew its lettering. Lowercase is drawn as uppercase.
*/

#pragma once

#include "raylib.h"

// math symbols live on control codes; paste them between string literals,
// e.g. VF_SQRT "N", so a following hex digit isn't eaten by the escape
#define VF_SQRT	""
#define VF_NE	""
#define VF_PI	""
#define VF_LE	""
#define VF_X	""
#define VF_GE	""
#define VF_APPROX	""

// size is the cap height in pixels, width is the beam width in pixels
float VecTextWidth(const char *text, float size);
void DrawVecText(const char *text, Vector2 pos, float size, float width, Color color);
void DrawVecTextRight(const char *text, Vector2 rightTop, float size, float width, Color color);
