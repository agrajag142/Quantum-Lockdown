// game.cpp - the full-screen game: the lift shafts and the rooms
//
// The complex, Impossible Mission style: 30 rooms in COLS (9) columns, each
// column holding MIN_PER_COL..MAX_PER_COL (2..6) of them on any of ROWS (6)
// rows, laid out afresh each game. A lift shaft runs down each side of every
// column (SHAFTS: 10, A to J). Every room has a door on its left and one on its
// right, each at the room's TOP or BOTTOM; a door leads into the shaft on that
// side, at the floor that matches it. Each shaft has two floor positions for
// each row (the top doors', then the bottom doors'), but the car only stops
// where there is a door: the rest of the shaft is wall.
//
//            shaft A    column 1   shaft B    column 2   shaft C ...
//   floor 1    |-------[top    ]     |                    |
//   floor 2    |       [ bottom]-----|-------[top    ]    |
//   floor 3    |  (no room here)     |       [ bottom]----|
//
// The agent, his poses and the somersault are NUCLEAR MISSION's (agent_rig.h);
// the movement, the lift car, the catwalks, the lift pads, the platforms and
// the searchable equipment are the sketch's, on a screen 1280 units wide.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "game.h"
#include "gfx.h"
#include "agent_rig.h"
#include "rooms_wide.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================ settings
#define GAME_VERSION      "002"
#define COLS              9        // columns of rooms across...
#define ROWS              6        // ...rows down...
#define ROOM_COUNT        30       // ...and the rooms in them
#define MIN_PER_COL       2        // each column has at least this many rooms...
#define MAX_PER_COL       6        // ...and at most this many
#define SHAFTS            (COLS + 1)
#define FLOORS            (ROWS * 2)
#define START_SHAFT       (SHAFTS / 2)   // he starts in this shaft's car (one of the middle two), at its top stop

#define AGENT_SCALE       1.3f
#define RUN_SPEED         175.0f
#define JUMP_DISTANCE     150.0f
#define GRAVITY           1700.0f
#define FALL_MAX_SPEED    950.0f
#define FALL_SAFE_PX      200
#define EDGE_GRACE_PX     8
#define LIFT_SPEED        150.0f   // lift pads in rooms
#define CAR_SPEED         260.0f   // the lift car in the shafts: its speed to start with...
#define CAR_MAX_SPEED     1100.0f  // ...picking up to this over a long run past walled-off floors
#define CAR_ACCEL         900.0f   // (units a second, each second; it slows the same way to stop)
#define RESPAWN_SECONDS   1.6f
#define BLEND_SECONDS     0.12f
#define SEARCH_SECONDS    1.6f     // hold USE this long to search a piece of equipment
#define NOTHING_SHOW      1.2f
#define DECOR_PER_ROOM    3

#define HUD_H    40
#define PLAY_Y1  (SCREEN_H - 1)
#define PLAT_T   16
#define PAD_H    12
#define DOOR_H   140               // a room's doorway (he is 130 tall)
#define CEIL_Y   56                // a room's ceiling (under it: the top level's headroom)

// The shaft view
#define SHAFT_X0     580
#define SHAFT_X1     700
#define CAR_FLOOR_Y  520           // the car's floor on screen (the view follows the car)
#define LEVEL_GAP    330           // between floor positions in the shaft
#define CORR_H       190           // catwalk: floor to the beam above
#define CAT_DOOR_H   168           // the doorway at the end of a catwalk

// ============================================================ the complex
struct RoomInfo {
  int8_t row, col;                 // where it is
  bool topL, topR;                 // the doors: true = at the top, false = at the bottom
  uint8_t tpl;                     // its layout (rooms_wide.h)
  uint16_t seed;                   // its equipment
  bool visited;
  bool searched[DECOR_PER_ROOM];
};
static RoomInfo rooms[ROOM_COUNT];         // numbered 1..30 across the rows, top to bottom
static int8_t roomAt[ROWS][COLS];          // which room is in each place (-1: none)
static bool shaftSeen[SHAFTS][FLOORS];
static uint32_t layoutSeed = 1;

static uint32_t rngState = 1;
static uint32_t rnd32() {
  rngState = rngState * 1664525u + 1013904223u;
  return rngState >> 8;
}
static int rnd(int n) { return n > 0 ? (int)(rnd32() % (uint32_t)n) : 0; }

static char shaftLetter(int s) { return (char)('A' + s); }

// The room through the door on one side of a shaft at a floor (its index), or
// -1 (a wall). right: the room to the right of the shaft (through its left
// door), else the one to the left (through its right door).
static int roomOff(int s, int f, bool right) {
  if (f < 0 || f >= FLOORS) return -1;
  int c = right ? s : s - 1, r = f / 2;
  if (c < 0 || c >= COLS) return -1;
  int i = roomAt[r][c];
  if (i < 0) return -1;
  bool top = (f % 2) == 0;
  bool door = right ? rooms[i].topL == top : rooms[i].topR == top;
  return door ? i : -1;
}

// Does the car stop here? (a door on either side)
static bool isStop(int s, int f) { return roomOff(s, f, false) >= 0 || roomOff(s, f, true) >= 0; }
static int topStop(int s) {
  for (int f = 0; f < FLOORS; f++)
    if (isStop(s, f)) return f;
  return -1;
}
static int bottomStop(int s) {
  for (int f = FLOORS - 1; f >= 0; f--)
    if (isStop(s, f)) return f;
  return -1;
}

static void layOut(uint32_t seed) {
  layoutSeed = seed;
  rngState = seed * 2654435761u + 77u;
  // how many rooms in each column: MIN_PER_COL each, the rest shared out at random
  int count[COLS];
  int left = ROOM_COUNT;
  for (int c = 0; c < COLS; c++) {
    count[c] = MIN_PER_COL;
    left -= MIN_PER_COL;
  }
  while (left > 0) {
    int c = rnd(COLS);
    if (count[c] >= MAX_PER_COL) continue;
    count[c]++;
    left--;
  }
  // which rows they are on
  memset(roomAt, -1, sizeof(roomAt));
  bool used[ROWS][COLS] = {};
  for (int c = 0; c < COLS; c++)
    for (int k = 0; k < count[c];) {
      int r = rnd(ROWS);
      if (used[r][c]) continue;
      used[r][c] = true;
      k++;
    }
  // number them across the rows, and give each its doors, layout and equipment
  int n = 0;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      if (!used[r][c]) continue;
      RoomInfo &ri = rooms[n];
      roomAt[r][c] = (int8_t)n++;
      ri.row = (int8_t)r;
      ri.col = (int8_t)c;
      ri.topL = rnd(2) == 0;
      ri.topR = rnd(2) == 0;
      ri.tpl = (uint8_t)rnd(WIDE_ROOM_COUNT);
      ri.seed = (uint16_t)rnd32();
      ri.visited = false;
      for (int i = 0; i < DECOR_PER_ROOM; i++) ri.searched[i] = false;
    }
  memset(shaftSeen, 0, sizeof(shaftSeen));
}

// ============================================================ where we are
enum Mode : uint8_t { MODE_SHAFT, MODE_ROOM };
static Mode mode = MODE_SHAFT;
static int curShaft = START_SHAFT;     // in a shaft
static int curRoom = 0;                // in a room

// The lift car (one per shaft; it is where he is)
static float carWorldY = 0;            // world height of the car (floor * LEVEL_GAP)
static int carFloor = 0;               // the floor it is at, or heading for
static bool carMoving = false;
static float carSpeed = 0;

// The current room, built from its layout
struct Lift {
  int16_t x0, x1;
  int16_t stops[MAX_STOPS];
  int8_t stopCount;
  int8_t at;
  float y;
  bool moving;
};
static Plat plats[MAX_PLATS];
static int platCount = 0;
static Lift lifts[MAX_LIFTS];
static int liftCount = 0;
static int16_t pits[MAX_PITS][2];
static int pitCount = 0;
static int doorYL = ROOM_FLOOR_Y, doorYR = ROOM_FLOOR_Y;
static int signX = 0, signY = 0;

// Equipment (searchable)
enum DecorKind : uint8_t { DEC_CABINET, DEC_SAFE, DEC_PLANT, DEC_RADAR, DEC_PANEL, DEC_KINDS };
static const int16_t decorW[DEC_KINDS] = { 52, 62, 44, 84, 96 };
static const int16_t decorH[DEC_KINDS] = { 96, 68, 96, 92, 82 };
struct Decor {
  uint8_t kind;
  int16_t x, y;                    // middle, and the top of what it stands on
  uint16_t seed;                   // its blinking lights
};
static Decor decor[DECOR_PER_ROOM];
static int decorCount = 0;

// Pictures painted once: the room, and the shaft (all but what scrolls)
static uint32_t roomBg[FB_W * FB_H];
static uint32_t shaftBg[FB_W * FB_H];

// ============================================================ the agent
enum AgentState : uint8_t { ST_IDLE, ST_RUN, ST_JUMP, ST_USE, ST_FALL, ST_LAND, ST_LIFT, ST_KO };
static AgentState agState = ST_IDLE;
static float agX = 640, agY = CAR_FLOOR_Y, agT = 0;
static int agFacing = 1, agLift = -1;
static float agVX = 0, agVY = 0, fallFromY = 0, jumpY0 = 0, jumpLiftScale = 1;
static AgentPose shownPose, blendFrom;
static float blendT = 0, blendLen = 0;
static float entryX = 60, entryY = ROOM_FLOOR_Y;
static int entryFacing = 1;
static float myScale = AGENT_SCALE;
static uint32_t held = 0, pressed = 0;

