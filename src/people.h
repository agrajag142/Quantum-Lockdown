// people.h
// The people on the launch control side of the base, drawn on the same
// skeleton as the agent (agent_rig.h): the same poses work for them.
//
//   PK_GUARD      Sergeant: camouflage uniform, helmet, body armour, rifle,
//                 three chevrons on the arm
//   PK_OFFICER    Captain:  flight suit, name patch, headset, pistol,
//                 two silver bars on the shoulder
//   PK_COMMANDER  Colonel:  blue dress uniform, peaked cap, ribbons, pistol,
//                 silver eagle on the shoulder
//   PK_SCIENTIST  The lab: white coat, ID badge on a lanyard, glasses
//   PK_STRIPPED   A scientist without his coat (shirt and trousers)
//   PK_DISGUISE   The agent in the scientist's coat and badge
//   PK_SECURITY   The lab's security guard: blue uniform, cap, badge, pistol
//   PK_SENIOR     A senior scientist: blue coat, grey hair, glasses, red badge
//   PK_ADVANCED   An advanced scientist (B2): lilac coat, white hair, glasses, red badge
//
// personDraw() also hands back where the weapon's muzzle is, so the sketch
// can put the bullet and the flash there. agentHand() does the same for the
// agent's pistol.

#ifndef PEOPLE_H
#define PEOPLE_H

#include "agent_rig.h"

enum PersonKind : uint8_t { PK_NONE, PK_GUARD, PK_OFFICER, PK_COMMANDER, PK_SCIENTIST, PK_STRIPPED, PK_DISGUISE, PK_SECURITY, PK_SCI_DIM, PK_SENIOR, PK_ADVANCED, PK_FIGHTER, PK_RADSUIT, PK_CHEF, PK_MAINT, PK_MAINT_UNDIES, PK_CLEANER };   // (QUANTUM LOCKDOWN: the kitchen's chef, the maintenance man, him in his vest and shorts)   // PK_FIGHTER: the guard, bare-headed, no rifle (SL1R4)

struct PersonLook {
  uint16_t suit, suitFar, shade, trouser, trouserFar, boot, bootFar, skin, skinFar, hair;
};

static const PersonLook PL_GUARD = {
  agentRgb(98, 106, 72), agentRgb(60, 66, 44), agentRgb(78, 86, 58), agentRgb(98, 106, 72), agentRgb(60, 66, 44),
  agentRgb(34, 30, 26), agentRgb(20, 18, 16), agentRgb(200, 156, 122), agentRgb(150, 112, 88), agentRgb(70, 52, 36) };
static const PersonLook PL_OFFICER = {
  agentRgb(118, 126, 96), agentRgb(72, 78, 58), agentRgb(96, 104, 78), agentRgb(118, 126, 96), agentRgb(72, 78, 58),
  agentRgb(36, 32, 28), agentRgb(22, 20, 18), agentRgb(214, 170, 136), agentRgb(160, 124, 98), agentRgb(96, 66, 40) };
static const PersonLook PL_COMMANDER = {
  agentRgb(46, 62, 108), agentRgb(28, 38, 70), agentRgb(36, 50, 90), agentRgb(40, 54, 96), agentRgb(24, 32, 62),
  agentRgb(20, 20, 24), agentRgb(10, 10, 14), agentRgb(206, 160, 128), agentRgb(154, 118, 94), agentRgb(170, 170, 176) };

static const PersonLook PL_SCIENTIST = {
  agentRgb(236, 238, 242), agentRgb(192, 196, 204), agentRgb(212, 216, 222), agentRgb(64, 66, 76), agentRgb(40, 42, 50),
  agentRgb(26, 24, 24), agentRgb(14, 14, 16), agentRgb(222, 178, 144), agentRgb(170, 132, 104), agentRgb(112, 76, 44) };
static const PersonLook PL_STRIPPED = {
  agentRgb(150, 184, 212), agentRgb(100, 128, 154), agentRgb(128, 162, 192), agentRgb(64, 66, 76), agentRgb(40, 42, 50),
  agentRgb(26, 24, 24), agentRgb(14, 14, 16), agentRgb(222, 178, 144), agentRgb(170, 132, 104), agentRgb(112, 76, 44) };
static const PersonLook PL_DISGUISE = {
  agentRgb(236, 238, 242), agentRgb(192, 196, 204), agentRgb(212, 216, 222), agentRgb(54, 56, 66), agentRgb(34, 36, 44),
  agentRgb(30, 30, 34), agentRgb(18, 18, 22), agentRgb(196, 150, 118), agentRgb(150, 112, 88), agentRgb(28, 26, 28) };

