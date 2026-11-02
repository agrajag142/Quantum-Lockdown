// agent_rig.h
// The player character for the silo game: a saboteur in filled vectors, built on a
// small skeleton, so every pose and animation is just a handful of angles.
//
//   hip (the root) -> torso -> neck -> head
//                            -> shoulder -> upper arm -> forearm -> hand
//                  -> thigh -> shin -> foot          (near and far side)
//
// Each body part is a filled polygon hung on its bone. A pose (AgentPose) is
// a list of joint angles; an animation is a few key poses blended in time.
// The whole figure can also be turned round its middle (the somersault).
//
// No Arduino code in here: the sketch passes in its own fillPolygon() and
// fillCircle(), so the same file also runs in a PC preview tool.
//
// Sizes are in pixels at scale 1: about 100 px from boot sole to helmet top.

#ifndef AGENT_RIG_H
#define AGENT_RIG_H

#include <math.h>
#include <stdint.h>

// ---------- Colours (RGB565) ----------
static constexpr uint16_t agentRgb(int r, int g, int b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
// near side / far side (the far limbs are darker, for depth)
#define AG_SUIT        agentRgb(74, 80, 96)      // charcoal night suit
#define AG_SUIT_FAR    agentRgb(42, 45, 56)
#define AG_SUIT_SHADE  agentRgb(56, 60, 74)      // torso back panel
#define AG_PAD         agentRgb(108, 112, 122)   // knee pads
#define AG_PAD_FAR     agentRgb(64, 66, 74)
#define AG_STRAP       agentRgb(150, 124, 80)    // webbing: harness, wrist straps, holster
#define AG_STRAP_FAR   agentRgb(90, 74, 50)
#define AG_BELT        agentRgb(104, 86, 58)
#define AG_POUCH       agentRgb(124, 104, 70)
#define AG_BUCKLE      agentRgb(170, 170, 160)
#define AG_BOOT        agentRgb(30, 30, 34)
#define AG_BOOT_FAR    agentRgb(18, 18, 22)
#define AG_GLOVE       agentRgb(36, 36, 40)
#define AG_GLOVE_FAR   agentRgb(22, 22, 26)
#define AG_PACK        agentRgb(110, 92, 62)     // satchel of tools and charges
#define AG_PACK_DARK   agentRgb(80, 66, 44)
#define AG_MASK        agentRgb(42, 44, 52)      // balaclava
#define AG_MASK_SH     agentRgb(28, 29, 34)
#define AG_MASK_HI     agentRgb(64, 68, 80)      // light catching the top of the head
#define AG_SKIN        agentRgb(196, 150, 118)   // round the eyes
#define AG_GOGGLE      agentRgb(64, 66, 72)      // night vision goggles
#define AG_LENS        agentRgb(90, 255, 140)

// ---------- Pose ----------
// All angles in degrees. Facing right; the sketch mirrors for facing left.
//   torso:  lean, + = forward            head:  nod, + = forward/down
//   arm:    upper arm from the torso line, 0 = hanging down, + = swung forward
//   elbow:  + = bends forward (the natural way)
//   leg:    thigh from straight down, + = swung forward
//   knee:   - = bends back (the natural way)
//   foot:   + = toes up
//   spin:   whole figure turned round its middle, + = forward roll
//   plant:  1 = the lowest boot (or knee) is put on the floor line,
//           0 = the hip floats at standing height (in the air)
//   lift:   extra height above that (pixels, + = up)
struct AgentPose {
  float torso, head;
  float armN, elbowN, armF, elbowF;
  float legN, kneeN, legF, kneeF;
  float footN, footF;
  float spin, plant, lift;
};

// ---------- Body measurements (pixels at scale 1) ----------
#define AG_TORSO_LEN   32.0f
#define AG_SHOULDER_AT 27.0f    // shoulder joint, up the torso from the hip
#define AG_NECK_LEN    11.0f    // neck to the centre of the helmet
#define AG_HEAD_R      10.5f
#define AG_UPPER_ARM   17.0f
#define AG_FOREARM     15.0f
#define AG_HAND_R      3.6f
#define AG_THIGH       23.0f
#define AG_SHIN        22.0f
#define AG_STAND_H     48.0f    // hip height above the floor, standing straight
#define AG_SPIN_PIVOT  12.0f    // the somersault turns round this point above the hip

// ---------- Drawing hooks (provided by the sketch) ----------
typedef void (*AgentFillPolyFn)(const int *xs, const int *ys, int n, uint16_t c);
typedef void (*AgentFillCircleFn)(int cx, int cy, int r, uint16_t c);

// ---------- Small maths ----------
static inline float agRad(float d) { return d * 0.017453293f; }
// Direction of a bone at angle a (degrees): 0 = straight down, +90 = forward (+x)
static inline void agDir(float a, float &dx, float &dy) {
  float r = agRad(a);
  dx = sinf(r);
  dy = cosf(r);
}

// Where a point of the figure lands on screen
struct AgentXf {
  float ox, oy;      // screen position of the figure's root (the hip)
  float pivotY;      // spin pivot, in figure units (up from the hip is negative)
  float c, s;        // spin
  float mir;         // +1 facing right, -1 facing left
  float sc;          // scale
  inline void apply(float x, float y, int &sx, int &sy) const {
    float py = y - pivotY;
    float rx = x * c - py * s;
    float ry = x * s + py * c + pivotY;
    sx = (int)lroundf(ox + mir * rx * sc);
    sy = (int)lroundf(oy + ry * sc);
  }
};

// A limb segment: a capsule from A (half width ra) to B (half width rb)
static void agCapsule(const AgentXf &xf, AgentFillPolyFn fill, float ax, float ay, float bx, float by,
                      float ra, float rb, uint16_t col) {
  float dx = bx - ax, dy = by - ay;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.001f) { dx = 0; dy = 1; } else { dx /= len; dy /= len; }
  float nx = -dy, ny = dx;
  int xs[10], ys[10];
  // five points round each end: A's cap goes round the back, B's round the front
  static const float cs[5] = { 0.0f, -0.7071f, -1.0f, -0.7071f, 0.0f };
  static const float sn[5] = { 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f };
  for (int k = 0; k < 5; k++) {
    float vx = dx * cs[k] + nx * sn[k], vy = dy * cs[k] + ny * sn[k];
    xf.apply(ax + vx * ra, ay + vy * ra, xs[k], ys[k]);
  }
  for (int k = 0; k < 5; k++) {
    float vx = -dx * cs[k] - nx * sn[k], vy = -dy * cs[k] - ny * sn[k];
    xf.apply(bx + vx * rb, by + vy * rb, xs[5 + k], ys[5 + k]);
  }
  fill(xs, ys, 10, col);
}