static int searchItem = -1;
static float searchT = 0;
static float nothingShowT = 0, nothingX = 0, nothingY = 0;

// On screen
static bool mapOn = false;
static float msgT = 0;
static char msgText[64] = "";
static float clockT = 0;               // seconds since the start (blinking things)
static float fpsShown = 60, fpsAcc = 0;
static int fpsFrames = 0;

static void showMsg(const char *s, float secs) {
  snprintf(msgText, sizeof(msgText), "%s", s);
  msgT = secs;
}

// ============================================================ painting: rooms (into roomBg, once per room)
static void paintRadiationSign(int cx, int cy, int r) {
  uint16_t yellow = rgb565(236, 196, 40);
  uint16_t black = rgb565(16, 16, 18);
  fillCircle(cx, cy, r + 4, black);
  fillCircle(cx, cy, r, yellow);
  for (int b = 0; b < 3; b++) {
    float mid = -HALF_PI + b * TWO_PI / 3.0f;
    float xs[14], ys[14];
    int n = 0;
    float r0 = r * 0.24f, r1 = r * 0.86f;
    for (int k = 0; k <= 6; k++) {
      float a = mid - PI / 6 + (PI / 3) * k / 6.0f;
      xs[n] = cx + 0.5f + cosf(a) * r1;
      ys[n] = cy + 0.5f + sinf(a) * r1;
      n++;
    }
    for (int k = 6; k >= 0; k--) {
      float a = mid - PI / 6 + (PI / 3) * k / 6.0f;
      xs[n] = cx + 0.5f + cosf(a) * r0;
      ys[n] = cy + 0.5f + sinf(a) * r0;
      n++;
    }
    fillPolygonF(xs, ys, n, black);
  }
  fillCircle(cx, cy, (int)(r * 0.17f), black);
}

static void paintPipe(int y, int h) {
  uint16_t body = rgb565(64, 74, 86), hi = rgb565(104, 116, 130), lo = rgb565(38, 44, 54);
  uint16_t flange = rgb565(50, 58, 70);
  fillRect(0, y, SCREEN_W - 1, y + h - 1, body);
  fillRect(0, y + 2, SCREEN_W - 1, y + 4, hi);
  fillRect(0, y + h - 4, SCREEN_W - 1, y + h - 1, lo);
  for (int x = 70; x < SCREEN_W; x += 180) {
    fillRect(x, y - 4, x + 12, y + h + 3, flange);
    fillRect(x + 2, y - 2, x + 4, y + h + 1, hi);
  }
}

static void paintPlatform(const Plat &p) {
  int x0 = p.x0, x1 = p.x1 - 1, y = p.y;
  bool isFloor = (y >= ROOM_FLOOR_Y);
  int bottom = isFloor ? PLAY_Y1 : y + PLAT_T - 1;
  fillRect(x0, y, x1, bottom, rgb565(70, 76, 88));
  fillRect(x0, y, x1, y + 2, rgb565(140, 148, 160));
  fillRect(x0, bottom - 2, x1, bottom, rgb565(36, 40, 48));
  if (isFloor) {
    for (int x = x0 + 60; x < x1; x += 120) fillRect(x, y + 14, x + 1, bottom - 3, rgb565(52, 56, 66));
    hazardBand(x0, x1, y + 3, y + 10);
  } else {
    for (int x = x0 + 10; x < x1 - 4; x += 12) fillRect(x, y + 6, x + 5, y + PLAT_T - 5, rgb565(48, 52, 62));
  }
  if (x0 > 0) fillRect(x0, y + 3, x0 + 5, isFloor ? y + 10 : y + PLAT_T - 4, rgb565(230, 186, 36));
  if (x1 < SCREEN_W - 1) fillRect(x1 - 5, y + 3, x1, isFloor ? y + 10 : y + PLAT_T - 4, rgb565(230, 186, 36));
}

static int shaftTop(const Lift &l) {
  int t = l.stops[0] - 130;
  return t < CEIL_Y + 14 ? CEIL_Y + 14 : t;
}

static void paintLiftShaft(const Lift &l) {
  int top = shaftTop(l), bottom = l.stops[l.stopCount - 1] + PAD_H;
  fillRect(l.x0, top, l.x1 - 1, bottom, rgb565(18, 22, 30));
  uint16_t rail = rgb565(96, 102, 116), railDark = rgb565(54, 58, 68);
  fillRect(l.x0, top, l.x0 + 3, bottom, rail);
  fillRect(l.x0 + 4, top, l.x0 + 5, bottom, railDark);
  fillRect(l.x1 - 4, top, l.x1 - 1, bottom, rail);
  fillRect(l.x1 - 6, top, l.x1 - 5, bottom, railDark);
  fillRect(l.x0 - 4, top - 14, l.x1 + 3, top, rgb565(60, 64, 74));
  fillRect(l.x0 - 4, top - 14, l.x1 + 3, top - 12, rgb565(110, 116, 128));
  fillCircle((l.x0 + l.x1) / 2, top - 6, 5, rgb565(30, 32, 38));
  for (int s = 0; s < l.stopCount; s++) {
    fillRect(l.x0 - 6, l.stops[s] - 2, l.x0 - 1, l.stops[s] + 2, rgb565(230, 186, 36));
    fillRect(l.x1, l.stops[s] - 2, l.x1 + 5, l.stops[s] + 2, rgb565(230, 186, 36));
  }
}

static void paintPit(int x0, int x1) {
  fillRect(x0, ROOM_FLOOR_Y, x1 - 1, PLAY_Y1, rgb565(4, 4, 6));
  for (int i = 0; i < 4; i++)
    fillRect(x0 + 4 + i * 6, PLAY_Y1 - 6 + i, x1 - 5 - i * 6, PLAY_Y1 - 6 + i, dimmed(160, 30, 20, 0.5f - i * 0.1f));
}

// One side of the room: a bulkhead, with a doorway cut in it at doorY (the
// top level or the floor) and an EXIT lamp beside it
static void paintRoomSide(bool right, int doorY) {
  uint16_t frame = rgb565(96, 104, 118), frameHi = rgb565(150, 158, 172), dark = rgb565(8, 10, 14);
  // the bulkhead
  int bx0 = right ? SCREEN_W - 16 : 0, bx1 = right ? SCREEN_W - 1 : 15;
  fillRect(bx0, HUD_H, bx1, ROOM_FLOOR_Y - 1, frame);
  fillRect(right ? bx0 : bx1 - 2, HUD_H, right ? bx0 + 2 : bx1, ROOM_FLOOR_Y - 1, frameHi);
  for (int y = 70; y < ROOM_FLOOR_Y; y += 40) fillCircle((bx0 + bx1) / 2, y, 2, rgb565(60, 66, 78));
  // the doorway
  int x0 = right ? SCREEN_W - 30 : 0, x1 = right ? SCREEN_W - 1 : 29;
  int top = doorY - DOOR_H;
  fillRect(x0, top, x1, doorY - 1, dark);
  int fx0 = right ? x0 - 10 : x1 + 1, fx1 = fx0 + 9;
  fillRect(fx0, top - 12, fx1, doorY - 1, frame);
  fillRect(right ? fx0 : fx1 - 2, top - 12, right ? fx0 + 2 : fx1, doorY - 1, frameHi);
  fillRect(x0, top - 12, x1, top - 1, frame);
  // the EXIT lamp: over the doorway, or beside it where the ceiling is too close
  bool above = top - 36 > CEIL_Y + 20;
  int lx = above ? (right ? SCREEN_W - 36 : 6) : (right ? SCREEN_W - 112 : 82);
  int ly = above ? top - 34 : top + 6;
  fillRect(lx, ly, lx + 30, ly + 16, rgb565(20, 60, 30));
  fillRect(lx + 3, ly + 3, lx + 27, ly + 13, rgb565(60, 220, 110));
}

// ---- the equipment (the sketch's painters)
static void paintCabinet(int x, int fy) {
  int x0 = x - 26, x1 = x + 26, top = fy - 96;
  uint16_t body = rgb565(92, 100, 90), edge = rgb565(58, 64, 56), face = rgb565(106, 114, 102);
  fillRect(x0 + 3, fy - 4, x0 + 8, fy - 1, edge);
  fillRect(x1 - 8, fy - 4, x1 - 3, fy - 1, edge);
  fillRect(x0, top, x1, fy - 5, body);
  fillRect(x0, top, x1, top + 3, rgb565(130, 138, 126));
  fillRect(x1 - 4, top + 4, x1, fy - 5, edge);
  for (int d = 0; d < 3; d++) {
    int dy = top + 8 + d * 29;
    fillRect(x0 + 4, dy, x1 - 8, dy + 24, face);
    fillRect(x0 + 4, dy + 24, x1 - 8, dy + 25, edge);
    fillRect(x - 10, dy + 13, x + 6, dy + 16, rgb565(44, 46, 44));
    fillRect(x - 7, dy + 5, x + 3, dy + 9, rgb565(206, 200, 170));
  }
}

