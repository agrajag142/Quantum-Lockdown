// rooms_wide.h
// Room layouts for the full-screen game. A room is the whole screen: 1280
// units wide (1920 pixels) with FOUR levels: the floor at y = 680, and 520,
// 360 and 200 above it - one level is 160 units, as in NUCLEAR MISSION, sized
// for the agent at scale 1.3. The ceiling is at 56.
//
// Every room has a door on each side, at its TOP (the top level, y = 200) or
// at its BOTTOM (the floor, y = 680): which, is decided for each room when the
// complex is laid out. So a layout must work with any of the four combinations:
//   - the floor reaches both screen edges (x = 0 and x = 1280)
//   - a top level platform reaches both screen edges
//   - both doors' ledges, and every spot, can be reached from both doors
//     (on foot, by lift pad, by somersaulting a gap, or a drop of one level:
//     a drop of more than 200 units knocks him out)
// The side without a door at a level is a wall there.
//
// Lift pads start at the stop nearest the door he came in by.
//
// spots: places a piece of equipment can stand (x its middle, y the top of the
// platform under it). Each room uses a few of them, picked at random.

#ifndef ROOMS_WIDE_H
#define ROOMS_WIDE_H

#include <stdint.h>

#define ROOM_FLOOR_Y 680
#define LEVEL_1      520
#define LEVEL_2      360
#define LEVEL_TOP    200
#define MAX_PLATS    16
#define MAX_LIFTS    4
#define MAX_STOPS    4
#define MAX_PITS     3
#define MAX_SPOTS    8

struct Plat {
  int16_t x0, x1, y;          // top surface from x0 to x1, at height y
};

struct TLift {
  int16_t x0, x1;             // the shaft
  int16_t stops[MAX_STOPS];   // heights it stops at, top first
  int8_t n;                   // how many stops
};

struct Spot {
  int16_t x, y;
};

struct WideRoom {
  const char *name;
  int8_t platCount;
  Plat plats[MAX_PLATS];
  int8_t liftCount;
  TLift lifts[MAX_LIFTS];
  int8_t pitCount;
  int16_t pits[MAX_PITS][2];
  int8_t spotCount;
  Spot spots[MAX_SPOTS];
  int16_t signX, signY;       // radiation sign on the back wall (0 = none)
};

static const WideRoom wideRooms[] = {
  // 0: two lift pads, a pit in the floor, a gap to somersault at the top, a
  //    long deck in the middle you drop down to
  //
  //   200  ======[A]======         ======[B]======    (top: both edges)
  //   360  ======[A]===               ===[B]======
  //   520          [A]     =========     [B]====
  //   680  =========[A]====  pit  =======[B]======    (floor: both edges)
  { "FOUR DECKS",
    12, { { 0, 560, 680 }, { 680, 1280, 680 },                     // floor, a pit in the middle
          { 420, 860, 520 }, { 1040, 1180, 520 },                  // level 1: the middle deck, a ledge off lift B
          { 0, 240, 360 }, { 320, 480, 360 },                      // level 2, either side of lift A...
          { 800, 960, 360 }, { 1040, 1280, 360 },                  // ...and of lift B
          { 0, 240, 200 }, { 320, 560, 200 },                      // the top, left of the gap...
          { 720, 960, 200 }, { 1040, 1280, 200 } },                // ...and right of it
    2, { { 240, 320, { 200, 360, 680 }, 3 },
         { 960, 1040, { 200, 360, 520, 680 }, 4 } },
    1, { { 560, 680 } },
    8, { { 120, 360 }, { 400, 360 }, { 640, 520 }, { 880, 360 },
         { 440, 200 }, { 840, 200 }, { 160, 680 }, { 820, 680 } },
    640, 280 },
};
#define WIDE_ROOM_COUNT (int)(sizeof(wideRooms) / sizeof(wideRooms[0]))

#endif // ROOMS_WIDE_H