static const PersonLook PL_RADSUIT = {   // him in the radiation suit: yellow all over, the hood up (the sketch adds the visor)
  agentRgb(236, 196, 40), agentRgb(176, 142, 28), agentRgb(210, 172, 34), agentRgb(236, 196, 40), agentRgb(176, 142, 28),
  agentRgb(30, 30, 34), agentRgb(18, 18, 22), agentRgb(226, 186, 36), agentRgb(176, 142, 28), agentRgb(226, 186, 36) };

static const PersonLook PL_SECURITY = {
  agentRgb(46, 66, 128), agentRgb(28, 40, 82), agentRgb(38, 56, 108), agentRgb(30, 34, 52), agentRgb(18, 20, 32),
  agentRgb(20, 20, 24), agentRgb(10, 10, 14), agentRgb(204, 160, 128), agentRgb(154, 118, 94), agentRgb(50, 36, 26) };

static const PersonLook PL_SCI_DIM = {   // a scientist in a dimly lit room
  agentRgb(120, 124, 136), agentRgb(92, 96, 108), agentRgb(106, 110, 122), agentRgb(34, 36, 44), agentRgb(22, 24, 30),
  agentRgb(16, 16, 18), agentRgb(10, 10, 12), agentRgb(120, 96, 82), agentRgb(92, 72, 60), agentRgb(52, 38, 26) };

static const PersonLook PL_SENIOR = {   // the senior scientist: a blue coat, grey hair
  agentRgb(120, 160, 214), agentRgb(84, 116, 160), agentRgb(104, 142, 194), agentRgb(40, 40, 46), agentRgb(26, 26, 30),
  agentRgb(70, 40, 24), agentRgb(46, 26, 16), agentRgb(214, 170, 140), agentRgb(164, 128, 104), agentRgb(196, 196, 200) };

static const PersonLook PL_ADVANCED = {   // the advanced scientist: a lilac coat, white hair
  agentRgb(196, 176, 226), agentRgb(146, 128, 176), agentRgb(176, 156, 206), agentRgb(46, 44, 52), agentRgb(30, 28, 34),
  agentRgb(30, 24, 20), agentRgb(18, 14, 12), agentRgb(222, 182, 150), agentRgb(170, 136, 110), agentRgb(240, 240, 244) };

static const PersonLook PL_CHEF = {     // the chef: white jacket and apron, checked trousers
  agentRgb(244, 244, 240), agentRgb(196, 198, 196), agentRgb(222, 222, 218), agentRgb(74, 74, 80), agentRgb(48, 48, 54),
  agentRgb(26, 24, 24), agentRgb(14, 14, 16), agentRgb(226, 180, 146), agentRgb(172, 134, 106), agentRgb(70, 48, 30) };
static const PersonLook PL_MAINT = {    // maintenance: blue overalls, a cap
  agentRgb(52, 88, 146), agentRgb(32, 56, 96), agentRgb(42, 72, 124), agentRgb(52, 88, 146), agentRgb(32, 56, 96),
  agentRgb(40, 30, 22), agentRgb(24, 18, 14), agentRgb(212, 166, 132), agentRgb(160, 122, 96), agentRgb(90, 62, 38) };
static const PersonLook PL_MAINT_UNDIES = {   // the maintenance man without his overalls: a vest, shorts, bare legs
  agentRgb(236, 236, 230), agentRgb(190, 190, 184), agentRgb(214, 214, 208), agentRgb(212, 166, 132), agentRgb(160, 122, 96),
  agentRgb(40, 30, 22), agentRgb(24, 18, 14), agentRgb(212, 166, 132), agentRgb(160, 122, 96), agentRgb(90, 62, 38) };

static const PersonLook PL_CLEANER = {   // a cleaner: teal tunic, navy trousers
  agentRgb(70, 166, 172), agentRgb(44, 112, 118), agentRgb(58, 142, 148), agentRgb(46, 52, 78), agentRgb(28, 32, 50),
  agentRgb(30, 30, 34), agentRgb(18, 18, 22), agentRgb(200, 150, 116), agentRgb(150, 110, 86), agentRgb(50, 36, 26) };

static inline bool personIsLab(uint8_t kind) {
  return kind == PK_SCIENTIST || kind == PK_STRIPPED || kind == PK_DISGUISE || kind == PK_SCI_DIM || kind == PK_SENIOR || kind == PK_ADVANCED;
}

// Shared: where the hip goes, as agentDraw() does it
static void personXf(const AgentPose &p, const AgentJoints &j, float x, float floorY, int facing, float scale, AgentXf &xf) {
  float planted = -agentLowest(j);
  float hipUp = p.plant * planted + (1.0f - p.plant) * (-AG_STAND_H) - p.lift;
  xf.ox = x;
  xf.oy = floorY + hipUp * scale;
  xf.pivotY = -AG_SPIN_PIVOT;
  xf.c = cosf(agRad(p.spin));
  xf.s = sinf(agRad(p.spin));
  xf.mir = facing < 0 ? -1.0f : 1.0f;
  xf.sc = scale;
}