// A shape given in a bone's own frame: u = along the bone, v = to the bone's
// left when looking along it (for the torso, which points up: v = forward)
static void agShape(const AgentXf &xf, AgentFillPolyFn fill, float ox, float oy, float ux, float uy,
                    const float (*pts)[2], int n, uint16_t col) {
  int xs[16], ys[16];
  if (n > 16) n = 16;
  float vx = -uy, vy = ux;   // u turned a quarter: for u pointing up, v points forward (+x)
  for (int i = 0; i < n; i++) {
    float u = pts[i][0], v = pts[i][1];
    xf.apply(ox + ux * u + vx * v, oy + uy * u + vy * v, xs[i], ys[i]);
  }
  fill(xs, ys, n, col);
}

// A band across a limb, from t0 to t1 along A->B (0..1), half widths ra->rb
static void agBand(const AgentXf &xf, AgentFillPolyFn fill, float ax, float ay, float bx, float by,
                   float ra, float rb, float t0, float t1, uint16_t col) {
  float dx = bx - ax, dy = by - ay;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.001f) return;
  float nx = -dy / len, ny = dx / len;
  float w0 = (ra + (rb - ra) * t0) * 1.04f, w1 = (ra + (rb - ra) * t1) * 1.04f;
  float p0x = ax + dx * t0, p0y = ay + dy * t0, p1x = ax + dx * t1, p1y = ay + dy * t1;
  int xs[4], ys[4];
  xf.apply(p0x + nx * w0, p0y + ny * w0, xs[0], ys[0]);
  xf.apply(p1x + nx * w1, p1y + ny * w1, xs[1], ys[1]);
  xf.apply(p1x - nx * w1, p1y - ny * w1, xs[2], ys[2]);
  xf.apply(p0x - nx * w0, p0y - ny * w0, xs[3], ys[3]);
  fill(xs, ys, 4, col);
}

