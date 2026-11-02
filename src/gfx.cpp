// gfx.cpp - drawing for the full-screen game (see gfx.h)

#include "gfx.h"
#include "agent_rig.h"

#include <string.h>

uint32_t g_frame[FB_W * FB_H];
uint32_t *gfxTarget = g_frame;
uint32_t lut565[65536];

static int clipY0 = 0, clipY1 = FB_H - 1;   // pixel rows

void gfxInit() {
  for (int i = 0; i < 65536; i++) {
    uint32_t r = (i >> 11) & 31, g = (i >> 5) & 63, b = i & 31;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    lut565[i] = 0xff000000u | b << 16 | g << 8 | r;
  }
}

// The first pixel whose centre is at or past edge v (pixel space)
static inline int edgeLo(float v) { return (int)ceilf(v - 0.5f); }

void gfxClip(int y0, int y1) {
  clipY0 = clampv(edgeLo(y0 * UNIT_K), 0, FB_H - 1);
  clipY1 = clampv(edgeLo((y1 + 1) * UNIT_K) - 1, 0, FB_H - 1);
}
void gfxClipAll() {
  clipY0 = 0;
  clipY1 = FB_H - 1;
}

// ============================================================ pixel space
void pxSpan(int y, int x0, int x1, uint32_t c) {
  if (y < clipY0 || y > clipY1) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= FB_W) x1 = FB_W - 1;
  if (x1 < x0) return;
  uint32_t *row = gfxTarget + (size_t)y * FB_W;
  for (int x = x0; x <= x1; x++) row[x] = c;
}

void pxRect(int x0, int y0, int x1, int y1, uint32_t c) {
  if (y0 < clipY0) y0 = clipY0;
  if (y1 > clipY1) y1 = clipY1;
  for (int y = y0; y <= y1; y++) pxSpan(y, x0, x1, c);
}

void pxPoly(const float *xs, const float *ys, int n, uint32_t c) {
  if (n < 3) return;
  float minY = ys[0], maxY = ys[0];
  for (int i = 1; i < n; i++) {
    if (ys[i] < minY) minY = ys[i];
    if (ys[i] > maxY) maxY = ys[i];
  }
  int y0 = edgeLo(minY), y1 = edgeLo(maxY) - 1;
  if (y0 < clipY0) y0 = clipY0;
  if (y1 > clipY1) y1 = clipY1;
  float cross[40];
  for (int y = y0; y <= y1; y++) {
    float fy = y + 0.5f;
    int k = 0;
    for (int i = 0; i < n && k < 40; i++) {
      int j = (i + 1 == n) ? 0 : i + 1;
      float a = ys[i], b = ys[j];
      if ((a <= fy && b > fy) || (b <= fy && a > fy)) cross[k++] = xs[i] + (fy - a) * (xs[j] - xs[i]) / (b - a);
    }
    for (int a = 1; a < k; a++) {
      float v = cross[a];
      int b = a - 1;
      while (b >= 0 && cross[b] > v) { cross[b + 1] = cross[b]; b--; }
      cross[b + 1] = v;
    }
    for (int a = 0; a + 1 < k; a += 2) pxSpan(y, edgeLo(cross[a]), edgeLo(cross[a + 1]) - 1, c);
  }
}

void pxCircle(float cx, float cy, float r, uint32_t c) {
  if (r <= 0) return;
  int y0 = edgeLo(cy - r), y1 = edgeLo(cy + r) - 1;
  if (y0 < clipY0) y0 = clipY0;
  if (y1 > clipY1) y1 = clipY1;
  for (int y = y0; y <= y1; y++) {
    float dy = y + 0.5f - cy, h = r * r - dy * dy;
    if (h < 0) continue;
    float dx = sqrtf(h);
    pxSpan(y, edgeLo(cx - dx), edgeLo(cx + dx) - 1, c);
  }
}

void pxLine(float x0, float y0, float x1, float y1, float w, uint32_t c) {
  float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
  float h = w * 0.5f;
  if (len < 0.001f) {
    pxRect(edgeLo(x0 - h), edgeLo(y0 - h), edgeLo(x0 + h) - 1, edgeLo(y0 + h) - 1, c);
    return;
  }
  float ux = dx / len * h, uy = dy / len * h;   // along, half the width long
  float nx = -uy, ny = ux;                      // across
  float xs[4] = { x0 - ux + nx, x1 + ux + nx, x1 + ux - nx, x0 - ux - nx };
  float ys[4] = { y0 - uy + ny, y1 + uy + ny, y1 + uy - ny, y0 - uy - ny };
  pxPoly(xs, ys, 4, c);
}

