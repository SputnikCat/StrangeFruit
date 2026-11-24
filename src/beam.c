#include "beam.h"
#include "raymath.h"
#include "rlgl.h"

#include <math.h>

static Texture2D blob;

Color PhosphorColor(float intensity)
{
	// premultiplied: the beams blend with MAX, which ignores alpha
	float k = Clamp(intensity, 0.0f, 1.0f);
	Color c = PHOSPHOR;
	return (Color){ (unsigned char)(c.r*k), (unsigned char)(c.g*k), (unsigned char)(c.b*k), (unsigned char)(k*255.0f) };
}

void BeamInit(void)
{
	// radial falloff: a hot core with a soft skirt, sampled as a capsule
	const int size = 64;
	Image img = GenImageColor(size, size, BLANK);
	Color *px = img.data;
	for (int y = 0; y < size; y++)
		for (int x = 0; x < size; x++)
		{
			float dx = (x + 0.5f)/size*2.0f - 1.0f;
			float dy = (y + 0.5f)/size*2.0f - 1.0f;
			float r2 = dx*dx + dy*dy;
			float core = expf(-r2*14.0f);
			float skirt = expf(-r2*4.0f)*0.25f;
			float v = fminf(core + skirt, 1.0f)*fmaxf(0.0f, 1.0f - r2);
			unsigned char b = (unsigned char)(v*255.0f);
			px[y*size + x] = (Color){ b, b, b, b };
		}
	blob = LoadTextureFromImage(img);
	UnloadImage(img);
	SetTextureFilter(blob, TEXTURE_FILTER_BILINEAR);
}

void BeamUnload(void)
{
	UnloadTexture(blob);
}

void BeamBegin(void)
{
	rlDisableBackfaceCulling();
	// MAX keeps crossings and stroke joints at the brightness of a single line,
	// so text reads evenly instead of hot spots at every vertex
	rlSetBlendFactors(RL_ONE, RL_ONE, RL_MAX);
	BeginBlendMode(BLEND_CUSTOM);
}

void BeamEnd(void)
{
	EndBlendMode();
	rlEnableBackfaceCulling();
}

// quad from p0 to p1, half-width along n, texture v from v0 to v1
static void BeamQuad(Vector2 p0, Vector2 p1, Vector2 n, float v0, float v1)
{
	rlTexCoord2f(0.0f, v0); rlVertex2f(p0.x + n.x, p0.y + n.y);
	rlTexCoord2f(1.0f, v0); rlVertex2f(p0.x - n.x, p0.y - n.y);
	rlTexCoord2f(1.0f, v1); rlVertex2f(p1.x - n.x, p1.y - n.y);
	rlTexCoord2f(0.0f, v1); rlVertex2f(p1.x + n.x, p1.y + n.y);
}

void BeamSegment(Vector2 a, Vector2 b, float width, Color color, bool capA, bool capB)
{
	if (color.r == 0 && color.g == 0 && color.b == 0) return;

	Vector2 d = Vector2Subtract(b, a);
	float len = Vector2Length(d);
	Vector2 dir = (len > 1e-4f)? Vector2Scale(d, 1.0f/len) : (Vector2){ 1.0f, 0.0f };
	// pad the quad so the falloff spans several pixels; a 2px quad samples the
	// profile so coarsely that brightness swings with sub-pixel position
	float h = width*0.5f + 1.5f;
	Vector2 n = { -dir.y*h, dir.x*h };
	Vector2 t = Vector2Scale(dir, h);

	rlCheckRenderBatchLimit(12);
	rlSetTexture(blob.id);
	rlBegin(RL_QUADS);
	rlColor4ub(color.r, color.g, color.b, color.a);
	if (capA) BeamQuad(Vector2Subtract(a, t), a, n, 0.0f, 0.5f);
	BeamQuad(a, b, n, 0.5f, 0.5f);
	if (capB) BeamQuad(b, Vector2Add(b, t), n, 0.5f, 1.0f);
	rlEnd();
	rlSetTexture(0);
}

void BeamLine(Vector2 a, Vector2 b, float width, Color color)
{
	BeamSegment(a, b, width, color, true, true);
}

void BeamDot(Vector2 p, float size, Color color)
{
	BeamSegment(p, p, size, color, true, true);
}