// ---------- The skeleton, worked out for one pose ----------
struct AgentJoints {
  float hipX, hipY;
  float neckX, neckY, shX, shY, headX, headY;
  float tdx, tdy;                     // torso direction (unit, pointing up the body)
  float hdx, hdy;                     // head direction (unit, pointing up out of the head)
  float elbX[2], elbY[2], wrX[2], wrY[2];   // [0] near, [1] far
  float kneeX[2], kneeY[2], ankX[2], ankY[2];
  float footA[2];                     // foot direction angle (degrees, agDir)
};

static void agentSolve(const AgentPose &p, AgentJoints &j) {
  j.hipX = 0;
  j.hipY = 0;
  float tAng = 180.0f - p.torso;            // up the body
  agDir(tAng, j.tdx, j.tdy);
  j.neckX = j.tdx * AG_TORSO_LEN;
  j.neckY = j.tdy * AG_TORSO_LEN;
  j.shX = j.tdx * AG_SHOULDER_AT;
  j.shY = j.tdy * AG_SHOULDER_AT;
  agDir(tAng - p.head, j.hdx, j.hdy);
  j.headX = j.neckX + j.hdx * AG_NECK_LEN;
  j.headY = j.neckY + j.hdy * AG_NECK_LEN;

  float arm[2] = { p.armN, p.armF }, elb[2] = { p.elbowN, p.elbowF };
  float leg[2] = { p.legN, p.legF }, knee[2] = { p.kneeN, p.kneeF };
  float foot[2] = { p.footN, p.footF };
  for (int s = 0; s < 2; s++) {
    float dx, dy;
    float ua = -p.torso + arm[s];           // upper arm: from "hanging along the torso"
    agDir(ua, dx, dy);
    j.elbX[s] = j.shX + dx * AG_UPPER_ARM;
    j.elbY[s] = j.shY + dy * AG_UPPER_ARM;
    agDir(ua + elb[s], dx, dy);
    j.wrX[s] = j.elbX[s] + dx * AG_FOREARM;
    j.wrY[s] = j.elbY[s] + dy * AG_FOREARM;

    agDir(leg[s], dx, dy);
    j.kneeX[s] = dx * AG_THIGH;
    j.kneeY[s] = dy * AG_THIGH;
    float shin = leg[s] + knee[s];
    agDir(shin, dx, dy);
    j.ankX[s] = j.kneeX[s] + dx * AG_SHIN;
    j.ankY[s] = j.kneeY[s] + dy * AG_SHIN;
    j.footA[s] = shin + 90.0f + foot[s];
  }
}

// Boot outline in the foot's frame: u = towards the toe, v = down towards the
// sole (ankle at the origin)
static const float AG_BOOT_PTS[][2] = {
  { -3.5f, -4.0f }, { -4.8f, 0.0f }, { -4.5f, 4.6f }, { 10.0f, 4.6f },
  { 11.8f, 2.6f }, { 10.8f, 0.2f }, { 5.0f, -1.8f }, { 2.5f, -4.2f },
};
#define AG_BOOT_N 8