static void paintSafe(int x, int fy) {
  int x0 = x - 31, x1 = x + 31, top = fy - 68;
  fillRect(x0 + 4, fy - 5, x0 + 10, fy - 1, rgb565(30, 30, 34));
  fillRect(x1 - 10, fy - 5, x1 - 4, fy - 1, rgb565(30, 30, 34));
  fillRect(x0, top, x1, fy - 6, rgb565(54, 56, 64));
  fillRect(x0, top, x1, top + 2, rgb565(96, 100, 110));
  fillRect(x0 + 6, top + 7, x1 - 6, fy - 12, rgb565(66, 68, 78));
  drawRectOutline(x0 + 6, top + 7, x1 - 6, fy - 12, rgb565(38, 40, 46), 1);
  fillRect(x0 + 3, top + 14, x0 + 6, top + 22, rgb565(120, 124, 134));
  fillRect(x0 + 3, fy - 28, x0 + 6, fy - 20, rgb565(120, 124, 134));
  int cx = x + 4, cy = top + 28;
  fillCircle(cx, cy, 12, rgb565(150, 154, 164));
  fillCircle(cx, cy, 8, rgb565(40, 42, 48));
  for (int k = 0; k < 8; k++) {
    float a = k * PI / 4;
    drawLineF(cx + 0.5f + cosf(a) * 9, cy + 0.5f + sinf(a) * 9, cx + 0.5f + cosf(a) * 11.5f, cy + 0.5f + sinf(a) * 11.5f,
              rgb565(40, 42, 48), 1.0f);
  }
  fillRect(cx - 1, cy - 7, cx + 1, cy - 3, rgb565(220, 220, 228));
  drawLine(x - 8, top + 48, x + 16, top + 48, rgb565(170, 174, 184), true);
  fillCircle(x - 8, top + 48, 3, rgb565(170, 174, 184));
  fillRect(x - 6, fy - 20, x + 14, fy - 16, rgb565(190, 160, 70));
}

static void paintPlant(int x, int fy) {
  int xs[4] = { x - 18, x + 18, x + 13, x - 13 };
  int ys[4] = { fy - 36, fy - 36, fy - 1, fy - 1 };
  fillPolygon(xs, ys, 4, rgb565(160, 84, 52));
  fillRect(x - 21, fy - 41, x + 21, fy - 35, rgb565(180, 98, 62));
  fillRect(x - 6, fy - 30, x + 10, fy - 6, rgb565(140, 72, 44));
  fillRect(x - 17, fy - 43, x + 17, fy - 41, rgb565(54, 36, 24));
  drawLine(x, fy - 42, x - 2, fy - 86, rgb565(50, 90, 46), true);
  static const int8_t leaves[7][4] = {
    { 8, -22, -10, 6 }, { 10, 22, -14, 6 }, { 20, -20, -22, 6 }, { 24, 20, -24, 6 },
    { 32, -14, -30, 5 }, { 36, 14, -28, 5 }, { 44, 2, -24, 5 } };
  const uint16_t greens[3] = { rgb565(56, 130, 64), rgb565(74, 158, 80), rgb565(44, 110, 54) };
  for (int i = 0; i < 7; i++) {
    int bx = x - (leaves[i][0] * 2) / 44, by = fy - 42 - leaves[i][0];
    int tx = bx + leaves[i][1], ty = by + leaves[i][2];
    int mx = (bx + tx) / 2, my = (by + ty) / 2;
    float lx = (float)(tx - bx), ly = (float)(ty - by), l = sqrtf(lx * lx + ly * ly);
    int nx = (int)(-ly / l * leaves[i][3]), ny = (int)(lx / l * leaves[i][3]);
    int px[4] = { bx, mx + nx, tx, mx - nx };
    int py[4] = { by, my + ny, ty, my - ny };
    fillPolygon(px, py, 4, greens[i % 3]);
    drawLine(bx, by, tx, ty, rgb565(36, 84, 40), false);
  }
}

static void paintRadar(int x, int fy) {
  int x0 = x - 42, x1 = x + 42, top = fy - 92;
  fillRect(x0, top, x1, fy - 1, rgb565(70, 78, 74));
  fillRect(x0, top, x1, top + 3, rgb565(104, 112, 106));
  fillRect(x1 - 4, top + 4, x1, fy - 1, rgb565(48, 54, 50));
  int cx = x, cy = top + 38;
  fillCircle(cx, cy, 31, rgb565(36, 40, 38));
  fillCircle(cx, cy, 26, rgb565(6, 28, 12));
  for (int r = 8; r <= 24; r += 8) drawCircle(cx, cy, r, rgb565(24, 76, 40));
  drawLine(cx - 25, cy, cx + 25, cy, rgb565(20, 64, 34), false);
  drawLine(cx, cy - 25, cx, cy + 25, rgb565(20, 64, 34), false);
  for (int k = 0; k < 3; k++) fillCircle(x - 24 + k * 24, fy - 16, 5, rgb565(40, 44, 42));
  fillRect(x0 + 6, fy - 6, x1 - 8, fy - 4, rgb565(48, 54, 50));
}

static void paintPanel(int x, int fy) {
  uint16_t body = rgb565(72, 80, 92), deck = rgb565(90, 98, 112), dark = rgb565(30, 32, 38);
  fillRect(x - 46, fy - 50, x + 46, fy - 1, body);
  fillRect(x + 42, fy - 50, x + 46, fy - 1, rgb565(52, 58, 68));
  int xs[4] = { x - 48, x + 48, x + 42, x - 42 };
  int ys[4] = { fy - 50, fy - 50, fy - 82, fy - 82 };
  fillPolygon(xs, ys, 4, deck);
  fillRect(x - 42, fy - 83, x + 42, fy - 81, rgb565(130, 138, 150));
  for (int row = 0; row < 2; row++)
    for (int i = 0; i < 6; i++) fillCircle(x - 30 + i * 12, fy - 77 + row * 10, 4, dark);
  for (int i = 0; i < 5; i++) {
    int sx = x - 28 + i * 14;
    fillRect(sx - 2, fy - 56, sx + 2, fy - 53, dark);
    drawLine(sx, fy - 55, sx + ((i & 1) ? 3 : -3), fy - 61, rgb565(200, 204, 210), false);
  }
  for (int g = 0; g < 2; g++) {
    int gx = x + 4 + g * 26, gy = fy - 26;
    fillCircle(gx, gy, 11, dark);
    fillCircle(gx, gy, 9, rgb565(206, 206, 196));
    for (int k = 0; k < 5; k++) {
      float a = PI * (0.85f + 0.3f * k);
      drawPixel(gx + (int)(cosf(a) * 7), gy + (int)(sinf(a) * 7), dark);
    }
  }
  fillCircle(x - 30, fy - 26, 8, dark);
  fillCircle(x - 30, fy - 27, 6, rgb565(200, 40, 34));
}

static void paintDecor(const Decor &d) {
  switch (d.kind) {
    case DEC_CABINET: paintCabinet(d.x, d.y); break;
    case DEC_SAFE:    paintSafe(d.x, d.y); break;
    case DEC_PLANT:   paintPlant(d.x, d.y); break;
    case DEC_RADAR:   paintRadar(d.x, d.y); break;
    case DEC_PANEL:   paintPanel(d.x, d.y); break;
  }
}

// The lights that move on the equipment (radar sweep, panel lamps)
static void drawDecorAnim() {
  for (int i = 0; i < decorCount; i++) {
    const Decor &d = decor[i];
    if (d.kind == DEC_RADAR) {
      int cx = d.x, cy = d.y - 92 + 38;
      float a = clockT * 2.2f + d.seed;
      drawLineF(cx + 0.5f, cy + 0.5f, cx + 0.5f + cosf(a) * 24, cy + 0.5f + sinf(a) * 24, rgb565(90, 255, 140), 1.5f);
      float ba = (float)(d.seed % 628) / 100.0f;
      float k = fmodf(a - ba, TWO_PI);
      if (k < 0) k += TWO_PI;
      if (k < 2.5f) {
        int g = (int)(220 * (1.0f - k / 2.5f));
        fillCircle(cx + (int)(cosf(ba) * 15), cy + (int)(sinf(ba) * 15), 2, rgb565(40, 40 + g, 60));
      }
    } else if (d.kind == DEC_PANEL) {
      for (int row = 0; row < 2; row++)
        for (int k = 0; k < 6; k++) {
          int ph = (int)(clockT * 3.0f) + k * 7 + row * 13 + d.seed;
          bool on = ((ph * 2654435761u) >> 13) & 1;
          if (!on) continue;
          uint16_t col = (k + row) % 3 == 0 ? rgb565(255, 80, 60) : ((k + row) % 3 == 1 ? rgb565(80, 230, 120) : rgb565(255, 200, 70));
          fillCircle(d.x - 30 + k * 12, d.y - 77 + row * 10, 3, col);
        }
    }
  }
}

static void placeDecor(const RoomInfo &ri, const WideRoom &t) {
  uint32_t save = rngState;
  rngState = ri.seed * 2654435761u + 12345u;
  decorCount = 0;
  int order[MAX_SPOTS];
  for (int i = 0; i < t.spotCount; i++) order[i] = i;
  for (int i = t.spotCount - 1; i > 0; i--) {
    int j = rnd(i + 1), k = order[i];
    order[i] = order[j];
    order[j] = k;
  }
  bool used[DEC_KINDS] = {};
  for (int i = 0; i < t.spotCount && decorCount < DECOR_PER_ROOM; i++) {
    const Spot &s = t.spots[order[i]];
    int kind = rnd(DEC_KINDS);
    for (int k = 0; k < DEC_KINDS && used[kind]; k++) kind = (kind + 1) % DEC_KINDS;
    used[kind] = true;
    Decor &d = decor[decorCount++];
    d.kind = (uint8_t)kind;
    d.x = s.x;
    d.y = s.y;
    d.seed = (uint16_t)rnd32();
  }
  rngState = save;
}