void BeamPoint(Vector2 p, float size, Color color)
{
	if (color.r == 0 && color.g == 0 && color.b == 0) return;
	float h = size*0.5f + 1.5f;
	rlCheckRenderBatchLimit(4);
	rlSetTexture(blob.id);
	rlBegin(RL_QUADS);
	rlColor4ub(color.r, color.g, color.b, color.a);
	rlTexCoord2f(0.0f, 0.0f); rlVertex2f(p.x - h, p.y - h);
	rlTexCoord2f(0.0f, 1.0f); rlVertex2f(p.x - h, p.y + h);
	rlTexCoord2f(1.0f, 1.0f); rlVertex2f(p.x + h, p.y + h);
	rlTexCoord2f(1.0f, 0.0f); rlVertex2f(p.x + h, p.y - h);
	rlEnd();
	rlSetTexture(0);
}

//----------------------------------------------------------------------------------
// Phosphor display
//----------------------------------------------------------------------------------

static const char *DECAY_FS =
	"#version 330\n"
	"in vec2 fragTexCoord;\n"
	"uniform sampler2D texture0;\n"
	"uniform sampler2D linesTex;\n"
	"uniform float decay;\n"
	"out vec4 finalColor;\n"
	"void main()\n"
	"{\n"
	"    vec3 old = max(texture(texture0, fragTexCoord).rgb*decay - 1.5/255.0, 0.0);\n"
	"    vec3 now = texture(linesTex, fragTexCoord).rgb;\n"
	"    finalColor = vec4(max(old, now), 1.0);\n"
	"}\n";

static const char *BLUR_FS =
	"#version 330\n"
	"in vec2 fragTexCoord;\n"
	"uniform sampler2D texture0;\n"
	"uniform vec2 dir;\n"
	"out vec4 finalColor;\n"
	"const float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);\n"
	"void main()\n"
	"{\n"
	"    vec3 c = texture(texture0, fragTexCoord).rgb*w[0];\n"
	"    for (int i = 1; i < 5; i++)\n"
	"    {\n"
	"        c += texture(texture0, fragTexCoord + dir*float(i)).rgb*w[i];\n"
	"        c += texture(texture0, fragTexCoord - dir*float(i)).rgb*w[i];\n"
	"    }\n"
	"    finalColor = vec4(c, 1.0);\n"
	"}\n";

static const char *COMPOSITE_FS =
	"#version 330\n"
	"in vec2 fragTexCoord;\n"
	"uniform sampler2D texture0;\n"
	"uniform sampler2D halfTex;\n"
	"uniform sampler2D quarterTex;\n"
	"uniform float glow;\n"
	"out vec4 finalColor;\n"
	"void main()\n"
	"{\n"
	"    vec3 base = texture(texture0, fragTexCoord).rgb;\n"
	"    vec3 bloom = texture(halfTex, fragTexCoord).rgb*0.8 + texture(quarterTex, fragTexCoord).rgb*1.4;\n"
	"    finalColor = vec4(base + bloom*glow, 1.0);\n"
	"}\n";

static RenderTexture2D MakeTarget(int w, int h)
{
	RenderTexture2D rt = LoadRenderTexture(w, h);
	SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR);
	SetTextureWrap(rt.texture, TEXTURE_WRAP_CLAMP);
	BeginTextureMode(rt);
	ClearBackground(BLACK);
	EndTextureMode();
	return rt;
}

// render textures are stored upside down, so every copy flips the source
static void DrawFlipped(Texture2D tex, Rectangle dest)
{
	DrawTexturePro(tex, (Rectangle){ 0, 0, (float)tex.width, (float)-tex.height }, dest, Vector2Zero(), 0.0f, WHITE);
}

static Rectangle Full(RenderTexture2D rt)
{
	return (Rectangle){ 0, 0, (float)rt.texture.width, (float)rt.texture.height };
}

static void Blit(RenderTexture2D src, RenderTexture2D dst)
{
	BeginTextureMode(dst);
	ClearBackground(BLACK);
	DrawFlipped(src.texture, Full(dst));
	EndTextureMode();
}