// Lowest point of the figure (largest y), hip at 0, before any spin.
// Boot soles and knees count, so kneeling works too.
static float agentLowest(const AgentJoints &j) {
  float low = -1000.0f;
  for (int s = 0; s < 2; s++) {
    float fx, fy;
    agDir(j.footA[s], fx, fy);
    // the boot frame, as agShape() uses it: u = (fx, fy), v = (-fy, fx)
    for (int k = 0; k < AG_BOOT_N; k++) {
      float u = AG_BOOT_PTS[k][0], v = AG_BOOT_PTS[k][1];
      float y = j.ankY[s] + fy * u + fx * v;
      if (y > low) low = y;
    }
    float ky = j.kneeY[s] + 4.2f;
    if (ky > low) low = ky;
  }
  return low;
}

// ---------- Draw the figure ----------
// (x, floorY): the spot on the floor under the figure. facing: +1 right, -1 left.
static void agentDraw(const AgentPose &p, float x, float floorY, int facing, float scale,
                      AgentFillPolyFn fillPoly, AgentFillCircleFn fillCircle) {
  AgentJoints j;
  agentSolve(p, j);

  // where the hip goes: planted on the floor, or floating at standing height
  float planted = -agentLowest(j);
  float hipUp = p.plant * planted + (1.0f - p.plant) * (-AG_STAND_H) - p.lift;

  AgentXf xf;
  xf.ox = x;
  xf.oy = floorY + hipUp * scale;
  xf.pivotY = -AG_SPIN_PIVOT;
  xf.c = cosf(agRad(p.spin));
  xf.s = sinf(agRad(p.spin));
  xf.mir = facing < 0 ? -1.0f : 1.0f;
  xf.sc = scale;

  // ---- far side first ----
  // far arm
  agCapsule(xf, fillPoly, j.shX, j.shY, j.elbX[1], j.elbY[1], 3.6f, 3.1f, AG_SUIT_FAR);
  agCapsule(xf, fillPoly, j.elbX[1], j.elbY[1], j.wrX[1], j.wrY[1], 3.1f, 2.7f, AG_SUIT_FAR);
  {
    int hx, hy;
    float dx = j.wrX[1] - j.elbX[1], dy = j.wrY[1] - j.elbY[1];
    float l = sqrtf(dx * dx + dy * dy);
    if (l > 0.01f) { dx /= l; dy /= l; }
    xf.apply(j.wrX[1] + dx * 2.0f, j.wrY[1] + dy * 2.0f, hx, hy);
    fillCircle(hx, hy, (int)lroundf(AG_HAND_R * scale), AG_GLOVE_FAR);
  }
  // far leg
  agCapsule(xf, fillPoly, 0, 0, j.kneeX[1], j.kneeY[1], 5.0f, 4.0f, AG_SUIT_FAR);
  agCapsule(xf, fillPoly, j.kneeX[1], j.kneeY[1], j.ankX[1], j.ankY[1], 4.0f, 3.4f, AG_SUIT_FAR);
  agBand(xf, fillPoly, j.kneeX[1], j.kneeY[1], j.ankX[1], j.ankY[1], 4.0f, 3.4f, -0.04f, 0.22f, AG_PAD_FAR);
  {
    float fx, fy;
    agDir(j.footA[1], fx, fy);
    agShape(xf, fillPoly, j.ankX[1], j.ankY[1], fx, fy, AG_BOOT_PTS, AG_BOOT_N, AG_BOOT_FAR);
  }

  // ---- body ----
  // satchel on the back (u = up the torso, v = forward)
  static const float PACK[][2] = {
    { 9.0f, -5.5f }, { 9.0f, -12.5f }, { 11.0f, -14.0f }, { 25.0f, -14.0f },
    { 27.0f, -12.5f }, { 27.0f, -5.5f },
  };
  agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, PACK, 6, AG_PACK);
  static const float PACK_FLAP[][2] = {
    { 20.0f, -13.6f }, { 26.6f, -13.6f }, { 26.6f, -6.0f }, { 20.0f, -6.0f },
  };
  agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, PACK_FLAP, 4, AG_PACK_DARK);

  // torso
  static const float TORSO[][2] = {
    { -3.0f, -6.0f }, { 6.0f, -7.0f }, { 16.0f, -7.0f }, { 26.0f, -6.5f }, { 31.0f, -4.5f },
    { 33.5f, -1.0f }, { 33.0f, 3.0f }, { 29.0f, 6.5f }, { 21.0f, 7.5f }, { 12.0f, 6.5f },
    { 4.0f, 6.0f }, { -3.0f, 6.0f },
  };
  agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, TORSO, 12, AG_SUIT);
  // shaded back panel, for a rounded look
  static const float TORSO_BACK[][2] = {
    { 2.0f, -6.6f }, { 16.0f, -7.0f }, { 26.0f, -6.5f }, { 30.5f, -4.6f }, { 26.0f, -3.2f }, { 14.0f, -3.6f }, { 2.0f, -3.4f },
  };
  agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, TORSO_BACK, 7, AG_SUIT_SHADE);
  // harness strap: from the back of the shoulder down across the chest
  static const float HARNESS[][2] = {
    { 30.0f, -6.0f }, { 32.6f, -3.0f }, { 12.0f, 7.0f }, { 8.5f, 6.4f },
  };
  agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, HARNESS, 4, AG_STRAP);
  // belt and buckle
  static const float BELT[][2] = {
    { 1.0f, -6.4f }, { 5.0f, -6.6f }, { 5.0f, 6.2f }, { 1.0f, 6.2f },
  };
  agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BELT, 4, AG_BELT);
  static const float BUCKLE[][2] = {
    { 1.4f, 3.2f }, { 4.6f, 3.2f }, { 4.6f, 6.4f }, { 1.4f, 6.4f },
  };
  agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BUCKLE, 4, AG_BUCKLE);
  static const float BELT_POUCH[][2] = {   // pouch on the belt, at the back
    { -1.0f, -7.4f }, { 7.0f, -7.6f }, { 7.0f, -2.6f }, { -1.0f, -2.6f },
  };
  agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BELT_POUCH, 4, AG_POUCH);

  // ---- near leg ----
  agCapsule(xf, fillPoly, 0, 0, j.kneeX[0], j.kneeY[0], 5.2f, 4.2f, AG_SUIT);
  agCapsule(xf, fillPoly, j.kneeX[0], j.kneeY[0], j.ankX[0], j.ankY[0], 4.2f, 3.5f, AG_SUIT);
  agBand(xf, fillPoly, j.kneeX[0], j.kneeY[0], j.ankX[0], j.ankY[0], 4.2f, 3.5f, -0.04f, 0.22f, AG_PAD);
  // holster pouch strapped to the thigh
  agBand(xf, fillPoly, 0, 0, j.kneeX[0], j.kneeY[0], 5.2f, 4.2f, 0.40f, 0.70f, AG_POUCH);
  agBand(xf, fillPoly, 0, 0, j.kneeX[0], j.kneeY[0], 5.2f, 4.2f, 0.46f, 0.52f, AG_STRAP);
  {
    float fx, fy;
    agDir(j.footA[0], fx, fy);
    agShape(xf, fillPoly, j.ankX[0], j.ankY[0], fx, fy, AG_BOOT_PTS, AG_BOOT_N, AG_BOOT);
  }

  // ---- head: balaclava, eye slit, night vision goggles ----
  {
    int cx, cy;
    xf.apply(j.headX, j.headY, cx, cy);
    int r = (int)lroundf((AG_HEAD_R - 0.8f) * scale);
    fillCircle(cx, cy, r, AG_MASK);
    // head frame: u = up out of the head, v = forward
    static const float NAPE[][2] = {   // shading at the back of the head
      { 4.0f, -8.6f }, { -2.0f, -9.6f }, { -6.5f, -7.0f }, { -9.4f, -2.0f },
      { -6.0f, -2.4f }, { -2.0f, -5.0f },
    };
    agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, NAPE, 6, AG_MASK_SH);
    static const float CROWN[][2] = {  // highlight over the crown, so the head reads on dark walls
      { 9.0f, -3.5f }, { 9.4f, 2.0f }, { 8.0f, 5.5f }, { 6.6f, 3.0f }, { 7.0f, -3.0f }, { 5.6f, -6.4f }, { 7.0f, -6.0f },
    };
    agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, CROWN, 7, AG_MASK_HI);
    static const float NECK[][2] = {   // the mask runs down into the collar
      { -4.0f, -4.0f }, { -4.0f, 4.0f }, { -12.0f, 4.5f }, { -12.0f, -5.0f },
    };
    agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, NECK, 4, AG_MASK);
    static const float EYES[][2] = {   // skin showing through the eye slit
      { 3.8f, 3.0f }, { 4.0f, 9.6f }, { 0.4f, 9.6f }, { 0.4f, 3.0f },
    };
    agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, EYES, 4, AG_SKIN);
    static const float STRAP[][2] = {  // goggle strap round the head
      { 4.4f, -9.2f }, { 4.6f, 4.0f }, { 1.8f, 4.0f }, { 1.6f, -9.4f },
    };
    agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, STRAP, 4, AG_GOGGLE);
    static const float GOGGLE[][2] = {
      { 5.4f, 5.0f }, { 4.6f, 13.6f }, { 0.4f, 13.6f }, { -0.6f, 5.6f },
    };
    agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, GOGGLE, 4, AG_GOGGLE);
    static const float LENS[][2] = {
      { 4.0f, 12.2f }, { 3.8f, 14.6f }, { 1.2f, 14.6f }, { 1.0f, 12.2f },
    };
    agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, LENS, 4, AG_LENS);
  }

  // ---- near arm, last (in front of the body) ----
  agCapsule(xf, fillPoly, j.shX, j.shY, j.elbX[0], j.elbY[0], 3.8f, 3.2f, AG_SUIT);
  agCapsule(xf, fillPoly, j.elbX[0], j.elbY[0], j.wrX[0], j.wrY[0], 3.2f, 2.8f, AG_SUIT);
  agBand(xf, fillPoly, j.elbX[0], j.elbY[0], j.wrX[0], j.wrY[0], 3.2f, 2.8f, 0.70f, 0.86f, AG_STRAP);
  {
    int hx, hy;
    float dx = j.wrX[0] - j.elbX[0], dy = j.wrY[0] - j.elbY[0];
    float l = sqrtf(dx * dx + dy * dy);
    if (l > 0.01f) { dx /= l; dy /= l; }
    xf.apply(j.wrX[0] + dx * 2.0f, j.wrY[0] + dy * 2.0f, hx, hy);
    fillCircle(hx, hy, (int)lroundf(AG_HAND_R * scale), AG_GLOVE);
  }
}