void pxBlend(int x0, int y0, int x1, int y1, int r, int g, int b, float a) {
  if (x0 < 0) x0 = 0;
  if (x1 >= FB_W) x1 = FB_W - 1;
  if (y0 < clipY0) y0 = clipY0;
  if (y1 > clipY1) y1 = clipY1;
  int k = (int)(clampv(a, 0.0f, 1.0f) * 256);
  for (int y = y0; y <= y1; y++) {
    uint32_t *row = gfxTarget + (size_t)y * FB_W;
    for (int x = x0; x <= x1; x++) {
      uint32_t p = row[x];
      int pr = p & 255, pg = (p >> 8) & 255, pb = (p >> 16) & 255;
      pr += ((r - pr) * k) >> 8;
      pg += ((g - pg) * k) >> 8;
      pb += ((b - pb) * k) >> 8;
      row[x] = rgba(pr, pg, pb);
    }
  }
}

// ============================================================ unit space
void fillRect(int x0, int y0, int x1, int y1, uint16_t c) {
  if (x1 < x0 || y1 < y0) return;
  pxRect(edgeLo(x0 * UNIT_K), edgeLo(y0 * UNIT_K), edgeLo((x1 + 1) * UNIT_K) - 1, edgeLo((y1 + 1) * UNIT_K) - 1, c32(c));
}

void fillRectF(float x0, float y0, float x1, float y1, uint16_t c) {
  if (x1 <= x0 || y1 <= y0) return;
  pxRect(edgeLo(x0 * UNIT_K), edgeLo(y0 * UNIT_K), edgeLo(x1 * UNIT_K) - 1, edgeLo(y1 * UNIT_K) - 1, c32(c));
}

void fillPolygon(const int *xs, const int *ys, int n, uint16_t c) {
  float fx[40], fy[40];
  if (n > 40) n = 40;
  for (int i = 0; i < n; i++) {
    fx[i] = xs[i] * UNIT_K;
    fy[i] = ys[i] * UNIT_K;
  }
  pxPoly(fx, fy, n, c32(c));
}

void fillPolygonF(const float *xs, const float *ys, int n, uint16_t c) {
  float fx[40], fy[40];
  if (n > 40) n = 40;
  for (int i = 0; i < n; i++) {
    fx[i] = xs[i] * UNIT_K;
    fy[i] = ys[i] * UNIT_K;
  }
  pxPoly(fx, fy, n, c32(c));
}

void fillCircle(int cx, int cy, int r, uint16_t c) {
  pxCircle((cx + 0.5f) * UNIT_K, (cy + 0.5f) * UNIT_K, (r + 0.5f) * UNIT_K, c32(c));
}

void drawCircle(int cx, int cy, int r, uint16_t c) {
  const int n = 8 + r * 2 > 64 ? 64 : 8 + r * 2;
  float px = cx + 0.5f + r, py = cy + 0.5f;
  for (int k = 1; k <= n; k++) {
    float a = TWO_PI * k / n;
    float qx = cx + 0.5f + cosf(a) * r, qy = cy + 0.5f + sinf(a) * r;
    drawLineF(px, py, qx, qy, c, 1.0f);
    px = qx;
    py = qy;
  }
}

void drawLine(int x0, int y0, int x1, int y1, uint16_t c, bool thick) {
  float o = thick ? 1.0f : 0.5f;   // (the sketch's thick lines are a 2x2 brush: centred half a unit on)
  pxLine((x0 + o) * UNIT_K, (y0 + o) * UNIT_K, (x1 + o) * UNIT_K, (y1 + o) * UNIT_K, (thick ? 2.0f : 1.0f) * UNIT_K, c32(c));
}

void drawLineF(float x0, float y0, float x1, float y1, uint16_t c, float w) {
  pxLine(x0 * UNIT_K, y0 * UNIT_K, x1 * UNIT_K, y1 * UNIT_K, w * UNIT_K, c32(c));
}

void drawPixel(int x, int y, uint16_t c) { fillRect(x, y, x, y, c); }

void drawRectOutline(int x0, int y0, int x1, int y1, uint16_t c, int w) {
  fillRect(x0, y0, x1, y0 + w - 1, c);
  fillRect(x0, y1 - w + 1, x1, y1, c);
  fillRect(x0, y0, x0 + w - 1, y1, c);
  fillRect(x1 - w + 1, y0, x1, y1, c);
}

void hazardBand(int x0, int x1, int y0, int y1) {
  uint16_t hz = rgb565(230, 186, 36), hk = rgb565(20, 20, 22);
  fillRect(x0, y0, x1, y1, hk);
  int h = y1 - y0 + 1;
  for (int x = x0 - 40; x < x1 + 40; x += 40) {
    int xs[4] = { x, x + 20, x + 20 + h, x + h };
    int ys[4] = { y0, y0, y1 + 1, y1 + 1 };
    for (int k = 0; k < 4; k++) xs[k] = clampv(xs[k], x0, x1 + 1);
    fillPolygon(xs, ys, 4, hz);
  }
}