// rlgl forgets extra shader textures on every batch flush, so passes bind
// them only after the target and shader are set
static void BlurPass(Phosphor *ph, RenderTexture2D src, RenderTexture2D dst, float dx, float dy)
{
	Vector2 dir = { dx/src.texture.width, dy/src.texture.height };
	BeginTextureMode(dst);
	ClearBackground(BLACK);
	BeginShaderMode(ph->blur);
	SetShaderValue(ph->blur, ph->blurDir, &dir, SHADER_UNIFORM_VEC2);
	DrawFlipped(src.texture, Full(dst));
	EndShaderMode();
	EndTextureMode();
}

Phosphor PhosphorCreate(int width, int height)
{
	Phosphor ph = { 0 };
	ph.width = width;
	ph.height = height;
	ph.lines = MakeTarget(width, height);
	ph.persist[0] = MakeTarget(width, height);
	ph.persist[1] = MakeTarget(width, height);
	ph.halfA = MakeTarget(width/2, height/2);
	ph.halfB = MakeTarget(width/2, height/2);
	ph.quarterA = MakeTarget(width/4, height/4);
	ph.quarterB = MakeTarget(width/4, height/4);

	ph.decay = LoadShaderFromMemory(NULL, DECAY_FS);
	ph.decayLines = GetShaderLocation(ph.decay, "linesTex");
	ph.decayFactor = GetShaderLocation(ph.decay, "decay");
	ph.blur = LoadShaderFromMemory(NULL, BLUR_FS);
	ph.blurDir = GetShaderLocation(ph.blur, "dir");
	ph.composite = LoadShaderFromMemory(NULL, COMPOSITE_FS);
	ph.compHalf = GetShaderLocation(ph.composite, "halfTex");
	ph.compQuarter = GetShaderLocation(ph.composite, "quarterTex");
	ph.compGlow = GetShaderLocation(ph.composite, "glow");
	return ph;
}

void PhosphorDestroy(Phosphor *ph)
{
	UnloadRenderTexture(ph->lines);
	UnloadRenderTexture(ph->persist[0]);
	UnloadRenderTexture(ph->persist[1]);
	UnloadRenderTexture(ph->halfA);
	UnloadRenderTexture(ph->halfB);
	UnloadRenderTexture(ph->quarterA);
	UnloadRenderTexture(ph->quarterB);
	UnloadShader(ph->decay);
	UnloadShader(ph->blur);
	UnloadShader(ph->composite);
}

void PhosphorBeginLines(Phosphor *ph)
{
	BeginTextureMode(ph->lines);
	ClearBackground(BLACK);
	BeamBegin();
}

void PhosphorEndLines(Phosphor *ph, float dt, float persistSeconds)
{
	BeamEnd();
	EndTextureMode();

	// persistence
	float decay = (persistSeconds > 0.001f)? expf(-dt/persistSeconds) : 0.0f;
	int next = 1 - ph->current;
	BeginTextureMode(ph->persist[next]);
	ClearBackground(BLACK);
	BeginShaderMode(ph->decay);
	SetShaderValue(ph->decay, ph->decayFactor, &decay, SHADER_UNIFORM_FLOAT);
	SetShaderValueTexture(ph->decay, ph->decayLines, ph->lines.texture);
	DrawFlipped(ph->persist[ph->current].texture, Full(ph->persist[next]));
	EndShaderMode();
	EndTextureMode();
	ph->current = next;

	// bloom
	Blit(ph->persist[ph->current], ph->halfA);
	BlurPass(ph, ph->halfA, ph->halfB, 1.0f, 0.0f);
	BlurPass(ph, ph->halfB, ph->halfA, 0.0f, 1.0f);
	Blit(ph->halfA, ph->quarterA);
	BlurPass(ph, ph->quarterA, ph->quarterB, 1.5f, 0.0f);
	BlurPass(ph, ph->quarterB, ph->quarterA, 0.0f, 1.5f);
}

void PhosphorDraw(Phosphor *ph, Rectangle dest, float glow)
{
	RenderTexture2D src = ph->persist[ph->current];
	BeginShaderMode(ph->composite);
	SetShaderValue(ph->composite, ph->compGlow, &glow, SHADER_UNIFORM_FLOAT);
	SetShaderValueTexture(ph->composite, ph->compHalf, ph->halfA.texture);
	SetShaderValueTexture(ph->composite, ph->compQuarter, ph->quarterA.texture);
	DrawFlipped(src.texture, dest);
	EndShaderMode();
}