// ---------- Animations ----------
enum AgentAnim : uint8_t { ANIM_IDLE, ANIM_RUN, ANIM_JUMP, ANIM_USE, ANIM_FALL, ANIM_DEAD, ANIM_COUNT };

#define AG_RUN_CYCLE    0.62f   // seconds for two strides (left + right)
#define AG_JUMP_TIME    0.95f   // the whole somersault, crouch to landing
#define AG_JUMP_UP      0.13f   // leaves the floor at this time...
#define AG_JUMP_DOWN    0.84f   // ...and lands at this one
#define AG_JUMP_HEIGHT  62.0f   // how high the hip goes (pixels at scale 1)

static inline float agLerp(float a, float b, float t) { return a + (b - a) * t; }

static void agentLerp(const AgentPose &a, const AgentPose &b, float t, AgentPose &o) {
  const float *pa = &a.torso, *pb = &b.torso;
  float *po = &o.torso;
  const int n = sizeof(AgentPose) / sizeof(float);
  for (int i = 0; i < n; i++) po[i] = agLerp(pa[i], pb[i], t);
  // spin: the short way round (360 is the same as 0)
  float d = fmodf(b.spin - a.spin, 360.0f);
  if (d > 180.0f) d -= 360.0f;
  if (d < -180.0f) d += 360.0f;
  o.spin = a.spin + d * t;
}