static void paintRoom() {
  gfxTarget = roomBg;
  gfxClipAll();
  // back wall, tinted a little differently in each room
  int tint = ((rooms[curRoom].row * 5 + rooms[curRoom].col * 3) % 6) * 4;
  fillRect(0, HUD_H, SCREEN_W - 1, PLAY_Y1, rgb565(24 + tint, 30, 40 - tint / 2));
  uint16_t panel = rgb565(32 + tint, 40, 52 - tint / 2), seam = rgb565(14, 18, 24);
  for (int x = 0; x < SCREEN_W; x += 144) {
    fillRect(x + 6, CEIL_Y + 24, x + 137, ROOM_FLOOR_Y - 8, panel);
    fillRect(x, HUD_H, x + 3, ROOM_FLOOR_Y - 1, seam);
  }
  uint16_t rib = rgb565(44, 52, 66), rivet = rgb565(80, 92, 108);
  const int ribs[4] = { LEVEL_TOP - 90, LEVEL_2 - 90, LEVEL_1 - 90, ROOM_FLOOR_Y - 90 };
  for (int k = 0; k < 4; k++) {
    int y = ribs[k];
    fillRect(0, y, SCREEN_W - 1, y + 9, rib);
    for (int x = 18; x < SCREEN_W; x += 36) fillCircle(x, y + 4, 2, rivet);
  }
  // the ceiling: a steel beam with a pipe under it
  fillRect(0, HUD_H, SCREEN_W - 1, CEIL_Y - 1, rgb565(52, 58, 70));
  fillRect(0, CEIL_Y - 3, SCREEN_W - 1, CEIL_Y - 1, rgb565(30, 34, 42));
  paintPipe(CEIL_Y + 4, 14);
  if (signX > 0) paintRadiationSign(signX, signY, 40);
  // the room's number, stencilled on the back wall
  char b[16];
  snprintf(b, sizeof(b), "%d", curRoom + 1);
  drawVectorText(b, signX > 0 ? signX : SCREEN_W / 2, signY + 64, 3.5f, rgb565(70, 80, 96), true);
  for (int i = 0; i < liftCount; i++) paintLiftShaft(lifts[i]);
  for (int i = 0; i < platCount; i++) paintPlatform(plats[i]);
  for (int i = 0; i < liftCount; i++) {
    Plat f = { lifts[i].x0, lifts[i].x1, lifts[i].stops[lifts[i].stopCount - 1] };
    paintPlatform(f);
  }
  for (int i = 0; i < pitCount; i++) paintPit(pits[i][0], pits[i][1]);
  for (int i = 0; i < decorCount; i++) paintDecor(decor[i]);
  paintRoomSide(false, doorYL);
  paintRoomSide(true, doorYR);
  gfxTarget = g_frame;
}

// ============================================================ painting: the shaft
// Inside the lift car: back wall, side walls, ceiling and floor in perspective
static void paintCarInside() {
  int x0 = SHAFT_X0 + 13, x1 = SHAFT_X1 - 14;
  int y0 = CAR_FLOOR_Y - 184, y1 = CAR_FLOOR_Y - 1;
  const int d = 16;
  int bx0 = x0 + d, bx1 = x1 - d, by0 = y0 + d, by1 = y1 - 12;
  fillRect(bx0, by0, bx1, by1, rgb565(118, 124, 136));
  for (int x = bx0 + 2; x < bx1; x += 3) fillRect(x, by0, x, by1, rgb565(110, 116, 128));
  int mid = (bx0 + bx1) / 2;
  fillRect(mid, by0, mid + 1, by1, rgb565(84, 90, 102));
  fillRect(bx0, by0 + 70, bx1, by0 + 71, rgb565(84, 90, 102));
  // the light falling on the back wall
  for (int k = 0; k < 5; k++) blendRect(mid - 30 + k * 6, by0, mid + 30 - k * 6, by0 + 20 + k * 10, 250, 248, 230, 0.06f);
  int lx[4] = { x0, bx0, bx0, x0 }, ly[4] = { y0, by0, by1, y1 };
  fillPolygon(lx, ly, 4, rgb565(70, 76, 88));
  int rx[4] = { bx1 + 1, x1, x1, bx1 + 1 }, ry[4] = { by0, y0, y1, by1 };
  fillPolygon(rx, ry, 4, rgb565(96, 102, 114));
  int cx[4] = { x0, x1, bx1, bx0 }, cy[4] = { y0, y0, by0, by0 };
  fillPolygon(cx, cy, 4, rgb565(52, 56, 64));
  fillRect(mid - 18, y0 + 5, mid + 18, y0 + 9, rgb565(250, 248, 230));
  int fx[4] = { bx0, bx1 + 1, x1, x0 }, fy[4] = { by1 + 1, by1 + 1, y1, y1 };
  fillPolygon(fx, fy, 4, rgb565(40, 42, 48));
  for (int k = 1; k < 6; k++) {
    int xb = bx0 + (bx1 - bx0) * k / 6, xf = x0 + (x1 - x0) * k / 6;
    drawLine(xb, by1 + 1, xf, y1, rgb565(56, 58, 64), false);
  }
  int hy = by0 + 92;
  fillRect(bx0, hy, bx1, hy + 2, rgb565(196, 170, 110));
  drawLine(x0 + 2, hy + 10, bx0, hy, rgb565(196, 170, 110), true);
  drawLine(bx1, hy, x1 - 2, hy + 10, rgb565(196, 170, 110), true);
  int px0 = x1 - 12, py0 = y0 + 70;
  int bpx[4] = { px0 - 1, px0 + 7, px0 + 7, px0 - 1 }, bpy[4] = { py0 - 3, py0 - 8, py0 + 46, py0 + 42 };
  fillPolygon(bpx, bpy, 4, rgb565(30, 32, 38));
  for (int k = 0; k < 6; k++) fillRect(px0 + 1, py0 + k * 7, px0 + 4, py0 + k * 7 + 3, (k == 0) ? rgb565(255, 190, 60) : rgb565(150, 156, 168));
  fillRect(x0, y0, x0, y1, rgb565(40, 44, 52));
  fillRect(x1, y0, x1, y1, rgb565(40, 44, 52));
}

static void paintShaftPicture() {
  gfxTarget = shaftBg;
  gfxClipAll();
  fillRect(0, HUD_H, SCREEN_W - 1, PLAY_Y1, rgb565(10, 13, 20));
  int girders[4] = { 150, 380, 900, 1130 };
  for (int i = 0; i < 4; i++) {
    int x = girders[i];
    fillRect(x, HUD_H, x + 15, PLAY_Y1, rgb565(30, 36, 48));
    fillRect(x, HUD_H, x + 2, PLAY_Y1, rgb565(48, 56, 70));
  }
  fillRect(SHAFT_X0, HUD_H, SHAFT_X1 - 1, PLAY_Y1, rgb565(5, 7, 11));
  fillRect(SHAFT_X0, HUD_H, SHAFT_X0 + 5, PLAY_Y1, rgb565(96, 102, 116));
  fillRect(SHAFT_X1 - 6, HUD_H, SHAFT_X1 - 1, PLAY_Y1, rgb565(96, 102, 116));
  fillRect(SHAFT_X0 + 6, HUD_H, SHAFT_X0 + 7, PLAY_Y1, rgb565(54, 58, 68));
  fillRect(SHAFT_X1 - 8, HUD_H, SHAFT_X1 - 7, PLAY_Y1, rgb565(54, 58, 68));
  paintCarInside();
  gfxTarget = g_frame;
}

// The wall on one side of the shaft, from y0 to y1: brushed aluminium panels
// fixed to the shaft (seams and rivets anchored to the world, so they scroll
// past as the car moves)
static void paintShaftWall(int side, int y0, int y1) {
  uint16_t a = rgb565(118, 124, 132), b = rgb565(108, 114, 122);
  uint16_t seam = rgb565(74, 80, 88), hi = rgb565(156, 162, 170), rivet = rgb565(172, 178, 186);
  int top = y0 < HUD_H ? HUD_H : y0, bot = y1 > PLAY_Y1 ? PLAY_Y1 : y1;
  if (bot < top) return;
  int x0 = side ? SHAFT_X1 : 0, x1 = side ? SCREEN_W - 1 : SHAFT_X0 - 1;
  int w0 = (int)lroundf(carWorldY) - CAR_FLOOR_Y;     // world y of screen row 0
  fillRect(x0, top, x1, bot, a);
  for (int y = top; y <= bot; y++)
    if (((y + w0) % 3 + 3) % 3 == 0) fillRect(x0, y, x1, y, b);   // the brushed lines
  // horizontal seams every 110 units of the shaft
  int first = top - (((top + w0) % 110) + 110) % 110;
  for (int y = first; y <= bot; y += 110) {
    if (y >= top) {
      fillRect(x0, y, x1, y + 1, seam);
      if (y + 2 <= bot) fillRect(x0, y + 2, x1, y + 2, hi);
    }
    for (int x = x0 + 100; x < x1; x += 100)
      for (int ry = y + 16; ry < y + 110; ry += 34)
        if (ry >= top + 3 && ry <= bot - 3) {
          fillCircle(x - 6, ry, 2, rivet);
          fillCircle(x + 8, ry, 2, rivet);
        }
  }
  for (int x = x0 + 100; x < x1; x += 100) {
    fillRect(x, top, x + 1, bot, seam);
    fillRect(x + 2, top, x + 2, bot, hi);
  }
  // the edge at the shaft
  if (side == 0) fillRect(x1 - 5, top, x1, bot, rgb565(90, 96, 108));
  else fillRect(x0, top, x0 + 5, bot, rgb565(90, 96, 108));
}

