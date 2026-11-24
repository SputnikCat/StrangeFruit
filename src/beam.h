/*
Vector beam drawing and the phosphor display.

Beams are soft-edged capsules drawn additively, so overlapping strokes and
joints get brighter the way they do on a real vector monitor.

The phosphor display runs the sim view through a CRT-like pipeline:
  1. this frame's beams are drawn into a lines buffer
  2. persistence: max(previous * decay, lines), so moving lines leave trails
  3. bloom: blurred half and quarter resolution copies added back on top
*/

#pragma once

#include "raylib.h"

#include <stdbool.h>

// classic yellow-green P1 phosphor
#define PHOSPHOR (Color){ 130, 255, 90, 255 }

Color PhosphorColor(float intensity);	// PHOSPHOR with alpha = intensity (0..1)

void BeamInit(void);
void BeamUnload(void);

// Call between BeamBegin/BeamEnd (sets additive blending)
void BeamBegin(void);
void BeamEnd(void);
void BeamLine(Vector2 a, Vector2 b, float width, Color color);
void BeamSegment(Vector2 a, Vector2 b, float width, Color color, bool capA, bool capB);
void BeamDot(Vector2 p, float size, Color color);
void BeamPoint(Vector2 p, float size, Color color);	// one quad, cheap enough for tens of thousands

typedef struct Phosphor
{
	int width, height;
	RenderTexture2D lines;
	RenderTexture2D persist[2];
	int current;
	RenderTexture2D halfA, halfB;
	RenderTexture2D quarterA, quarterB;
	Shader decay, blur, composite;
	int decayLines, decayFactor;
	int blurDir;
	int compHalf, compQuarter, compGlow;
} Phosphor;

Phosphor PhosphorCreate(int width, int height);
void PhosphorDestroy(Phosphor *ph);
void PhosphorBeginLines(Phosphor *ph);		// starts drawing this frame's beams
void PhosphorEndLines(Phosphor *ph, float dt, float persistSeconds);
void PhosphorDraw(Phosphor *ph, Rectangle dest, float glow);