// Swap the near and far limbs (the second half of the run cycle)
static void agentSwapSides(AgentPose &p) {
  float t;
  t = p.armN; p.armN = p.armF; p.armF = t;
  t = p.elbowN; p.elbowN = p.elbowF; p.elbowF = t;
  t = p.legN; p.legN = p.legF; p.legF = t;
  t = p.kneeN; p.kneeN = p.kneeF; p.kneeF = t;
  t = p.footN; p.footN = p.footF; p.footF = t;
}

//                       torso head  armN elbN  armF elbF  legN  kneeN  legF  kneeF  footN footF spin plant lift
static const AgentPose AG_STAND =
                       {  2,    0,   -4,  12,    5,  10,    3,   -3,   -3,   -2,     0,    0,   0,   1,    0 };

// Run: four key poses for one stride; the next stride is the same with the
// sides swapped. Contact, down, passing, up.
static const AgentPose AG_RUN_KEYS[4] = {
  //torso head  armN elbN  armF elbF  legN  kneeN  legF  kneeF  footN footF spin plant lift
  {  14,   -6,  -45,  75,   42,  80,   38,  -12,  -30,  -55,    12,  -25,   0,   1,   0   },  // near heel strikes
  {  16,   -6,  -25,  80,   25,  85,   18,  -38,  -12,  -95,     0,  -30,   0,   1,  -3   },  // weight down
  {  15,   -6,   -2,  85,    0,  90,   -8,  -22,   28, -105,     0,  -10,   0,   1,  -1   },  // passing
  {  13,   -6,   35,  80,  -38,  75,  -32,  -18,   52,  -45,   -20,   10,   0,   1,   6   },  // push off: both boots up
};