// The doorway at the outer end of a catwalk, into a room
static void drawCatwalkDoor(bool right, int fy, int room) {
  int x0 = right ? SCREEN_W - 30 : 0, x1 = right ? SCREEN_W - 1 : 29;
  int top = fy - CAT_DOOR_H;
  uint16_t frame = rgb565(96, 104, 118), frameHi = rgb565(150, 158, 172);
  int fx0 = right ? x0 - 10 : x1 + 1;
  // the wall the door is in
  int wx0 = right ? x0 - 10 : 0, wx1 = right ? SCREEN_W - 1 : x1 + 10;
  fillRect(wx0, fy - CORR_H, wx1, fy - 1, rgb565(56, 60, 70));
  fillRect(x0, top, x1, fy - 1, rgb565(8, 10, 14));
  int lx = right ? SCREEN_W - 36 : 6;
  fillRect(lx, top - 20, lx + 30, top - 6, rgb565(20, 60, 30));
  fillRect(lx + 3, top - 17, lx + 27, top - 9, rgb565(60, 220, 110));
  // the room's number beside the door
  char b[16];
  snprintf(b, sizeof(b), "ROOM %d", room + 1);
  float w = vectorTextWidth(b, 2.2f);
  drawVectorText(b, right ? x0 - 22 - w / 2 : x1 + 22 + w / 2, top + 14, 2.2f, rgb565(170, 230, 180), false);
  fillRect(fx0, top, fx0 + 9, fy - 1, frame);
  fillRect(right ? fx0 : fx0 + 7, top, right ? fx0 + 2 : fx0 + 9, fy - 1, frameHi);
  fillRect(x0, top - 4, x1, top - 1, frame);
}

// A door on this side of the shaft at the car's floor? (side 0 = left)
static bool sideDoor(int side) { return roomOff(curShaft, carFloor, side == 1) >= 0; }

// The shaft: walls on both sides, and a catwalk out to each door - only at
// the floors where there is one. The car stays put on screen; all this scrolls.
static void drawShaftScene() {
  uint16_t deck = rgb565(80, 86, 98), deckHi = rgb565(150, 156, 168), truss = rgb565(54, 60, 72);
  uint16_t beam = rgb565(60, 66, 78), lamp = rgb565(255, 220, 140), rail = rgb565(110, 116, 130);
  auto screenY = [](int f) { return (int)lroundf(CAR_FLOOR_Y + (f * LEVEL_GAP - carWorldY)); };
  int ts = topStop(curShaft), bs = bottomStop(curShaft);
  int capY = screenY(ts) - CORR_H - 70;          // the head of the shaft, above its top stop
  int pitY = screenY(bs) + 14;                   // its bottom, under the last stop
  // rungs on the back of the shaft (they show the car moving)
  float off = fmodf(carWorldY, 60.0f);
  for (int k = -1; k < 14; k++) {
    int y = (int)lroundf(HUD_H + k * 60 - off);
    if (y >= CAR_FLOOR_Y - 194 && y <= CAR_FLOOR_Y + 14) continue;   // (the car is in front)
    if (y <= capY || y >= pitY) continue;
    drawLine(SHAFT_X0 + 10, y, SHAFT_X1 - 11, y, rgb565(28, 32, 42), false);
  }
  paintShaftWall(0, HUD_H, PLAY_Y1);
  paintShaftWall(1, HUD_H, PLAY_Y1);
  // the head of the shaft and its bottom
  if (capY >= HUD_H) {
    fillRect(SHAFT_X0, HUD_H, SHAFT_X1 - 1, capY, rgb565(56, 60, 70));
    hazardBand(SHAFT_X0, SHAFT_X1 - 1, capY - 12, capY);
    fillCircle((SHAFT_X0 + SHAFT_X1) / 2, capY - 30, 9, rgb565(30, 32, 38));   // the pulley
  }
  if (pitY <= PLAY_Y1) {
    fillRect(SHAFT_X0, pitY, SHAFT_X1 - 1, PLAY_Y1, rgb565(14, 16, 20));
    hazardBand(SHAFT_X0 + 8, SHAFT_X1 - 9, pitY, pitY + 8);
  }
  for (int L = 0; L < FLOORS; L++) {
    int fy = screenY(L);
    if (fy < HUD_H - 40 || fy - CORR_H - 20 > PLAY_Y1) continue;
    int cy = fy - CORR_H;
    char name[8];
    snprintf(name, sizeof(name), "%d", L + 1);
    for (int side = 0; side < 2; side++) {
      int room = roomOff(curShaft, L, side == 1);
      if (room < 0) continue;                      // no door: the wall carries on
      int x0 = side ? SHAFT_X1 : 0, x1 = side ? SCREEN_W - 1 : SHAFT_X0 - 1;
      // the bay: dark behind, girders standing in it
      fillRect(x0, cy, x1, fy + 32, rgb565(10, 13, 20));
      for (int gx = x0 + 150; gx < x1 - 40; gx += 230) {
        fillRect(gx, cy, gx + 15, fy - 1, rgb565(30, 36, 48));
        fillRect(gx, cy, gx + 2, fy - 1, rgb565(48, 56, 70));
      }
      // catwalk deck and the truss under it
      fillRect(x0, fy, x1, fy + 13, deck);
      fillRect(x0, fy, x1, fy + 2, deckHi);
      for (int x = x0; x < x1; x += 30) drawLine(x, fy + 14, x + 15, fy + 32, truss, false);
      drawLine(x0, fy + 32, x1, fy + 32, truss, false);
      // the beam above, with lamps under it
      fillRect(x0, cy - 12, x1, cy, beam);
      fillRect(x0, cy - 12, x1, cy - 10, deckHi);
      for (int x = x0 + 50; x < x1 - 20; x += 120) fillRect(x, cy + 1, x + 18, cy + 4, lamp);
      // the door at the far end, then the hand rail in front of the wall
      drawCatwalkDoor(side == 1, fy, room);
      int r0 = side ? x0 : 40, r1 = side ? x1 - 40 : x1;
      drawLine(r0, fy - 44, r1, fy - 44, rail, true);
      for (int x = r0 + 20; x < r1; x += 40) drawLine(x, fy - 44, x, fy - 1, rail, false);
      // the floor's number, near the shaft, and the hazard edge
      drawVectorText(name, side ? SHAFT_X1 + 70 : SHAFT_X0 - 70, cy + 34, 5.0f, rgb565(255, 190, 70), true);
      if (side == 0) fillRect(SHAFT_X0 - 8, fy, SHAFT_X0 - 1, fy + 5, rgb565(230, 186, 36));
      else fillRect(SHAFT_X1, fy, SHAFT_X1 + 7, fy + 5, rgb565(230, 186, 36));
    }
  }
  // the car: cables, roof, cage and floor (it stays put on screen)
  int cx0 = SHAFT_X0 + 8, cx1 = SHAFT_X1 - 9;
  int cableTop = capY > HUD_H ? capY : HUD_H;
  drawLine(SHAFT_X0 + 30, cableTop, SHAFT_X0 + 30, CAR_FLOOR_Y - 190, rgb565(120, 126, 138), false);
  drawLine(SHAFT_X1 - 31, cableTop, SHAFT_X1 - 31, CAR_FLOOR_Y - 190, rgb565(120, 126, 138), false);
  fillRect(cx0, CAR_FLOOR_Y - 192, cx1, CAR_FLOOR_Y - 184, rgb565(110, 116, 130));
  fillRect(cx0, CAR_FLOOR_Y - 184, cx0 + 4, CAR_FLOOR_Y - 1, rgb565(90, 96, 110));
  fillRect(cx1 - 4, CAR_FLOOR_Y - 184, cx1, CAR_FLOOR_Y - 1, rgb565(90, 96, 110));
  fillRect(cx0, CAR_FLOOR_Y, cx1, CAR_FLOOR_Y + 13, rgb565(150, 156, 168));
  fillRect(cx0, CAR_FLOOR_Y, cx1, CAR_FLOOR_Y + 2, rgb565(205, 210, 220));
  fillRect(cx0, CAR_FLOOR_Y + 10, cx1, CAR_FLOOR_Y + 13, rgb565(230, 186, 36));
  uint16_t carLamp = carMoving ? rgb565(255, 170, 40) : rgb565(80, 230, 120);
  fillRect((cx0 + cx1) / 2 - 5, CAR_FLOOR_Y + 5, (cx0 + cx1) / 2 + 5, CAR_FLOOR_Y + 8, carLamp);
}

// ============================================================ rooms and shafts: building and moving between
static void buildRoom(int idx, bool fromLeft) {
  const RoomInfo &ri = rooms[idx];
  const WideRoom &t = wideRooms[ri.tpl];
  platCount = t.platCount;
  for (int i = 0; i < platCount; i++) plats[i] = t.plats[i];
  pitCount = t.pitCount;
  for (int i = 0; i < pitCount; i++) {
    pits[i][0] = t.pits[i][0];
    pits[i][1] = t.pits[i][1];
  }
  doorYL = ri.topL ? LEVEL_TOP : ROOM_FLOOR_Y;
  doorYR = ri.topR ? LEVEL_TOP : ROOM_FLOOR_Y;
  int enterY = fromLeft ? doorYL : doorYR;
  liftCount = t.liftCount;
  for (int i = 0; i < liftCount; i++) {
    Lift &l = lifts[i];
    const TLift &tl = t.lifts[i];
    l.x0 = tl.x0;
    l.x1 = tl.x1;
    l.stopCount = tl.n;
    for (int s = 0; s < tl.n; s++) l.stops[s] = tl.stops[s];
    // waiting at the stop nearest the door he came in by
    int best = 0;
    for (int s = 1; s < tl.n; s++)
      if (abs(tl.stops[s] - enterY) < abs(tl.stops[best] - enterY)) best = s;
    l.at = (int8_t)best;
    l.y = tl.stops[best];
    l.moving = false;
  }
  signX = t.signX;
  signY = t.signY;
  placeDecor(ri, t);
}