// The agent's near hand, and the way his forearm points (for his pistol)
static void agentHand(const AgentPose &p, float x, float floorY, int facing, float scale,
                      int &hx, int &hy, int &mx, int &my) {
  AgentJoints j;
  agentSolve(p, j);
  AgentXf xf;
  personXf(p, j, x, floorY, facing, scale, xf);
  float dx = j.wrX[0] - j.elbX[0], dy = j.wrY[0] - j.elbY[0];
  float l = sqrtf(dx * dx + dy * dy);
  if (l > 0.01f) { dx /= l; dy /= l; }
  xf.apply(j.wrX[0] + dx * 2.0f, j.wrY[0] + dy * 2.0f, hx, hy);
  xf.apply(j.wrX[0] + dx * 15.0f, j.wrY[0] + dy * 15.0f - 2.0f, mx, my);
}

// A small shape on a bone in the bone's frame (u along A->B, v across)
static void personPatch(const AgentXf &xf, AgentFillPolyFn fill, float ax, float ay, float bx, float by,
                        float u0, float u1, float v0, float v1, uint16_t col) {
  float dx = bx - ax, dy = by - ay;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.001f) return;
  dx /= len; dy /= len;
  float nx = -dy, ny = dx;
  int xs[4], ys[4];
  float us[4] = { u0, u1, u1, u0 }, vs[4] = { v0, v0, v1, v1 };
  for (int k = 0; k < 4; k++) xf.apply(ax + dx * us[k] + nx * vs[k], ay + dy * us[k] + ny * vs[k], xs[k], ys[k]);
  fill(xs, ys, 4, col);
}