static const AgentPose AG_USE_BASE =
  //torso head  armN elbN  armF elbF  legN  kneeN  legF  kneeF  footN footF spin plant lift
  {  30,   18,   72,  28,   62,  38,   72, -112,   -6, -100,    10,  -40,   0,   1,   0   };

// Falling: arms thrown up, legs bent, flailing (plant 0: the sketch moves him)
static const AgentPose AG_FALL_BASE =
  //torso head  armN elbN  armF elbF  legN  kneeN  legF  kneeF  footN footF spin plant lift
  {   6,  -18,  150,  30,  128,  40,   32,  -62,  -12,  -48,     0,  -10,   0,   0,   0   };

// Knocked out after a long drop: crumpled on his knees, head down
static const AgentPose AG_DEAD_POSE =
  //torso head  armN elbN  armF elbF  legN  kneeN  legF  kneeF  footN footF spin plant lift
  {  72,   35,   25,  10,   18,  15,   96, -140,   18, -118,     0,  -35,   0,   1,   0   };

// Somersault keys: time (seconds), pose
struct AgentKey { float t; AgentPose p; };
static const AgentKey AG_JUMP_KEYS[] = {
  //        torso head  armN elbN  armF elbF  legN  kneeN  legF  kneeF  footN footF spin  plant lift
  { 0.00f, {  24,    8,  -50,  25,  -40,  25,   48,  -82,   36,  -78,     0,    0,   0,   1,   0 } },  // crouch
  { 0.13f, {  12,   -8,  150,  10,  140,  15,    0,   -6,  -14,  -10,   -35,  -40,   0,   1,   0 } },  // spring up
  { 0.24f, {  38,   22,   70, 100,   60, 105,  112, -140,  100, -138,    10,   10,  40,   0,   0 } },  // tucked
  { 0.62f, {  40,   22,   70, 105,   60, 108,  115, -142,  104, -140,    10,   10, 320,   0,   0 } },  // still tucked, nearly round
  { 0.76f, {  18,    0,   95,  20,   80,  25,   34,  -45,   14,  -35,     0,    0, 360,   0,   0 } },  // opening out
  { 0.95f, {  22,    6,   38,  35,   30,  40,   42,  -78,   32,  -72,     0,    0, 360,   1,   0 } },  // landing crouch
};
#define AG_JUMP_KEY_N (int)(sizeof(AG_JUMP_KEYS) / sizeof(AG_JUMP_KEYS[0]))