static void placeAgent(float x, float y, int facing) {
  agX = x;
  agY = y;
  agFacing = facing;
  agLift = -1;
  agVX = agVY = 0;
  bool running = (held & (facing > 0 ? BTN_RIGHT : BTN_LEFT)) != 0;
  agentSample(running ? ANIM_RUN : ANIM_IDLE, 0, shownPose);
  blendLen = 0;
  agState = running ? ST_RUN : ST_IDLE;
  agT = 0;
  entryX = x;
  entryY = y;
  entryFacing = facing;
  searchItem = -1;
}

static void enterRoom(int idx, bool fromLeft) {
  mode = MODE_ROOM;
  curRoom = idx;
  buildRoom(idx, fromLeft);
  paintRoom();
  rooms[idx].visited = true;
  int y = fromLeft ? doorYL : doorYR;
  placeAgent(fromLeft ? 36 : SCREEN_W - 36, (float)y, fromLeft ? 1 : -1);
}

static void enterShaft(int s, int f, bool fromLeft) {
  mode = MODE_SHAFT;
  curShaft = s;
  carFloor = f;
  carWorldY = (float)(f * LEVEL_GAP);
  carMoving = false;
  platCount = liftCount = pitCount = 0;
  decorCount = 0;
  shaftSeen[s][f] = true;
  placeAgent(fromLeft ? 30 : SCREEN_W - 30, CAR_FLOOR_Y, fromLeft ? 1 : -1);
}

// Walked off the screen edge: where does that lead?
static void leaveThroughEdge(bool right) {
  if (mode == MODE_SHAFT) {
    int idx = roomOff(curShaft, carFloor, right);
    if (idx < 0) return;
    enterRoom(idx, right);             // going right: in through the room's left door
    return;
  }
  const RoomInfo &ri = rooms[curRoom];
  if (right) enterShaft(ri.col + 1, ri.row * 2 + (ri.topR ? 0 : 1), true);
  else enterShaft(ri.col, ri.row * 2 + (ri.topL ? 0 : 1), false);
}

// ============================================================ the agent's world
static bool standOn(float x, float y, int &liftOut) {
  liftOut = -1;
  if (mode == MODE_SHAFT) return fabsf(y - CAR_FLOOR_Y) < 0.5f;   // the catwalk at this floor, and the car
  for (int i = 0; i < liftCount; i++) {
    const Lift &l = lifts[i];
    if (x >= l.x0 && x <= l.x1 && fabsf(l.y - y) < 0.5f) {
      liftOut = i;
      return true;
    }
  }
  for (int i = 0; i < platCount; i++) {
    const Plat &p = plats[i];
    if (fabsf(p.y - y) < 0.5f && x >= p.x0 - EDGE_GRACE_PX && x <= p.x1 + EDGE_GRACE_PX) return true;
  }
  for (int i = 0; i < liftCount; i++) {
    const Lift &l = lifts[i];
    if (x < l.x0 - EDGE_GRACE_PX || x > l.x1 + EDGE_GRACE_PX) continue;
    if (fabsf(l.y - y) < 0.5f) {
      liftOut = i;
      return true;
    }
    if (fabsf(l.stops[l.stopCount - 1] - y) < 0.5f) return true;
  }
  return false;
}

static float landingTop(float x, float y0, float y1, int &liftOut) {
  float best = 1e9f;
  liftOut = -1;
  if (mode == MODE_SHAFT) {
    if (CAR_FLOOR_Y >= y0 - 0.5f && CAR_FLOOR_Y <= y1) return CAR_FLOOR_Y;
    return -1.0f;
  }
  for (int i = 0; i < platCount; i++) {
    const Plat &p = plats[i];
    if (x < p.x0 - EDGE_GRACE_PX || x > p.x1 + EDGE_GRACE_PX) continue;
    if (p.y >= y0 - 0.5f && p.y <= y1 && p.y < best) {
      best = p.y;
      liftOut = -1;
    }
  }
  for (int i = 0; i < liftCount; i++) {
    const Lift &l = lifts[i];
    if (x < l.x0 - EDGE_GRACE_PX || x > l.x1 + EDGE_GRACE_PX) continue;
    if (l.y >= y0 - 0.5f && l.y <= y1 && l.y < best) {
      best = l.y;
      liftOut = i;
    }
    float f = l.stops[l.stopCount - 1];
    if (f >= y0 - 0.5f && f <= y1 && f < best) {
      best = f;
      liftOut = (fabsf(l.y - f) < 0.5f) ? i : -1;
    }
  }
  return best < 1e8f ? best : -1.0f;
}

static float headroom(float x0, float x1, float y) {
  if (mode == MODE_SHAFT) return CORR_H - 14;
  if (x0 > x1) { float t = x0; x0 = x1; x1 = t; }
  float room = y - CEIL_Y;
  for (int i = 0; i < platCount; i++) {
    const Plat &p = plats[i];
    if (p.x1 < x0 || p.x0 > x1) continue;
    float under = p.y + PLAT_T;
    if (under <= y - 1 && y - under < room) room = y - under;
  }
  for (int i = 0; i < liftCount; i++) {
    const Lift &l = lifts[i];
    if (l.x1 < x0 || l.x0 > x1) continue;
    float under = l.y + PAD_H;
    if (under <= y - 1 && y - under < room) room = y - under;
  }
  return room;
}

static bool inCar(float x) { return mode == MODE_SHAFT && x > SHAFT_X0 + 14 && x < SHAFT_X1 - 14; }

static bool exitOpen(bool right) {
  if (mode == MODE_SHAFT) return !carMoving && roomOff(curShaft, carFloor, right) >= 0;
  return fabsf(agY - (right ? doorYR : doorYL)) < 0.5f;
}

static float edgeMin() {
  if (mode == MODE_SHAFT && !sideDoor(0)) return SHAFT_X0 + 20;   // no door on this side: the wall
  return 34.0f * myScale;
}
static float edgeMax() {
  if (mode == MODE_SHAFT && !sideDoor(1)) return SHAFT_X1 - 20;
  return SCREEN_W - 34.0f * myScale;
}

static void setState(AgentState s, float blend) {
  blendFrom = shownPose;
  blendT = 0;
  blendLen = blend;
  agState = s;
  agT = 0;
}

static AgentAnim animFor(AgentState s) {
  switch (s) {
    case ST_RUN:  return ANIM_RUN;
    case ST_JUMP: return ANIM_JUMP;
    case ST_LAND: return ANIM_JUMP;
    case ST_USE:  return ANIM_USE;
    case ST_FALL: return ANIM_FALL;
    case ST_KO:   return ANIM_DEAD;
    default:      return ANIM_IDLE;
  }
}

static void startFall(float vx, float vy) {
  agVX = vx;
  agVY = vy;
  fallFromY = agY;
  agLift = -1;
  setState(ST_FALL, 0.15f);
}

static void startJump() {
  jumpY0 = agY;
  float dist = JUMP_DISTANCE * myScale * agFacing;
  float room = headroom(agX - 20, agX + dist + 20 * agFacing, agY) / myScale;
  float allowed = room - 86.0f;
  jumpLiftScale = clampv(allowed / AG_JUMP_HEIGHT, 0.15f, 1.0f);
  setState(ST_JUMP, 0.08f);
}

static void knockOut() {
  agLift = -1;
  setState(ST_KO, 0.10f);
}

static void respawn() {
  if (mode == MODE_ROOM) {
    buildRoom(curRoom, entryFacing > 0);   // (the lift pads back where they were)
  }
  float x = clampv(entryX, edgeMin(), edgeMax());
  placeAgent(x, entryY, entryFacing);
}

static bool callLift(int i, int dir) {
  Lift &l = lifts[i];
  if (l.moving) return false;
  int next = l.at + dir;
  if (next < 0 || next >= l.stopCount) return false;
  l.at = (int8_t)next;
  l.moving = true;
  return true;
}

// The lift car: to the next stop up (dir -1) or down (+1), past the walled-off floors
static bool callCar(int dir) {
  if (carMoving) return false;
  int next = carFloor + dir;
  while (next >= 0 && next < FLOORS && !isStop(curShaft, next)) next += dir;
  if (next < 0 || next >= FLOORS) return false;
  carFloor = next;
  carMoving = true;
  carSpeed = 0;
  return true;
}

static void updateLifts(float dt) {
  for (int i = 0; i < liftCount; i++) {
    Lift &l = lifts[i];
    if (!l.moving) continue;
    float target = l.stops[l.at];
    float step = LIFT_SPEED * dt;
    if (fabsf(target - l.y) <= step) {
      l.y = target;
      l.moving = false;
    } else {
      l.y += (target > l.y) ? step : -step;
    }
  }
  if (mode == MODE_SHAFT && carMoving) {
    float target = (float)(carFloor * LEVEL_GAP);
    float dist = fabsf(target - carWorldY);
    carSpeed = fminf(fmaxf(carSpeed, CAR_SPEED) + CAR_ACCEL * dt, CAR_MAX_SPEED);
    float v = fminf(carSpeed, fmaxf(sqrtf(2.0f * CAR_ACCEL * dist), CAR_SPEED * 0.5f));   // (slowing down for the stop)
    float step = v * dt;
    if (dist <= step) {
      carWorldY = target;
      carMoving = false;
      carSpeed = 0;
      shaftSeen[curShaft][carFloor] = true;
    } else {
      carWorldY += (target > carWorldY) ? step : -step;
    }
  }
}