void blendRect(int x0, int y0, int x1, int y1, int r, int g, int b, float a) {
  pxBlend(edgeLo(x0 * UNIT_K), edgeLo(y0 * UNIT_K), edgeLo((x1 + 1) * UNIT_K) - 1, edgeLo((y1 + 1) * UNIT_K) - 1, r, g, b, a);
}

// ============================================================ vector letters (from the sketch)
static const char *vectorGlyph(char ch) {
  switch (ch) {
    case 'A': return "0602204246|0444";
    case 'B': return "003041423303|334445360600";
    case 'C': return "4130100105163645";
    case 'D': return "00204244260600";
    case 'E': return "40000646|0333";
    case 'F': return "400006|0333";
    case 'G': return "41301001051636454323";
    case 'H': return "0006|4046|0343";
    case 'I': return "1030|2026|1636";
    case 'J': return "4045361605";
    case 'K': return "0006|4004|1346";
    case 'L': return "000646";
    case 'M': return "0600224046";
    case 'N': return "06004640";
    case 'O': return "103041453616050110";
    case 'P': return "06003041423303";
    case 'Q': return "103041453616050110|2446";
    case 'R': return "06003041423303|2346";
    case 'S': return "41301001021233344445361605";
    case 'T': return "0040|2026";
    case 'U': return "000516364540";
    case 'V': return "002640";
    case 'W': return "0016233640";
    case 'X': return "0046|4006";
    case 'Y': return "002240|2226";
    case 'Z': return "00400646";
    case '0': return "103041453616050110|4105";
    case '1': return "112026|1636";
    case '2': return "01103041420646";
    case '3': return "004022324345361605";
    case '4': return "36300444";
    case '5': return "4000023243453606";
    case '6': return "30100105163645443303";
    case '7': return "004016";
    case '8': return "10304142331304051636454433|13020110";
    case '9': return "43130201103041453616";
    case '-': return "1333";
    case '+': return "2125|0343";
    case '/': return "0640";
    case '!': return "2024|2526";
    case ',': return "252617";
    case '.': return "2526";
    case '\'': return "2021";
    case ':': return "2122|2425";
    case '>': return "113415";
    case '<': return "411445";
    case '?': return "0110304142332324|2626";
    case '=': return "0242|0444";
    case '(': return "30121436";
    case ')': return "10323416";
    default:  return "";
  }
}

float vectorTextWidth(const char *text, float size) {
  int n = (int)strlen(text);
  return (n > 0 ? n * 6 - 2 : 0) * size;
}

// Centred on (cx, cy), cy the middle of the capitals. size: units per glyph step.
void drawVectorText(const char *text, float cx, float cy, float size, uint16_t c, bool thick) {
  int n = (int)strlen(text);
  int units = n > 0 ? n * 6 - 2 : 0;
  float w = thick ? 2.0f : 1.0f;
  if (size < 2.0f) w = thick ? 1.4f : 0.9f;
  for (int i = 0; i < n; i++) {
    const char *g = vectorGlyph(text[i]);
    float left = cx + (i * 6 - units * 0.5f) * size;
    bool penDown = false;
    float px = 0, py = 0;
    for (int k = 0; g[k];) {
      if (g[k] == '|') {
        penDown = false;
        k++;
        continue;
      }
      float sx = left + (g[k] - '0') * size;
      float sy = cy + (g[k + 1] - '0' - 3) * size;
      k += 2;
      if (penDown) drawLineF(px, py, sx, sy, c, w);
      else if (g[k] == 0 || g[k] == '|') drawLineF(sx, sy, sx, sy, c, w);   // (a dot)
      px = sx;
      py = sy;
      penDown = true;
    }
  }
}

// ============================================================ the agent
static void agPolyPx(const int *xs, const int *ys, int n, uint16_t c) {
  float fx[16], fy[16];
  if (n > 16) n = 16;
  for (int i = 0; i < n; i++) {
    fx[i] = (float)xs[i];
    fy[i] = (float)ys[i];
  }
  pxPoly(fx, fy, n, c32(c));
}
static void agCirclePx(int cx, int cy, int r, uint16_t c) { pxCircle(cx + 0.5f, cy + 0.5f, r + 0.5f, c32(c)); }

void drawAgentU(const AgentPose &p, float x, float floorY, int facing, float scale) {
  agentDraw(p, x * UNIT_K, floorY * UNIT_K, facing, scale * UNIT_K, agPolyPx, agCirclePx);
}