// The pose of an animation, t seconds after it started
static void agentSample(AgentAnim anim, float t, AgentPose &out) {
  switch (anim) {
    case ANIM_RUN: {
      float ph = fmodf(t / AG_RUN_CYCLE, 1.0f) * 8.0f;
      if (ph < 0) ph += 8.0f;
      int i = (int)ph;
      float f = ph - i;
      AgentPose a = AG_RUN_KEYS[i & 3], b = AG_RUN_KEYS[(i + 1) & 3];
      if (i >= 4) agentSwapSides(a);
      if (((i + 1) & 7) >= 4) agentSwapSides(b);
      agentLerp(a, b, f, out);
      break;
    }
    case ANIM_JUMP: {
      if (t <= 0) { out = AG_JUMP_KEYS[0].p; break; }
      if (t >= AG_JUMP_KEYS[AG_JUMP_KEY_N - 1].t) { out = AG_JUMP_KEYS[AG_JUMP_KEY_N - 1].p; out.spin = 0; break; }
      int k = 0;
      while (k + 1 < AG_JUMP_KEY_N && AG_JUMP_KEYS[k + 1].t <= t) k++;
      const AgentKey &a = AG_JUMP_KEYS[k], &b = AG_JUMP_KEYS[k + 1];
      float f = (t - a.t) / (b.t - a.t);
      f = f * f * (3.0f - 2.0f * f);
      agentLerp(a.p, b.p, f, out);
      out.spin = agLerp(a.p.spin, b.p.spin, f);   // the long way round, on purpose
      // the arc: off the floor between AG_JUMP_UP and AG_JUMP_DOWN
      if (t > AG_JUMP_UP && t < AG_JUMP_DOWN) {
        float u = (t - AG_JUMP_UP) / (AG_JUMP_DOWN - AG_JUMP_UP);
        out.lift = AG_JUMP_HEIGHT * sinf(u * 3.14159265f);
      }
      break;
    }
    case ANIM_USE: {
      out = AG_USE_BASE;
      float w = t * 9.0f;   // working at a panel: hands tapping, head nodding a little
      out.armN += 8.0f * sinf(w);
      out.elbowN += 10.0f * sinf(w + 1.0f);
      out.armF += 7.0f * sinf(w + 2.6f);
      out.elbowF += 9.0f * sinf(w + 3.4f);
      out.head += 3.0f * sinf(t * 2.3f);
      break;
    }
    case ANIM_FALL: {
      out = AG_FALL_BASE;
      float w = t * 14.0f;
      out.armN += 16.0f * sinf(w);
      out.armF += 14.0f * sinf(w + 2.2f);
      out.elbowN += 12.0f * sinf(w + 1.0f);
      out.legN += 10.0f * sinf(w * 0.8f);
      out.legF -= 10.0f * sinf(w * 0.8f);
      break;
    }
    case ANIM_DEAD:
      out = AG_DEAD_POSE;
      break;
    case ANIM_IDLE:
    default: {
      out = AG_STAND;
      float b = sinf(t * 2.2f);   // breathing
      out.torso += 1.5f * b;
      out.armN += 2.0f * b;
      out.armF -= 1.5f * b;
      out.head += 2.0f * sinf(t * 0.7f);
      out.lift = 0.6f * b;
      break;
    }
  }
}

#endif // AGENT_RIG_H