// The piece of equipment he is standing at, or -1
static int decorAt() {
  if (mode != MODE_ROOM) return -1;
  for (int i = 0; i < decorCount; i++) {
    const Decor &d = decor[i];
    if (fabsf(agY - d.y) < 0.5f && fabsf(agX - d.x) < decorW[d.kind] / 2 + 14) return i;
  }
  return -1;
}

static void updateAgent(float dt) {
  bool L = held & BTN_LEFT, R = held & BTN_RIGHT;
  int want = (R && !L) ? 1 : ((L && !R) ? -1 : 0);
  int upDown = (held & BTN_UP) ? -1 : ((held & BTN_DOWN) ? 1 : 0);
  float oldT = agT;
  agT += dt;
  blendT += dt;

  if (agState == ST_IDLE || agState == ST_RUN || agState == ST_USE || agState == ST_LAND) {
    int lift;
    if (!standOn(agX, agY, lift)) {
      float vx = (agState == ST_RUN) ? agFacing * RUN_SPEED * myScale * 0.45f : 0.0f;
      startFall(vx, 0);
    } else {
      agLift = lift;
    }
  }

  switch (agState) {
    case ST_IDLE:
      if (upDown != 0 && agLift >= 0 && callLift(agLift, upDown)) {
        setState(ST_LIFT, 0.1f);
      } else if (upDown != 0 && inCar(agX) && callCar(upDown)) {
        setState(ST_LIFT, 0.1f);
      } else if (pressed & BTN_JUMP) {
        if (want) agFacing = want;
        startJump();
      } else if (held & BTN_USE) {
        searchT = 0;
        searchItem = -1;
        setState(ST_USE, 0.18f);
      } else if (want) {
        agFacing = want;
        bool blocked = (want > 0) ? (agX >= edgeMax() && !exitOpen(true)) : (agX <= edgeMin() && !exitOpen(false));
        if (!blocked) setState(ST_RUN, BLEND_SECONDS);
      }
      break;

    case ST_RUN: {
      if (pressed & BTN_JUMP) {
        startJump();
        break;
      }
      if (want == 0) {
        setState(ST_IDLE, 0.15f);
        break;
      }
      agFacing = want;
      agX += agFacing * RUN_SPEED * myScale * dt;
      if (agX < -6 && exitOpen(false)) { leaveThroughEdge(false); return; }
      if (agX > SCREEN_W + 6 && exitOpen(true)) { leaveThroughEdge(true); return; }
      bool openSide = (agFacing > 0) ? exitOpen(true) : exitOpen(false);
      if (!openSide && (agX <= edgeMin() || agX >= edgeMax())) {
        agX = clampv(agX, edgeMin(), edgeMax());
        setState(ST_IDLE, 0.15f);
      }
      break;
    }

    case ST_LIFT: {
      bool stillMoving;
      if (mode == MODE_SHAFT) {
        agY = CAR_FLOOR_Y;
        stillMoving = carMoving;
      } else {
        Lift &l = lifts[agLift];
        agY = l.y;
        stillMoving = l.moving;
      }
      if (!stillMoving) {
        bool again = false;
        if (upDown != 0) again = (mode == MODE_SHAFT) ? callCar(upDown) : callLift(agLift, upDown);
        if (!again) setState(ST_IDLE, 0.1f);
      }
      break;
    }

    case ST_JUMP: {
      if (oldT < AG_JUMP_UP && agT >= AG_JUMP_UP) agLift = -1;
      if (agT > AG_JUMP_UP && oldT < AG_JUMP_DOWN) {
        float t0 = fmaxf(oldT, AG_JUMP_UP), t1 = fminf(agT, AG_JUMP_DOWN);
        agX += agFacing * JUMP_DISTANCE * myScale * (t1 - t0) / (AG_JUMP_DOWN - AG_JUMP_UP);
        agX = clampv(agX, edgeMin(), edgeMax());
      }
      if (oldT < AG_JUMP_DOWN && agT >= AG_JUMP_DOWN) {
        int lift;
        agY = jumpY0;
        if (standOn(agX, jumpY0, lift)) {
          agLift = lift;
        } else {
          float vx = agFacing * JUMP_DISTANCE * myScale / (AG_JUMP_DOWN - AG_JUMP_UP) * 0.5f;
          float vy = AG_JUMP_HEIGHT * jumpLiftScale * myScale * PI / (AG_JUMP_DOWN - AG_JUMP_UP);
          startFall(vx, vy);
          fallFromY = jumpY0 - AG_JUMP_HEIGHT * jumpLiftScale * myScale;
          break;
        }
      }
      if (agT >= AG_JUMP_TIME) {
        if (want) {
          agFacing = want;
          setState(ST_RUN, 0.10f);
        } else {
          setState(ST_IDLE, 0.15f);
        }
      }
      break;
    }

    case ST_FALL: {
      float y0 = agY;
      agVY = fminf(agVY + GRAVITY * dt, FALL_MAX_SPEED);
      agY += agVY * dt;
      agX += agVX * dt;
      if (agX < edgeMin() || agX > edgeMax()) {
        agX = clampv(agX, edgeMin(), edgeMax());
        agVX = 0;
      }
      int lift;
      float top = landingTop(agX, y0, agY, lift);
      if (top >= 0) {
        agY = top;
        agLift = lift;
        if (agY - fallFromY > FALL_SAFE_PX) knockOut();
        else setState(ST_LAND, 0.06f);
      } else if (agY > SCREEN_H + 160) {
        knockOut();
      }
      break;
    }

    case ST_LAND:
      if (agT >= AG_JUMP_TIME - AG_JUMP_DOWN) {
        if (want) {
          agFacing = want;
          setState(ST_RUN, 0.08f);
        } else {
          setState(ST_IDLE, 0.12f);
        }
      }
      break;

    case ST_USE: {
      int i = decorAt();
      if (i != searchItem) {
        searchItem = i;
        searchT = 0;
      }
      if (i >= 0 && searchT < SEARCH_SECONDS) {
        searchT += dt;
        if (searchT >= SEARCH_SECONDS) {
          rooms[curRoom].searched[i] = true;
          nothingShowT = NOTHING_SHOW;   // (nothing hidden anywhere yet)
          nothingX = decor[i].x;
          nothingY = decor[i].y - decorH[decor[i].kind] - 22;
        }
      }
      if (!(held & BTN_USE)) {
        searchItem = -1;
        setState(ST_IDLE, 0.18f);
      }
      break;
    }

    case ST_KO:
      if (agT >= RESPAWN_SECONDS) respawn();
      break;
  }

  AgentPose target;
  float animT = agT;
  if (agState == ST_LAND) animT = AG_JUMP_DOWN + agT;
  agentSample(animFor(agState), animT, target);
  if (agState == ST_JUMP) target.lift *= jumpLiftScale;
  if (agState == ST_LAND) target.spin = 0;
  if (blendLen > 0 && blendT < blendLen) {
    float k = blendT / blendLen;
    k = k * k * (3.0f - 2.0f * k);
    agentLerp(blendFrom, target, k, shownPose);
  } else {
    shownPose = target;
  }
}

// ============================================================ drawing each frame
static void drawLiftPad(const Lift &l) {
  int top = shaftTop(l);
  int y = (int)lroundf(l.y);
  int cx = (l.x0 + l.x1) / 2;
  drawLine(cx - 18, top, cx - 18, y - 1, rgb565(120, 126, 138), false);
  drawLine(cx + 18, top, cx + 18, y - 1, rgb565(120, 126, 138), false);
  int x0 = l.x0 + 6, x1 = l.x1 - 7;
  fillRect(x0, y, x1, y + PAD_H - 1, rgb565(150, 156, 168));
  fillRect(x0, y, x1, y + 2, rgb565(205, 210, 220));
  fillRect(x0, y + PAD_H - 4, x1, y + PAD_H - 1, rgb565(230, 186, 36));
  for (int x = x0 + 4; x < x1 - 4; x += 12) fillRect(x, y + PAD_H - 4, x + 5, y + PAD_H - 1, rgb565(20, 20, 22));
  uint16_t lamp = l.moving ? rgb565(255, 170, 40) : rgb565(80, 230, 120);
  fillRect(cx - 4, y + 4, cx + 3, y + 6, lamp);
}

static void drawSearchMarks() {
  const RoomInfo &ri = rooms[curRoom];
  for (int i = 0; i < decorCount; i++) {
    const Decor &d = decor[i];
    int top = d.y - decorH[d.kind];
    if (ri.searched[i]) {
      uint16_t c = rgb565(80, 230, 120);
      int tx = d.x - 6, ty = top - 12;
      drawLine(tx, ty, tx + 4, ty + 5, c, true);
      drawLine(tx + 4, ty + 5, tx + 13, ty - 6, c, true);
    }
  }
  if (agState == ST_USE && searchItem >= 0 && searchT < SEARCH_SECONDS) {
    const Decor &d = decor[searchItem];
    int x0 = d.x - 36, y0 = d.y - decorH[d.kind] - 30;
    drawRectOutline(x0, y0, x0 + 72, y0 + 9, rgb565(220, 220, 230), 1);
    fillRect(x0 + 2, y0 + 2, x0 + 2 + (int)(68 * searchT / SEARCH_SECONDS), y0 + 7, rgb565(120, 200, 255));
  }
  if (nothingShowT > 0)
    drawVectorText("NOTHING", nothingX, nothingY - (NOTHING_SHOW - nothingShowT) * 20, 2.5f, rgb565(170, 170, 180), false);
}