// Draw one of the people. (mx, my): where the muzzle of the weapon is.
static void personDraw(const AgentPose &p, float x, float floorY, int facing, float scale, uint8_t kind,
                       AgentFillPolyFn fillPoly, AgentFillCircleFn fillCircle, int &mx, int &my) {
  const PersonLook &L = ((kind == PK_GUARD || kind == PK_FIGHTER)) ? PL_GUARD : (kind == PK_OFFICER) ? PL_OFFICER :
                        (kind == PK_SCIENTIST) ? PL_SCIENTIST : (kind == PK_STRIPPED) ? PL_STRIPPED :
                        (kind == PK_DISGUISE) ? PL_DISGUISE : (kind == PK_RADSUIT) ? PL_RADSUIT : (kind == PK_SECURITY) ? PL_SECURITY :
                        (kind == PK_SCI_DIM) ? PL_SCI_DIM : (kind == PK_SENIOR) ? PL_SENIOR : (kind == PK_ADVANCED) ? PL_ADVANCED :
                        (kind == PK_CHEF) ? PL_CHEF : (kind == PK_MAINT) ? PL_MAINT : (kind == PK_MAINT_UNDIES) ? PL_MAINT_UNDIES : (kind == PK_CLEANER) ? PL_CLEANER : PL_COMMANDER;
  bool coat = (kind == PK_SCIENTIST || kind == PK_DISGUISE || kind == PK_SCI_DIM || kind == PK_SENIOR || kind == PK_ADVANCED);
  AgentJoints j;
  agentSolve(p, j);
  AgentXf xf;
  personXf(p, j, x, floorY, facing, scale, xf);
  int hr = (int)lroundf(AG_HAND_R * scale);

  // ---- far side ----
  agCapsule(xf, fillPoly, j.shX, j.shY, j.elbX[1], j.elbY[1], 3.7f, 3.2f, L.suitFar);
  agCapsule(xf, fillPoly, j.elbX[1], j.elbY[1], j.wrX[1], j.wrY[1], 3.2f, 2.8f, L.suitFar);
  { int hx, hy; xf.apply(j.wrX[1], j.wrY[1], hx, hy); fillCircle(hx, hy, hr, L.skinFar); }
  agCapsule(xf, fillPoly, 0, 0, j.kneeX[1], j.kneeY[1], 5.2f, 4.2f, L.trouserFar);
  agCapsule(xf, fillPoly, j.kneeX[1], j.kneeY[1], j.ankX[1], j.ankY[1], 4.2f, 3.6f, L.trouserFar);
  { float fx, fy; agDir(j.footA[1], fx, fy); agShape(xf, fillPoly, j.ankX[1], j.ankY[1], fx, fy, AG_BOOT_PTS, AG_BOOT_N, L.bootFar); }

  // ---- body ----
  static const float TORSO[][2] = {
    { -3.0f, -6.5f }, { 6.0f, -7.5f }, { 16.0f, -7.5f }, { 26.0f, -7.0f }, { 31.0f, -5.0f },
    { 33.5f, -1.0f }, { 33.0f, 3.0f }, { 29.0f, 7.0f }, { 21.0f, 8.0f }, { 12.0f, 7.0f },
    { 4.0f, 6.5f }, { -3.0f, 6.5f },
  };
  agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, TORSO, 12, L.suit);
  static const float BACK[][2] = {
    { 2.0f, -7.0f }, { 16.0f, -7.4f }, { 26.0f, -6.9f }, { 30.5f, -5.0f }, { 26.0f, -3.6f }, { 14.0f, -4.0f }, { 2.0f, -3.8f },
  };
  agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BACK, 7, L.shade);
  if ((kind == PK_GUARD || kind == PK_FIGHTER)) {
    // camouflage blotches, then the body armour vest over them
    static const float BLOT1[][2] = { { 2.0f, -2.0f }, { 7.0f, -4.0f }, { 9.0f, 1.0f }, { 4.0f, 3.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BLOT1, 4, agentRgb(120, 100, 66));
    static const float VEST[][2] = {
      { 5.0f, -8.2f }, { 28.0f, -7.8f }, { 31.0f, -4.0f }, { 31.0f, 4.5f }, { 27.0f, 8.6f }, { 5.0f, 8.4f },
    };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, VEST, 6, agentRgb(74, 80, 60));
    static const float PLATE[][2] = { { 10.0f, 2.0f }, { 24.0f, 2.0f }, { 24.0f, 7.6f }, { 10.0f, 7.6f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, PLATE, 4, agentRgb(88, 94, 72));
    static const float POUCHES[][2] = { { 6.0f, 4.0f }, { 11.0f, 4.0f }, { 11.0f, 9.4f }, { 6.0f, 9.4f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, POUCHES, 4, agentRgb(62, 66, 48));
  } else if (kind == PK_OFFICER) {
    static const float ZIP[][2] = { { 2.0f, 6.0f }, { 31.0f, 6.0f }, { 31.0f, 7.2f }, { 2.0f, 7.2f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, ZIP, 4, agentRgb(150, 156, 128));
    static const float NAME[][2] = { { 21.0f, 2.0f }, { 24.5f, 2.0f }, { 24.5f, 7.2f }, { 21.0f, 7.2f } };   // name patch
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, NAME, 4, agentRgb(236, 236, 228));
    static const float BELT[][2] = { { 1.0f, -6.8f }, { 4.0f, -7.0f }, { 4.0f, 6.8f }, { 1.0f, 6.8f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BELT, 4, agentRgb(40, 40, 36));
  } else if (coat) {
    // the coat's open front over a shirt, lapels, and the ID badge on its lanyard
    static const float FRONT[][2] = { { 4.0f, 5.2f }, { 31.0f, 4.0f }, { 33.0f, 7.0f }, { 4.0f, 7.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, FRONT, 4, agentRgb(150, 184, 212));
    static const float LAPEL[][2] = { { 22.0f, 3.0f }, { 32.0f, 2.0f }, { 33.2f, 4.6f }, { 24.0f, 6.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, LAPEL, 4, agentRgb(206, 210, 218));
    static const float LANYARD[][2] = { { 18.0f, 4.6f }, { 31.0f, 3.6f }, { 31.0f, 4.4f }, { 18.0f, 5.4f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, LANYARD, 4, agentRgb(40, 90, 190));
    static const float BADGE[][2] = { { 12.0f, 3.0f }, { 18.6f, 3.0f }, { 18.6f, 7.8f }, { 12.0f, 7.8f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BADGE, 4, agentRgb(250, 250, 250));
    static const float BADGE_TOP[][2] = { { 16.6f, 3.0f }, { 18.6f, 3.0f }, { 18.6f, 7.8f }, { 16.6f, 7.8f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BADGE_TOP, 4, (kind == PK_SENIOR || kind == PK_ADVANCED) ? agentRgb(210, 40, 40) : agentRgb(40, 110, 200));
    if (kind == PK_SENIOR) {   // a tie
      static const float TIE[][2] = { { 20.0f, 6.0f }, { 31.0f, 5.4f }, { 31.0f, 6.8f }, { 20.0f, 7.2f } };
      agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, TIE, 4, agentRgb(160, 30, 40));
    }
    static const float PEN[][2] = { { 22.0f, -1.0f }, { 26.5f, -1.0f }, { 26.5f, 0.2f }, { 22.0f, 0.2f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, PEN, 4, agentRgb(30, 60, 160));
  } else if (kind == PK_SECURITY) {
    static const float BADGE[][2] = { { 22.0f, 3.0f }, { 26.0f, 3.0f }, { 26.0f, 7.0f }, { 22.0f, 7.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BADGE, 4, agentRgb(210, 214, 222));
    static const float POCKET[][2] = { { 18.0f, 1.0f }, { 21.0f, 1.0f }, { 21.0f, 6.8f }, { 18.0f, 6.8f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, POCKET, 4, agentRgb(38, 56, 108));
    static const float BELT[][2] = { { 1.0f, -6.8f }, { 4.0f, -7.0f }, { 4.0f, 6.8f }, { 1.0f, 6.8f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BELT, 4, agentRgb(20, 20, 22));
    static const float BUCKLE[][2] = { { 1.4f, 3.6f }, { 3.6f, 3.6f }, { 3.6f, 6.6f }, { 1.4f, 6.6f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BUCKLE, 4, agentRgb(200, 200, 206));
  } else if (kind == PK_CHEF) {
    static const float SCARF[][2] = { { 28.0f, -2.0f }, { 32.6f, -1.0f }, { 32.6f, 6.6f }, { 28.0f, 7.4f } };   // the neckerchief
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, SCARF, 4, agentRgb(200, 40, 40));
    for (int k = 0; k < 4; k++) {   // two rows of buttons down the jacket
      float u = 10.0f + k * 4.8f;
      const float B1[][2] = { { u, 3.0f }, { u + 1.6f, 3.0f }, { u + 1.6f, 4.6f }, { u, 4.6f } };
      const float B2[][2] = { { u, 6.0f }, { u + 1.6f, 6.0f }, { u + 1.6f, 7.6f }, { u, 7.6f } };
      agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, B1, 4, agentRgb(150, 150, 156));
      agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, B2, 4, agentRgb(150, 150, 156));
    }
  } else if (kind == PK_MAINT) {
    static const float COLLAR[][2] = { { 27.0f, -6.0f }, { 33.0f, -3.6f }, { 33.4f, 4.0f }, { 27.0f, 7.6f } };   // the shirt under the overalls
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, COLLAR, 4, agentRgb(200, 196, 180));
    static const float STRIPE[][2] = { { 15.0f, -7.6f }, { 18.0f, -7.6f }, { 18.0f, 8.1f }, { 15.0f, 8.1f } };   // a hi-vis band
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, STRIPE, 4, agentRgb(214, 236, 70));
    static const float POCKET[][2] = { { 20.0f, 2.0f }, { 25.0f, 2.0f }, { 25.0f, 7.4f }, { 20.0f, 7.4f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, POCKET, 4, agentRgb(40, 72, 124));
    static const float PEN[][2] = { { 24.0f, 3.0f }, { 27.5f, 3.0f }, { 27.5f, 4.0f }, { 24.0f, 4.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, PEN, 4, agentRgb(220, 60, 40));
    static const float BELT[][2] = { { 1.0f, -6.8f }, { 4.0f, -7.0f }, { 4.0f, 6.8f }, { 1.0f, 6.8f } };   // the tool belt
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BELT, 4, agentRgb(96, 66, 36));
    static const float POUCH[][2] = { { -3.0f, -2.0f }, { 4.0f, -2.0f }, { 4.0f, 3.6f }, { -3.0f, 3.6f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, POUCH, 4, agentRgb(120, 84, 46));
  } else if (kind == PK_CLEANER) {
    static const float COLLAR[][2] = { { 28.0f, -4.0f }, { 33.0f, -2.6f }, { 33.4f, 4.0f }, { 28.0f, 7.4f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, COLLAR, 4, agentRgb(236, 240, 240));
    static const float APRON[][2] = { { 2.0f, 2.0f }, { 24.0f, 2.6f }, { 24.0f, 8.0f }, { 2.0f, 7.6f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, APRON, 4, agentRgb(150, 110, 190));
    static const float TAG[][2] = { { 25.0f, 3.0f }, { 28.0f, 3.0f }, { 28.0f, 7.0f }, { 25.0f, 7.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, TAG, 4, agentRgb(250, 250, 250));
  } else if (kind == PK_STRIPPED) {
    static const float BTNS[][2] = { { 4.0f, 6.4f }, { 31.0f, 5.4f }, { 31.0f, 6.4f }, { 4.0f, 7.2f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BTNS, 4, agentRgb(120, 154, 184));
  } else {
    static const float SHIRT[][2] = { { 26.0f, 3.0f }, { 33.0f, 3.0f }, { 33.0f, 7.0f }, { 28.0f, 7.0f } };    // collar
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, SHIRT, 4, agentRgb(236, 236, 240));
    static const float TIE[][2] = { { 22.0f, 6.0f }, { 28.5f, 6.0f }, { 28.5f, 7.5f }, { 22.0f, 7.5f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, TIE, 4, agentRgb(20, 22, 30));
    static const float RIB1[][2] = { { 20.0f, 1.0f }, { 22.0f, 1.0f }, { 22.0f, 6.0f }, { 20.0f, 6.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, RIB1, 4, agentRgb(210, 40, 40));
    static const float RIB2[][2] = { { 18.0f, 1.0f }, { 20.0f, 1.0f }, { 20.0f, 6.0f }, { 18.0f, 6.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, RIB2, 4, agentRgb(240, 200, 60));
    static const float RIB3[][2] = { { 16.0f, 1.0f }, { 18.0f, 1.0f }, { 18.0f, 6.0f }, { 16.0f, 6.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, RIB3, 4, agentRgb(60, 110, 210));
    static const float BTN[][2] = { { 6.0f, 6.0f }, { 14.0f, 6.0f }, { 14.0f, 7.4f }, { 6.0f, 7.4f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, BTN, 4, agentRgb(210, 180, 80));
  }

  // ---- near leg ----
  agCapsule(xf, fillPoly, 0, 0, j.kneeX[0], j.kneeY[0], 5.4f, 4.4f, L.trouser);
  agCapsule(xf, fillPoly, j.kneeX[0], j.kneeY[0], j.ankX[0], j.ankY[0], 4.4f, 3.7f, L.trouser);
  if ((kind == PK_GUARD || kind == PK_FIGHTER)) {   // camouflage on the leg
    personPatch(xf, fillPoly, 0, 0, j.kneeX[0], j.kneeY[0], 4.0f, 11.0f, -3.0f, 2.0f, agentRgb(120, 100, 66));
    personPatch(xf, fillPoly, 0, 0, j.kneeX[0], j.kneeY[0], 13.0f, 19.0f, 0.0f, 4.4f, agentRgb(58, 70, 42));
    personPatch(xf, fillPoly, j.kneeX[0], j.kneeY[0], j.ankX[0], j.ankY[0], 6.0f, 12.0f, -3.6f, 1.0f, agentRgb(58, 70, 42));
  }
  if (kind == PK_COMMANDER)   // the stripe down the trouser leg
    personPatch(xf, fillPoly, 0, 0, j.kneeX[0], j.kneeY[0], 0.0f, 23.0f, -0.6f, 0.6f, agentRgb(200, 170, 70));
  { float fx, fy; agDir(j.footA[0], fx, fy); agShape(xf, fillPoly, j.ankX[0], j.ankY[0], fx, fy, AG_BOOT_PTS, AG_BOOT_N, L.boot); }
  if (kind == PK_MAINT_UNDIES) {   // shorts over both thighs
    personPatch(xf, fillPoly, 0, 0, j.kneeX[1], j.kneeY[1], -2.0f, 10.0f, -5.6f, 5.6f, agentRgb(70, 92, 150));
    personPatch(xf, fillPoly, 0, 0, j.kneeX[0], j.kneeY[0], -2.0f, 10.0f, -5.8f, 5.8f, agentRgb(90, 112, 170));
  }
  if (kind == PK_CHEF) {   // the apron, down over the thighs
    static const float APRON[][2] = { { 6.0f, 1.0f }, { -20.0f, 0.0f }, { -21.0f, 9.0f }, { 6.0f, 8.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, APRON, 4, agentRgb(250, 250, 248));
    static const float TIE[][2] = { { 3.0f, -7.0f }, { 5.0f, -7.0f }, { 5.0f, 8.0f }, { 3.0f, 8.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, TIE, 4, agentRgb(200, 200, 196));
  }
  if (coat) {   // the coat hangs down over the thighs
    static const float TAIL[][2] = { { 2.0f, -8.0f }, { -23.0f, -9.6f }, { -24.0f, 8.6f }, { 2.0f, 7.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, TAIL, 4, L.suit);
    static const float EDGE[][2] = { { 2.0f, 5.6f }, { -24.0f, 7.0f }, { -24.0f, 8.6f }, { 2.0f, 7.0f } };
    agShape(xf, fillPoly, 0, 0, j.tdx, j.tdy, EDGE, 4, L.shade);
  }

  // ---- head: face, then what is on it ----
  {
    int cx, cy;
    xf.apply(j.headX, j.headY, cx, cy);
    fillCircle(cx, cy, (int)lroundf((AG_HEAD_R - 1.2f) * scale), L.skin);
    static const float HAIR[][2] = {   // back of the head
      { 8.0f, -6.0f }, { 3.0f, -9.4f }, { -4.0f, -9.0f }, { -8.0f, -5.0f }, { -4.0f, -3.0f }, { 4.0f, -2.0f },
    };
    agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, HAIR, 6, L.hair);
    static const float EYE[][2] = { { 2.6f, 5.0f }, { 2.6f, 6.6f }, { 1.0f, 6.6f }, { 1.0f, 5.0f } };
    if (kind == PK_SCIENTIST || kind == PK_SCI_DIM || kind == PK_SENIOR || kind == PK_ADVANCED) {   // glasses
      static const float FRAME[][2] = { { 0.2f, 3.4f }, { 4.0f, 3.4f }, { 4.0f, 8.2f }, { 0.2f, 8.2f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, FRAME, 4, agentRgb(36, 36, 40));
      static const float LENS[][2] = { { 0.8f, 4.0f }, { 3.4f, 4.0f }, { 3.4f, 7.6f }, { 0.8f, 7.6f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, LENS, 4, agentRgb(196, 214, 224));
    }
    agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, EYE, 4, agentRgb(30, 26, 24));
    static const float NOSE[][2] = { { 0.5f, 8.2f }, { -1.5f, 9.6f }, { -2.4f, 8.2f } };
    agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, NOSE, 3, L.skin);
    if (kind == PK_GUARD) {
      static const float HELMET[][2] = {
        { 2.0f, -10.4f }, { 7.0f, -9.0f }, { 10.4f, -4.0f }, { 11.2f, 1.0f }, { 10.0f, 6.0f }, { 6.0f, 9.0f },
        { 3.4f, 10.6f }, { 2.6f, 7.0f }, { 1.4f, -3.0f }, { -2.0f, -11.2f },
      };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, HELMET, 10, agentRgb(84, 94, 62));
      static const float STRAP[][2] = { { 2.0f, 2.0f }, { -6.0f, 4.0f }, { -6.0f, 5.4f }, { 2.0f, 3.4f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, STRAP, 4, agentRgb(40, 40, 34));
    } else if (kind == PK_OFFICER) {
      static const float BAND[][2] = { { 10.0f, -2.0f }, { 10.0f, 1.2f }, { -1.0f, 1.2f }, { -1.0f, -2.0f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, BAND, 4, agentRgb(36, 36, 40));
      int ex, ey;
      xf.apply(j.headX + j.hdx * -0.5f, j.headY + j.hdy * -0.5f, ex, ey);
      fillCircle(ex, ey, (int)lroundf(3.6f * scale), agentRgb(30, 30, 34));        // ear cup
      static const float MIC[][2] = { { -1.0f, 0.0f }, { -5.0f, 8.0f }, { -6.0f, 8.0f }, { -2.0f, -0.6f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, MIC, 4, agentRgb(30, 30, 34));
    } else if (kind == PK_CHEF) {   // the tall white hat
      static const float TOQUE[][2] = {
        { 6.0f, -8.8f }, { 13.0f, -10.4f }, { 19.0f, -10.0f }, { 23.4f, -6.0f }, { 24.6f, 0.0f }, { 23.4f, 6.0f },
        { 19.0f, 10.0f }, { 13.0f, 10.4f }, { 6.0f, 8.8f },
      };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, TOQUE, 9, agentRgb(250, 250, 248));
      static const float PLEAT[][2] = { { 12.0f, -1.0f }, { 23.0f, -1.6f }, { 23.0f, 0.0f }, { 12.0f, 0.4f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, PLEAT, 4, agentRgb(214, 214, 210));
      static const float BAND[][2] = { { 5.4f, -9.0f }, { 9.0f, -9.4f }, { 9.0f, 9.4f }, { 5.4f, 9.0f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, BAND, 4, agentRgb(226, 226, 222));
    } else if (kind == PK_CLEANER) {   // hair tied back in a bun
      static const float BUN[][2] = { { 2.0f, -8.0f }, { 6.0f, -12.0f }, { 1.0f, -15.0f }, { -3.0f, -12.0f }, { -2.0f, -8.0f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, BUN, 5, L.hair);
    } else if (kind == PK_MAINT) {  // a cap with a peak
      static const float CROWN[][2] = { { 5.4f, -9.8f }, { 9.6f, -8.4f }, { 11.2f, 0.0f }, { 9.6f, 7.4f }, { 5.4f, 8.6f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, CROWN, 5, agentRgb(40, 70, 120));
      static const float PEAK[][2] = { { 6.4f, 7.0f }, { 4.8f, 15.0f }, { 3.4f, 15.0f }, { 4.6f, 6.4f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, PEAK, 4, agentRgb(28, 50, 90));
      static const float LOGO[][2] = { { 7.0f, 2.0f }, { 9.4f, 2.0f }, { 9.4f, 5.0f }, { 7.0f, 5.0f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, LOGO, 4, agentRgb(214, 236, 70));
    } else if (kind == PK_COMMANDER || kind == PK_SECURITY) {
      bool sec = (kind == PK_SECURITY);
      static const float CAP[][2] = {   // crown of the peaked cap
        { 6.0f, -10.0f }, { 12.4f, -9.0f }, { 13.0f, 4.0f }, { 7.0f, 5.0f }, { 6.0f, -2.0f },
      };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, CAP, 5, sec ? agentRgb(24, 30, 56) : agentRgb(40, 54, 96));
      static const float BAND[][2] = { { 6.0f, -9.8f }, { 8.2f, -9.6f }, { 8.2f, 5.4f }, { 6.0f, 5.0f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, BAND, 4, sec ? agentRgb(14, 16, 24) : agentRgb(210, 176, 70));
      static const float PEAK[][2] = { { 6.4f, 4.0f }, { 5.0f, 12.0f }, { 3.8f, 12.0f }, { 4.8f, 3.6f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, PEAK, 4, agentRgb(16, 16, 20));
      static const float BADGE[][2] = { { 9.0f, 2.0f }, { 11.4f, 2.0f }, { 11.4f, 4.4f }, { 9.0f, 4.4f } };
      agShape(xf, fillPoly, j.headX, j.headY, j.hdx, j.hdy, BADGE, 4, sec ? agentRgb(210, 214, 222) : agentRgb(220, 200, 120));
    }
  }

  // ---- the weapon, held in the near hand ----
  float dx = j.wrX[0] - j.elbX[0], dy = j.wrY[0] - j.elbY[0];
  float l = sqrtf(dx * dx + dy * dy);
  if (l > 0.01f) { dx /= l; dy /= l; }
  if (personIsLab(kind) || kind == PK_FIGHTER || kind == PK_RADSUIT || kind == PK_CHEF || kind == PK_MAINT || kind == PK_MAINT_UNDIES || kind == PK_CLEANER) {
    xf.apply(j.wrX[0], j.wrY[0], mx, my);                              // (no weapon)
  } else if (kind == PK_GUARD) {
    float sx = j.wrX[0] - dx * 20.0f, sy = j.wrY[0] - dy * 20.0f;      // stock
    float ex = j.wrX[0] + dx * 24.0f, ey = j.wrY[0] + dy * 24.0f;      // muzzle
    agCapsule(xf, fillPoly, sx, sy, ex, ey, 2.4f, 1.3f, agentRgb(36, 38, 34));
    personPatch(xf, fillPoly, j.wrX[0], j.wrY[0], ex, ey, 2.0f, 7.0f, -1.0f, 6.0f, agentRgb(46, 48, 42));   // magazine
    xf.apply(ex, ey, mx, my);
  } else {
    float ex = j.wrX[0] + dx * 12.0f, ey = j.wrY[0] + dy * 12.0f;
    agCapsule(xf, fillPoly, j.wrX[0] - dx * 1.0f, j.wrY[0] - dy * 1.0f, ex, ey, 1.9f, 1.6f, agentRgb(40, 40, 44));
    xf.apply(ex, ey, mx, my);
  }

  // ---- near arm, with the rank on it ----
  agCapsule(xf, fillPoly, j.shX, j.shY, j.elbX[0], j.elbY[0], 3.9f, 3.3f, L.suit);
  agCapsule(xf, fillPoly, j.elbX[0], j.elbY[0], j.wrX[0], j.wrY[0], 3.3f, 2.9f, L.suit);
  if ((kind == PK_GUARD || kind == PK_FIGHTER)) {         // sergeant: three chevrons
    for (int k = 0; k < 3; k++) {
      float t0 = 0.22f + k * 0.13f;
      agBand(xf, fillPoly, j.shX, j.shY, j.elbX[0], j.elbY[0], 3.9f, 3.3f, t0, t0 + 0.06f, agentRgb(30, 30, 26));
    }
  } else if (kind == PK_OFFICER) { // captain: two silver bars on the shoulder
    personPatch(xf, fillPoly, j.shX, j.shY, j.elbX[0], j.elbY[0], -1.0f, 0.4f, -3.2f, 3.2f, agentRgb(220, 222, 230));
    personPatch(xf, fillPoly, j.shX, j.shY, j.elbX[0], j.elbY[0], 1.4f, 2.8f, -3.2f, 3.2f, agentRgb(220, 222, 230));
  } else if (kind == PK_SECURITY) {  // a shoulder patch
    personPatch(xf, fillPoly, j.shX, j.shY, j.elbX[0], j.elbY[0], 3.0f, 8.0f, -2.6f, 2.6f, agentRgb(200, 170, 60));
  } else if (kind == PK_COMMANDER) { // colonel: silver eagle on the shoulder
    personPatch(xf, fillPoly, j.shX, j.shY, j.elbX[0], j.elbY[0], -1.4f, 2.4f, -3.6f, 3.6f, agentRgb(200, 204, 214));
    personPatch(xf, fillPoly, j.shX, j.shY, j.elbX[0], j.elbY[0], 0.2f, 1.0f, -1.0f, 1.0f, agentRgb(110, 114, 124));
    agBand(xf, fillPoly, j.elbX[0], j.elbY[0], j.wrX[0], j.wrY[0], 3.3f, 2.9f, 0.78f, 0.86f, agentRgb(200, 170, 70));   // cuff braid
  }
  { int hx, hy; xf.apply(j.wrX[0] + dx * 1.5f, j.wrY[0] + dy * 1.5f, hx, hy); fillCircle(hx, hy, hr, L.skin); }
}

#endif // PEOPLE_H
