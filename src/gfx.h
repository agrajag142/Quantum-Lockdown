// gfx.h - drawing for the full-screen game.
//
// The picture is the whole TV: 1920x1080, 32-bit (0xAABBGGRR, the PS5's
// scanout format). The game itself works in "units": a 1280x720 screen drawn
// at 1.5x, so the shapes and sizes from NUCLEAR MISSION (a 720x720 screen
// shown 1.5x on the TV) carry straight over: the agent at scale 1.3 is the
// same size on the TV as before, there is just more room round him.
//
// Two layers of drawing:
//   px...()   pixel space (1920x1080), float corners: used for the agent and
//             for anything that wants the full resolution
//   the rest  unit space, the same names and arguments as the sketch's
//             (fillRect, fillPolygon, fillCircle, drawLine, drawVectorText,
//             hazardBand...), so its painters can be copied over as they are
//
// Colours stay RGB565 as in the sketch (rgb565(), agent_rig.h's colours) and
// are turned into 32-bit as they are drawn.

#ifndef GFX_H
#define GFX_H

#include <math.h>
#include <stdint.h>

#define FB_W 1920
#define FB_H 1080
#define UNIT_K 1.5f              // pixels per unit
#define SCREEN_W 1280            // the game's screen, in units
#define SCREEN_H 720

#ifndef PI
#define PI 3.14159265f
#endif
#ifndef HALF_PI
#define HALF_PI 1.57079633f
#endif
#ifndef TWO_PI
#define TWO_PI 6.28318531f
#endif

// The TV picture (the platform copies it to the screen each frame)
extern uint32_t g_frame[FB_W * FB_H];
// Where drawing goes: g_frame, or a background picture being painted
extern uint32_t *gfxTarget;
extern uint32_t lut565[65536];

void gfxInit();

static inline uint16_t rgb565(int r, int g, int b) {
  r = r < 0 ? 0 : (r > 255 ? 255 : r);
  g = g < 0 ? 0 : (g > 255 ? 255 : g);
  b = b < 0 ? 0 : (b > 255 ? 255 : b);
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
static inline uint16_t dimmed(int r, int g, int b, float f) {
  return rgb565((int)(r * f), (int)(g * f), (int)(b * f));
}
static inline uint32_t c32(uint16_t c) { return lut565[c]; }
static inline uint32_t rgba(int r, int g, int b) { return 0xff000000u | (uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r; }

template <typename T> static inline T clampv(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---- clipping (units): drawing only lands between these rows
void gfxClip(int y0, int y1);          // e.g. gfxClip(HUD_H, SCREEN_H - 1)
void gfxClipAll();                     // the whole screen

// ---- pixel space
void pxSpan(int y, int x0, int x1, uint32_t c);
void pxRect(int x0, int y0, int x1, int y1, uint32_t c);      // corners included
void pxPoly(const float *xs, const float *ys, int n, uint32_t c);
void pxCircle(float cx, float cy, float r, uint32_t c);
void pxLine(float x0, float y0, float x1, float y1, float w, uint32_t c);   // w: thickness (pixels)
void pxBlend(int x0, int y0, int x1, int y1, int r, int g, int b, float a); // darken / tint a box

// ---- unit space (as the sketch)
void fillRect(int x0, int y0, int x1, int y1, uint16_t c);              // corners included
void fillRectF(float x0, float y0, float x1, float y1, uint16_t c);     // edges (not inclusive)
void fillPolygon(const int *xs, const int *ys, int n, uint16_t c);
void fillPolygonF(const float *xs, const float *ys, int n, uint16_t c);
void fillCircle(int cx, int cy, int r, uint16_t c);
void blendPolygonF(const float *xs, const float *ys, int n, int r, int g, int b, float a);   // see-through, a: 0..1
void drawCircle(int cx, int cy, int r, uint16_t c);
void drawLine(int x0, int y0, int x1, int y1, uint16_t c, bool thick);
void drawLineF(float x0, float y0, float x1, float y1, uint16_t c, float w);   // w in units
void drawPixel(int x, int y, uint16_t c);
void drawRectOutline(int x0, int y0, int x1, int y1, uint16_t c, int w);
void hazardBand(int x0, int x1, int y0, int y1);
void blendRect(int x0, int y0, int x1, int y1, int r, int g, int b, float a);
void glowDisc(float cx, float cy, float r, int cr, int cg, int cb, float a);   // a soft light: brightest in the middle, fading out to r
void drawVectorText(const char *text, float cx, float cy, float size, uint16_t c, bool thick);
void drawVectorTextW(const char *text, float cx, float cy, float size, uint16_t c, float width);   // width in units
float vectorTextWidth(const char *text, float size);

// ---- the agent (and the people) through agent_rig.h, at full resolution
struct AgentPose;
void drawAgentU(const AgentPose &p, float x, float floorY, int facing, float scale);

#endif // GFX_H