static void drawHud() {
  gfxClipAll();
  fillRect(0, 0, SCREEN_W - 1, HUD_H - 1, rgb565(10, 12, 16));
  fillRect(0, HUD_H - 2, SCREEN_W - 1, HUD_H - 1, rgb565(44, 50, 62));
  char b[64];
  if (mode == MODE_SHAFT) snprintf(b, sizeof(b), "LIFT %c  FLOOR %d", shaftLetter(curShaft), carFloor + 1);
  else snprintf(b, sizeof(b), "ROOM %d", curRoom + 1);
  float w = vectorTextWidth(b, 2.6f);
  drawVectorText(b, 24 + w / 2, HUD_H / 2, 2.6f, rgb565(255, 190, 70), true);
  int seen = 0;
  for (int i = 0; i < ROOM_COUNT; i++) seen += rooms[i].visited;
  snprintf(b, sizeof(b), "ROOMS %d/%d", seen, ROOM_COUNT);
  drawVectorText(b, SCREEN_W / 2, HUD_H / 2, 2.6f, rgb565(200, 206, 220), true);
  snprintf(b, sizeof(b), "V%s  FPS %d", GAME_VERSION, (int)lroundf(fpsShown));
  w = vectorTextWidth(b, 2.0f);
  drawVectorText(b, SCREEN_W - 24 - w / 2, HUD_H / 2, 2.0f, rgb565(110, 120, 140), false);
}

static void drawMessage() {
  if (msgT <= 0) return;
  float w = vectorTextWidth(msgText, 3.0f);
  int x0 = (int)(SCREEN_W / 2 - w / 2 - 24), x1 = (int)(SCREEN_W / 2 + w / 2 + 24);
  blendRect(x0, HUD_H + 16, x1, HUD_H + 60, 0, 0, 0, 0.6f);
  drawRectOutline(x0, HUD_H + 16, x1, HUD_H + 60, rgb565(90, 100, 120), 1);
  drawVectorText(msgText, SCREEN_W / 2, HUD_H + 38, 3.0f, rgb565(240, 240, 245), true);
}

// The map: every room, its doors and the shafts they lead to
static void drawMap() {
  gfxClipAll();
  blendRect(0, HUD_H, SCREEN_W - 1, SCREEN_H - 1, 4, 6, 10, 0.88f);
  const int RW = 96, SW = 30, RH = 60, GAP = 22;
  const int left = (SCREEN_W - (COLS * RW + SHAFTS * SW)) / 2, top = 124;
  bool blink = fmodf(clockT, 0.8f) < 0.5f;
  drawVectorText("THE COMPLEX", SCREEN_W / 2, 72, 4.0f, rgb565(255, 190, 70), true);
  auto floorY = [&](int f) { int r = f / 2; return top + r * (RH + GAP) + ((f & 1) ? RH - 12 : 12); };
  // the shafts: from their top stop to their bottom one, a mark at each stop
  for (int s = 0; s < SHAFTS; s++) {
    int x0 = left + s * (RW + SW);
    int ts = topStop(s), bs = bottomStop(s);
    char b[2] = { shaftLetter(s), 0 };
    drawVectorText(b, x0 + SW / 2, top - 30, 2.2f, rgb565(200, 206, 220), true);
    if (ts < 0) continue;
    int y0 = floorY(ts) - 14, y1 = floorY(bs) + 14;
    fillRect(x0 + 6, y0, x0 + SW - 7, y1, rgb565(26, 30, 40));
    drawRectOutline(x0 + 6, y0, x0 + SW - 7, y1, rgb565(70, 78, 96), 1);
    for (int f = ts; f <= bs; f++) {
      if (!isStop(s, f)) continue;
      int y = floorY(f);
      fillRect(x0 + 9, y, x0 + SW - 10, y + 1, shaftSeen[s][f] ? rgb565(150, 160, 180) : rgb565(60, 66, 80));
    }
  }
  // the rooms, and a corridor from each door to its shaft
  for (int i = 0; i < ROOM_COUNT; i++) {
    const RoomInfo &ri = rooms[i];
    int x0 = left + SW + ri.col * (RW + SW), y0 = top + ri.row * (RH + GAP);
    uint16_t fill = ri.visited ? rgb565(40, 64, 96) : rgb565(22, 26, 34);
    uint16_t edge = ri.visited ? rgb565(110, 160, 220) : rgb565(70, 78, 96);
    fillRect(x0, y0, x0 + RW - 1, y0 + RH - 1, fill);
    drawRectOutline(x0, y0, x0 + RW - 1, y0 + RH - 1, edge, 2);
    uint16_t door = rgb565(80, 230, 120);
    int yl = floorY(ri.row * 2 + (ri.topL ? 0 : 1)), yr = floorY(ri.row * 2 + (ri.topR ? 0 : 1));
    fillRect(x0 - SW / 2 + 6, yl - 2, x0 + 8, yl + 3, door);
    fillRect(x0 + RW - 9, yr - 2, x0 + RW + SW / 2 - 6, yr + 3, door);
    char b[8];
    snprintf(b, sizeof(b), "%d", i + 1);
    drawVectorText(b, x0 + RW / 2, y0 + RH / 2, 2.4f, ri.visited ? rgb565(230, 236, 245) : rgb565(110, 118, 136), true);
    if (mode == MODE_ROOM && i == curRoom && blink)
      drawRectOutline(x0 - 4, y0 - 4, x0 + RW + 3, y0 + RH + 3, rgb565(255, 190, 70), 3);
  }
  // him, in a shaft
  if (mode == MODE_SHAFT) {
    int x = left + curShaft * (RW + SW) + SW / 2;
    float f = carWorldY / LEVEL_GAP;
    int f0 = (int)f;
    float k = f - f0;
    float y = floorY(f0) + (f0 + 1 < FLOORS ? (floorY(f0 + 1) - floorY(f0)) * k : 0);
    fillCircle(x, (int)y, 9, rgb565(10, 12, 16));
    fillCircle(x, (int)y, 7, blink ? rgb565(255, 190, 70) : rgb565(255, 240, 200));
  }
  int bottom = top + ROWS * RH + (ROWS - 1) * GAP;
  drawVectorText("EACH ROOM: A DOOR AT THE TOP OR THE BOTTOM ON EACH SIDE (GREEN) - THE LIFTS STOP ONLY AT DOORS", SCREEN_W / 2,
                 bottom + 40, 1.8f, rgb565(150, 158, 176), false);
  drawVectorText("OPTIONS: BACK TO THE GAME     TRIANGLE: LAY OUT A NEW COMPLEX", SCREEN_W / 2, bottom + 70, 2.2f,
                 rgb565(200, 206, 220), true);
}

// ============================================================ the game
void gameInit(uint32_t seed) {
  gfxInit();
  layOut(seed);
  paintShaftPicture();
  enterShaft(START_SHAFT, topStop(START_SHAFT), true);
  placeAgent((SHAFT_X0 + SHAFT_X1) / 2, CAR_FLOOR_Y, 1);
  showMsg("OPTIONS: THE MAP", 5.0f);
}

static void newComplex() {
  layOut(layoutSeed * 7919u + 13u);
  enterShaft(START_SHAFT, topStop(START_SHAFT), true);
  placeAgent((SHAFT_X0 + SHAFT_X1) / 2, CAR_FLOOR_Y, 1);
}

void gameFrame(float dt, uint32_t heldNow, uint32_t pressedNow) {
  if (dt > 0.05f) dt = 0.05f;          // (after a pause: no big jumps)
  held = heldNow;
  pressed = pressedNow;
  clockT += dt;
  fpsAcc += dt;
  fpsFrames++;
  if (fpsAcc >= 0.5f) {
    fpsShown = fpsFrames / fpsAcc;
    fpsAcc = 0;
    fpsFrames = 0;
  }

  if (pressed & BTN_MAP) mapOn = !mapOn;
  if (mapOn) {
    if (pressed & BTN_NEW) newComplex();
    pressed = 0;
  } else {
    if (msgT > 0) msgT -= dt;
    if (nothingShowT > 0) nothingShowT -= dt;
    updateLifts(dt);
    updateAgent(dt);
  }

  // the picture
  gfxTarget = g_frame;
  if (mode == MODE_ROOM) {
    memcpy(g_frame, roomBg, sizeof(roomBg));
    gfxClip(HUD_H, PLAY_Y1);
    for (int i = 0; i < liftCount; i++) drawLiftPad(lifts[i]);
    drawDecorAnim();
    drawSearchMarks();
  } else {
    memcpy(g_frame, shaftBg, sizeof(shaftBg));
    gfxClip(HUD_H, PLAY_Y1);
    drawShaftScene();
  }
  gfxClip(HUD_H, PLAY_Y1);
  drawAgentU(shownPose, agX, agY, agFacing, myScale);
  if (mapOn) drawMap();
  else drawMessage();
  drawHud();
}

// For the PC test build: where he is and what he is doing
const char *gameDebug() {
  static char b[160];
  static const char *st[] = { "IDLE", "RUN", "JUMP", "USE", "FALL", "LAND", "LIFT", "KO" };
  if (mode == MODE_SHAFT)
    snprintf(b, sizeof(b), "shaft %c floor %d%s x=%.0f y=%.0f %s", shaftLetter(curShaft), carFloor + 1, carMoving ? " (moving)" : "", agX, agY, st[agState]);
  else
    snprintf(b, sizeof(b), "room %d (row %d col %d, doors L:%s R:%s) x=%.0f y=%.0f %s", curRoom + 1, rooms[curRoom].row, rooms[curRoom].col,
             rooms[curRoom].topL ? "top" : "bottom", rooms[curRoom].topR ? "top" : "bottom", agX, agY, st[agState]);
  return b;
}
void gameDebugPos(float *x, float *y, int *state) {
  *x = agX;
  *y = agY;
  *state = (int)agState;
}
