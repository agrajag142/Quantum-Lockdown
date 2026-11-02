// rooms_wide.h
// Room layouts for the full-screen game. A room is the whole screen: 1280
// units wide (1920 pixels), floor at y = 664 and two more levels above it at
// 504 and 344 - one level is 160 units, as in NUCLEAR MISSION, sized for the
// agent at scale 1.3.
//
// Every room has a door on each side, at its TOP (y = 344) or at its BOTTOM
// (the floor, y = 664): which, is decided for each room when the complex is
// laid out. So a layout must work with any of the four combinations:
//   - the floor reaches both screen edges (x = 0 and x = 1280)
//   - a top level platform reaches both screen edges
//   - both doors' ledges, and every spot, can be reached from both doors
//     (on foot, by lift pad, by somersaulting a gap, or a short drop:
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

#define ROOM_FLOOR_Y 664
#define LEVEL_MID    504
#define LEVEL_TOP    344
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
  // 0: two lift pads, a pit in the floor, a gap to somersault at the top
  //
  //   344  ========[A]=========      ========[B]=========    (top: both edges)
  //   504          [A]=========      ==========[B]
  //   664  ===========[A]=====   pit   ==========[B]=======  (floor: both edges)
  { "TWIN LIFTS",
    8, { { 0, 560, 664 }, { 680, 1280, 664 },                      // floor, a pit in the middle
         { 340, 520, 504 }, { 760, 940, 504 },                     // the middle level, off the lifts
         { 0, 260, 344 }, { 340, 560, 344 },                       // the top, left of the gap...
         { 720, 940, 344 }, { 1020, 1280, 344 } },                 // ...and right of it
    2, { { 260, 340, { 344, 504, 664 }, 3 },
         { 940, 1020, { 344, 504, 664 }, 3 } },
    1, { { 560, 680 } },
    8, { { 430, 504 }, { 850, 504 }, { 460, 344 }, { 830, 344 },
         { 180, 664 }, { 1110, 664 }, { 450, 664 }, { 820, 664 } },
    640, 214 },
};
#define WIDE_ROOM_COUNT (int)(sizeof(wideRooms) / sizeof(wideRooms[0]))

#endif // ROOMS_WIDE_H
