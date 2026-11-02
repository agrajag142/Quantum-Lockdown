// game.cpp - the full-screen game: the lift shafts and the rooms
//
// QUANTUM LOCKDOWN. The complex, Impossible Mission style: 30 rooms in COLS (9) columns, each
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
#include "scream_sound.h"

#include "micromod.h"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================ settings
#define GAME_VERSION      "012"
#define COLS              9        // columns of rooms across...
#define ROWS              6        // ...rows down...
#define ROOM_COUNT        30       // ...and the rooms in them
#define MIN_PER_COL       2        // each column has at least this many rooms...
#define MAX_PER_COL       6        // ...and at most this many
#define SHAFTS            (COLS + 1)
#define FLOORS            (ROWS * 2)
#define START_SHAFT       0        // he starts in this shaft's car (A: the top left corner), at its top stop

#define AGENT_SCALE       1.3f
#define RUN_SPEED         175.0f
#define JUMP_DISTANCE     150.0f
#define GRAVITY           1700.0f
#define FALL_MAX_SPEED    950.0f
#define HEALTH_MAX        50       // the health bar
#define FALL_HURT_2       5        // a drop of two levels costs this much health...
#define FALL_HURT_3       15       // ...three or more (top to bottom), or down a pit, this much (one level: nothing)
#define FIRST_AID_KITS    5        // first aid kits over the whole complex (each in a different room)
#define GAME_SECONDS      (15 * 60)  // the countdown: 15 minutes
#define EDGE_GRACE_PX     8
#define LIFT_SPEED        150.0f   // lift pads in rooms
#define CAR_SPEED         260.0f   // the lift car in the shafts: its speed to start with...
#define CAR_MAX_SPEED     1100.0f  // ...picking up to this over a long run past walled-off floors
#define CAR_ACCEL         900.0f   // (units a second, each second; it slows the same way to stop)
#define RESPAWN_SECONDS   1.6f
#define BLEND_SECONDS     0.12f
#define SEARCH_SECONDS    1.6f     // hold USE this long to search a piece of equipment
#define NOTHING_SHOW      1.2f
#define MAX_DECOR         5        // equipment in a room: 4 or 5...
#define EQUIPMENT_TOTAL   128      // ...128 in the whole complex (so 8 rooms have 5)
#define ITEMS_HIDDEN      36       // pieces of equipment with something in them (the rest: nothing)
#define SEARCH_SCORE      50       // searching a piece of equipment (the first time)
#define FOUND_SCORE       200      // finding something in it
#define ITEM_SHOW         2.5f     // seconds FOUND OBJECT stays up
#define MAX_POPS          6        // score numbers floating up at once
#define HURT_SECONDS      0.8f     // knocked down by a long drop before he gets up
// Room hazards (never in room 1, where he starts)
#define ELECTRIC_ROOMS    6        // 20% of the rooms: an electrified floor (a HIGH VOLTAGE sign on the wall)
#define DARK_ROOMS        3        // 10%: the lights go out and his night vision comes on
#define ELEC_SAFE_MIN     4.0f     // electrified floor: safe for this long...
#define ELEC_SAFE_MAX     7.0f     // ...up to this (random each time)
#define ELEC_WARN_SECONDS 1.6f     // then the lights flicker and the floor crackles...
#define ELEC_LIVE_SECONDS 2.6f     // ...then it is live this long: be off the floor (a ledge, a raised lift pad, in the air)
#define ELEC_AFTER_SHOCK  6.0f     // a shock trips the power: safe this long after
#define SHOCK_SECONDS     0.9f     // jolting before he drops
#define ELEC_HURT         10       // health a shock costs (and he wakes at the door he came in by)
#define CAMERA_ROOMS      6        // 20% of the rooms: a security camera in a top corner...
#define DUAL_CAMERA_ROOMS 3        // ...10%: one in each top corner
#define CAM_PENALTY       30       // seconds off the clock when a camera sees him moving
#define CAM_PERIOD        6.0f     // seconds for a sweep across and back
#define CAM_MID_DEG       38.0f    // the sweep: its middle (degrees from straight down, out into the room)...
#define CAM_SWING_DEG     34.0f    // ...and how far each way
#define CAM_HALF_DEG      7.0f     // half the width of the beam (it goes through everything)
#define CAM_REACT         0.12f    // seconds of moving in the beam before it notices
#define CAM_ENTRY_GRACE   0.8f     // seconds after coming in before it can notice
#define CAUGHT_SECONDS    1.6f     // hands up, then back at the door he came in by
#define DRONE_ROOMS       4        // rooms with a drone hunting him
#define DRONE_SPEED       115.0f   // its top speed (he runs at about 230)
#define DRONE_WARN        440.0f   // closer than this: its light goes yellow...
#define DRONE_RANGE       270.0f   // ...closer than this, red: it fires
#define DRONE_FIRE_EVERY  0.8f     // seconds between shots (the first a little after it goes red)
#define BULLET_SPEED      650.0f
#define BULLET_HURT       5        // health a bullet costs
#define DRONE_GRACE       1.5f     // seconds it waits after he comes in (or back in)
#define NVG_RANGE         400      // night vision: how far the goggles light up ahead...
#define NVG_HALF_DEG      28.0f    // ...half the width of their view...
#define NVG_AROUND        56       // ...and the circle round him
#define MAX_ORNAMENTS     14       // things on a room's walls that are only to look at (lights, fans...)
#ifndef BIG_FAN_MIN
#define BIG_FAN_MIN       88       // a big fan goes where there is a space this big (its radius): rooms with big gaps
#endif
#define ELEVATOR_MUSIC_VOLUME 0.40f  // elevator.mod, while he stands in the lift car: 0.0 = off ... 1.0 = full
#define STAGE1_MUSIC_VOLUME   0.25f  // stage1.mod, the rest of the time in stage 1
#define MUSIC_FADE_SECONDS    0.8f   // fading in and out (the two cross over)
#define SFX_VOLUME            1.00f  // the sound effects (steps, the lift)
#define LIFT_HUM_VOLUME       0.30f  // the lift's motor while it moves (room lift pads a little quieter)
#define SCREAM_VOLUME         0.55f  // his scream, falling down a pit (scream_sound.h)
#define AUDIO_RATE        48000      // the PS5's output rate

#define HUD_H    40
#define PLAY_Y1  (SCREEN_H - 1)
#define PLAT_T   16
#define PAD_H    12
#define DOOR_H   140               // a room's doorway (he is 130 tall)
#define CEIL_Y   56                // a room's ceiling (under it: the top level's headroom)

// The shaft view
#define SHAFT_X0     580
#define SHAFT_X1     700
#define CAR_FLOOR_Y  400           // the car's floor on screen (the view follows the car)
#define PANEL_Y      500           // the lift area: the shaft above this, the panel (mini map, details) below
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
  uint8_t decorN;                  // its equipment: how many pieces (4 or 5)
  bool searched[MAX_DECOR];        // ...searched yet
  bool holds[MAX_DECOR];           // ...something in it
  bool kit, kitUsed;               // a first aid kit here, and whether it has been used
  float progress[MAX_DECOR + 1];   // how far each piece has been searched (seconds; the kit last)
  uint8_t hazard;                  // Hazard
};
enum Hazard : uint8_t { HZ_NONE, HZ_ELECTRIC, HZ_DARK, HZ_CAMERA, HZ_CAMERA2, HZ_DRONE };
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
  used[0][0] = true;                   // (always a room in the top left corner: he starts beside it, in lift A)
  for (int c = 0; c < COLS; c++)
    for (int k = (c == 0 ? 1 : 0); k < count[c];) {
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
      ri.decorN = (uint8_t)(EQUIPMENT_TOTAL / ROOM_COUNT);
      for (int i = 0; i < MAX_DECOR; i++) ri.searched[i] = ri.holds[i] = false;
      ri.kit = ri.kitUsed = false;
      for (int i = 0; i <= MAX_DECOR; i++) ri.progress[i] = 0;
      ri.hazard = HZ_NONE;
    }
  // the rooms with one more piece of equipment, to make EQUIPMENT_TOTAL
  for (int extra = EQUIPMENT_TOTAL - ROOM_COUNT * (EQUIPMENT_TOTAL / ROOM_COUNT); extra > 0;) {
    RoomInfo &ri = rooms[rnd(ROOM_COUNT)];
    if (ri.decorN >= MAX_DECOR || ri.decorN >= wideRooms[ri.tpl].spotCount) continue;
    ri.decorN++;
    extra--;
  }
  // and the pieces with something in them, ITEMS_HIDDEN of them anywhere
  for (int k = 0; k < ITEMS_HIDDEN;) {
    RoomInfo &ri = rooms[rnd(ROOM_COUNT)];
    int i = rnd(ri.decorN);
    if (ri.holds[i]) continue;
    ri.holds[i] = true;
    k++;
  }
  // the first aid kits, in rooms with a spare spot for one
  for (int k = 0; k < FIRST_AID_KITS;) {
    RoomInfo &ri = rooms[rnd(ROOM_COUNT)];
    if (ri.kit || ri.decorN >= wideRooms[ri.tpl].spotCount) continue;
    ri.kit = true;
    k++;
  }
  // the hazards, one at most in a room (none in room 1, where he starts)
  const int hzCount[5] = { ELECTRIC_ROOMS, DARK_ROOMS, CAMERA_ROOMS, DUAL_CAMERA_ROOMS, DRONE_ROOMS };
  for (int h = 0; h < 5; h++)
    for (int k = 0; k < hzCount[h];) {
      int i = 1 + rnd(ROOM_COUNT - 1);
      if (rooms[i].hazard != HZ_NONE) continue;
      rooms[i].hazard = (uint8_t)(HZ_ELECTRIC + h);
      k++;
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
enum DecorKind : uint8_t {
  DEC_CABINET, DEC_SAFE, DEC_PLANT, DEC_RADAR, DEC_PANEL,       // from NUCLEAR MISSION's silo rooms
  DEC_DESK, DEC_COOLER, DEC_SHELF, DEC_COAT,                    // ...and its offices
  DEC_COMPUTER, DEC_CUPBOARD, DEC_SERVER,                       // new: a desk with a computer, a cupboard, a computer rack
  DEC_KINDS, DEC_FIRSTAID = DEC_KINDS };
static const int16_t decorW[DEC_KINDS + 1] = { 52, 62, 44, 84, 96, 104, 34, 70, 56, 104, 72, 58, 48 };
static const int16_t decorH[DEC_KINDS + 1] = { 96, 68, 96, 92, 82, 98, 96, 110, 132, 108, 118, 120, 100 };
struct Decor {
  uint8_t kind;
  int16_t x, y;                    // middle, and the top of what it stands on
  uint16_t seed;                   // its blinking lights
};
static Decor decor[MAX_DECOR + 1];       // (the equipment, then a first aid kit if the room has one)
static int decorCount = 0;

// On the walls, only to look at: red warning lights, fans (a big one where
// there is a big space), coolant tubes, gauges, notices; and the HIGH VOLTAGE
// sign in a room with an electrified floor
enum OrnKind : uint8_t { ORN_REDLIGHT, ORN_FAN, ORN_BIGFAN, ORN_TUBE, ORN_GAUGE, ORN_NOTICE, ORN_VOLTAGE };
struct Ornament {
  uint8_t kind;
  int16_t x, y, r;                 // middle; r: a fan's size, a tube's half height
  uint16_t seed;
};
static Ornament orn[MAX_ORNAMENTS];
static int ornCount = 0;

// The room's hazard, as it is now
static uint8_t curHazard = HZ_NONE;
static uint8_t elecState = 0;          // electrified floor: 0 safe, 1 the warning, 2 live
static float elecT = 0, elecSafeFor = 5, elecSoundT = 0;
static float darkT = 0;                // dark room: seconds since coming in
static bool lightsOut = false;         // ...the lights have gone: night vision
// Security cameras (1 or 2), hung from the ceiling in the top corners
struct Camera {
  float x, y;                          // the pivot
  float t, angle;                      // the sweep's clock; where it points (degrees from straight down, + to the right)
  float mid;                           // the middle of its sweep (out into the room)
  float movingT;                       // how long he has been moving in its beam
  uint8_t view;                        // 0 not on him, 1 on him (but he is still), 2 caught him
};
static Camera cams[2];
static int camCount = 0;
static float camGrace = 0;
static bool camCaughtPending = false;  // seen in the air: hands up when he lands
// The drone
static float drX, drY, drVX, drVY, drGrace, drFireT, drFlashT;
static uint8_t drLight = 0;            // 0 green (out of range), 1 yellow (getting close), 2 red (firing)
#define MAX_BULLETS 8
struct Bullet { float x, y, vx, vy, life; };
static Bullet bullets[MAX_BULLETS];
static uint32_t fxState = 99991;       // (sparks and such: not the layout's numbers)
static int fxRnd(int lo, int hi) {     // lo..hi-1
  fxState = fxState * 1664525u + 1013904223u;
  return hi > lo ? lo + (int)((fxState >> 8) % (uint32_t)(hi - lo)) : lo;
}

// Pictures painted once: the room, and the shaft (all but what scrolls)
static uint32_t roomBg[FB_W * FB_H];
static uint32_t shaftBg[FB_W * FB_H];

// ============================================================ the agent
enum AgentState : uint8_t { ST_IDLE, ST_RUN, ST_JUMP, ST_USE, ST_FALL, ST_LAND, ST_LIFT, ST_KO, ST_SHOCK, ST_CAUGHT };
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
static float searchBeepT = 0;
#define SEARCH_BEEP_SECONDS 0.18f
static float nothingShowT = 0, nothingX = 0, nothingY = 0;

// The opening: out of the air-conditioning duct onto the catwalk (as NUCLEAR MISSION's lab)
static float introT = -1;              // >= 0: the opening is playing (seconds)
static int ventShaft = -1, ventFloor = -1, ventX = 0;   // where the duct is
static bool ventOpen = false;          // its grille has come out (it lies on the catwalk)

// Health, the clock, the end
static int health = HEALTH_MAX;
static float hurtFlashT = 0;           // the health bar flashes after a hurt
static float timeLeft = GAME_SECONDS;
static bool koGetUp = false;           // knocked down by a drop: he gets up where he is (else back to the door)
enum EndReason : uint8_t { END_NONE, END_TIME, END_HEALTH };
static EndReason gameOver = END_NONE;
static float gameOverT = 0;
static char floatText[24] = "";        // a word floating up from the equipment (NOTHING, HEALTH RESTORED...)
static uint16_t floatColour = 0;

// The score
static long score = 0;
static int itemsFound = 0;
static float itemShowT = 0;            // FOUND OBJECT on screen
static float popX[MAX_POPS], popY[MAX_POPS], popT[MAX_POPS];
static long popV[MAX_POPS];

static void addScore(long v, float x, float y) {
  score += v;
  int k = 0;
  for (int i = 1; i < MAX_POPS; i++)
    if (popT[i] < popT[k]) k = i;      // (the oldest one)
  popX[k] = x;
  popY[k] = y;
  popT[k] = 1.2f;
  popV[k] = v;
}

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

// ============================================================ sound
// The platform calls gameAudio from its sound thread for 48 kHz stereo. The
// game only says what it wants: which tune (tuneWanted), whether a lift is
// moving (liftHum), and sound effects through a little queue. The sound thread
// does the rest: the effects are NUCLEAR MISSION's little synthesiser (square,
// triangle and noise voices that slide in pitch); the music is two MOD players,
// one for each tune, so each keeps its place while the other plays.
extern "C" {
long micromod2_calculate_mod_file_len(signed char *module_header);
long micromod2_initialise(signed char *data, long sampling_rate);
void micromod2_set_gain(long value);
void micromod2_get_audio(short *output_buffer, long count);
}

enum Sfx : uint8_t { SFX_NONE, SFX_STEP, SFX_LIFT_STOP, SFX_BEEP, SFX_CLUNK, SFX_FOUND, SFX_FANFARE, SFX_LAND,
                     SFX_BUZZ, SFX_ZAP, SFX_POWERDOWN, SFX_SCREAM, SFX_ALARM, SFX_SHOT, SFX_HIT };
enum Wave : uint8_t { WAVE_SQUARE, WAVE_TRIANGLE, WAVE_NOISE };
struct Voice {
  bool active;
  Wave wave;
  float f0, f1, level, phase, noiseValue;
  uint32_t delay, length, pos;
};
#define MAX_VOICES 12
static Voice voices[MAX_VOICES];
static uint32_t noiseState = 22222;

static std::atomic<int> tuneWanted{0};    // 0 none, 1 the lift's (elevator.mod), 2 stage 1's (stage1.mod)
static std::atomic<int> liftHum{0};       // 0 still, 1 the car going up, 2 down, 3 / 4 a room's lift pad
static std::atomic<int> droneHum{0};      // a drone in the room: its rotors' whine (0 none)
static bool tuneReady[2] = { false, false };
static char tuneProblem[2][48] = { "NO ELEVATOR.MOD IN THE ASSETS", "NO STAGE1.MOD IN THE ASSETS" };
static float tuneGain[2] = { 0, 0 };       // (the sound thread's)
static short tuneBuf[2048 * 2];
static uint8_t sfxRing[64];
static std::atomic<int> sfxHead{0}, sfxTail{0};

static void sfxPlay(uint8_t sfx) {           // (the game's side of the queue)
  int h = sfxHead.load(std::memory_order_relaxed), nx = (h + 1) & 63;
  if (nx == sfxTail.load(std::memory_order_acquire)) return;
  sfxRing[h] = sfx;
  sfxHead.store(nx, std::memory_order_release);
}

static void startVoice(Wave wave, float f0, float f1, int delayMs, int lengthMs, float level) {
  for (int i = 0; i < MAX_VOICES; i++) {
    Voice &v = voices[i];
    if (v.active) continue;
    v.active = true;
    v.wave = wave;
    v.f0 = f0;
    v.f1 = f1;
    v.delay = (uint32_t)delayMs * AUDIO_RATE / 1000;
    v.length = (uint32_t)lengthMs * AUDIO_RATE / 1000;
    v.pos = 0;
    v.level = level;
    v.phase = 0;
    v.noiseValue = 0;
    return;
  }
}

static float screamPos = -1;           // (the sound thread's) where the scream is playing, in its samples; < 0 not playing

static void startEffect(uint8_t sfx) {
  switch (sfx) {
    case SFX_ALARM:                            // caught on camera: NUCLEAR MISSION's alarm, twice over
      for (int k = 0; k < 3; k++) {
        startVoice(WAVE_SQUARE, 880, 660, k * 260, 220, 0.14f);
        startVoice(WAVE_SQUARE, 440, 330, k * 260, 220, 0.06f);
      }
      break;
    case SFX_SHOT:                             // the drone's gun: a sharp crack
      startVoice(WAVE_NOISE, 7000, 1500, 0, 90, 0.26f);
      startVoice(WAVE_SQUARE, 900, 200, 0, 60, 0.10f);
      break;
    case SFX_HIT:                              // a bullet hits him
      startVoice(WAVE_NOISE, 1200, 200, 0, 120, 0.32f);
      startVoice(WAVE_SQUARE, 140, 60, 0, 140, 0.20f);
      break;
    case SFX_SCREAM:                           // a recording, not the synthesiser (from the start each time)
      screamPos = 0;
      break;
    case SFX_STEP:                             // NUCLEAR MISSION's footstep
      startVoice(WAVE_NOISE, 1800, 500, 0, 35, 0.22f);
      startVoice(WAVE_SQUARE, 150, 90, 0, 30, 0.10f);
      break;
    case SFX_BEEP: {                           // NUCLEAR MISSION's searching beep: a random note
      static const float notes[4] = { 880, 1175, 1319, 1568 };
      noiseState = noiseState * 1664525u + 1013904223u;
      float f = notes[(noiseState >> 20) & 3];
      startVoice(WAVE_SQUARE, f, f, 0, 40, 0.10f);
      break;
    }
    case SFX_CLUNK:                            // ...nothing in it
      startVoice(WAVE_NOISE, 900, 150, 0, 90, 0.30f);
      startVoice(WAVE_SQUARE, 90, 60, 0, 90, 0.18f);
      break;
    case SFX_FOUND:                            // ...something found (its "found" chirp)
      startVoice(WAVE_SQUARE, 600, 1200, 0, 120, 0.14f);
      startVoice(WAVE_SQUARE, 900, 1800, 130, 160, 0.12f);
      break;
    case SFX_FANFARE:                          // ...and its rising four notes
      startVoice(WAVE_TRIANGLE, 523, 523, 0, 120, 0.4f);
      startVoice(WAVE_TRIANGLE, 659, 659, 110, 120, 0.4f);
      startVoice(WAVE_TRIANGLE, 784, 784, 220, 120, 0.4f);
      startVoice(WAVE_TRIANGLE, 1047, 1047, 330, 400, 0.4f);
      break;
    case SFX_LAND:                             // NUCLEAR MISSION's landing
      startVoice(WAVE_NOISE, 1500, 200, 0, 130, 0.30f);
      startVoice(WAVE_SQUARE, 110, 50, 0, 120, 0.18f);
      break;
    case SFX_BUZZ:                             // NUCLEAR MISSION's electrified floor: the warning buzz...
      startVoice(WAVE_SQUARE, 100, 100, 0, 400, 0.10f);
      startVoice(WAVE_SQUARE, 150, 150, 0, 400, 0.06f);
      startVoice(WAVE_NOISE, 4000, 4000, 0, 120, 0.06f);
      break;
    case SFX_ZAP:                              // ...the crackle while it is live
      startVoice(WAVE_NOISE, 6000, 2500, 0, 140, 0.22f);
      startVoice(WAVE_SQUARE, 60, 120, 0, 140, 0.12f);
      break;
    case SFX_POWERDOWN:                        // ...and its blackout: the power dying
      startVoice(WAVE_TRIANGLE, 700, 60, 0, 1000, 0.30f);
      startVoice(WAVE_SQUARE, 350, 30, 0, 1000, 0.08f);
      break;
    case SFX_LIFT_STOP:                        // the lift settling at a stop: a soft low thump
      startVoice(WAVE_TRIANGLE, 90, 45, 0, 140, 0.30f);
      startVoice(WAVE_NOISE, 500, 120, 0, 90, 0.10f);
      break;
  }
}

static inline float voiceSample(Voice &v) {
  float p = (float)v.pos / (float)v.length;
  float freq = v.f0 + (v.f1 - v.f0) * p;
  v.phase += freq / AUDIO_RATE;
  bool wrapped = false;
  if (v.phase >= 1.0f) {
    v.phase -= 1.0f;
    wrapped = true;
  }
  float out;
  switch (v.wave) {
    case WAVE_SQUARE: out = (v.phase < 0.5f) ? 1.0f : -1.0f; break;
    case WAVE_TRIANGLE: out = 4.0f * fabsf(v.phase - 0.5f) - 1.0f; break;
    default:
      if (wrapped || v.pos == 0) {
        noiseState = noiseState * 1664525u + 1013904223u;
        v.noiseValue = (float)(noiseState >> 16) / 32768.0f - 1.0f;
      }
      out = v.noiseValue;
      break;
  }
  float attack = (float)v.pos / (0.003f * AUDIO_RATE);
  float env = (attack < 1.0f) ? attack : (1.0f - p);
  return out * env * v.level;
}

// The lift's motor, in the manner of Impossible Mission's: a low buzzing hum
// that throbs, its pitch rising while the lift goes up and falling going down
static float humPhase = 0, humPhase2 = 0, humWob = 0, humPitch = 0, humLevel = 0;
static inline float liftHumSample(int hum) {
  float target = hum ? LIFT_HUM_VOLUME * (hum >= 3 ? 0.7f : 1.0f) : 0.0f;
  humLevel += (target - humLevel) * 0.0015f;                       // (a short swell in and out)
  if (humLevel < 0.0005f && !hum) return 0;
  bool up = (hum == 1 || hum == 3);
  float bend = up ? 1.0f : -1.0f;
  if (hum) humPitch += (bend - humPitch) * 0.00004f;               // the pitch drifts the way it's going
  float f = 62.0f + 14.0f * humPitch;
  humPhase += f / AUDIO_RATE;
  if (humPhase >= 1) humPhase -= 1;
  humPhase2 += f * 2.01f / AUDIO_RATE;
  if (humPhase2 >= 1) humPhase2 -= 1;
  humWob += 11.0f / AUDIO_RATE;                                    // the throb
  if (humWob >= 1) humWob -= 1;
  float pulse = humPhase < 0.3f ? 1.0f : -0.43f;                   // a narrow pulse: buzzy
  float tri = 4.0f * fabsf(humPhase2 - 0.5f) - 1.0f;
  float throb = 0.65f + 0.35f * sinf(humWob * TWO_PI);
  return (pulse * 0.6f + tri * 0.4f) * throb * humLevel;
}

// A drone's rotors: a thin whine with a fast flutter, quiet
static float drPhase = 0, drPhase2 = 0, drWob = 0, drLevel = 0;
static inline float droneSample(int on) {
  float target = on ? 0.07f : 0.0f;
  drLevel += (target - drLevel) * 0.001f;
  if (drLevel < 0.0005f && !on) return 0;
  drPhase += 230.0f / AUDIO_RATE;
  if (drPhase >= 1) drPhase -= 1;
  drPhase2 += 347.0f / AUDIO_RATE;
  if (drPhase2 >= 1) drPhase2 -= 1;
  drWob += 37.0f / AUDIO_RATE;
  if (drWob >= 1) drWob -= 1;
  float a = 4.0f * fabsf(drPhase - 0.5f) - 1.0f, b = drPhase2 < 0.5f ? 0.5f : -0.5f;
  return (a + b * 0.5f) * (0.7f + 0.3f * sinf(drWob * TWO_PI)) * drLevel;
}

void gameSetTune(int which, signed char *data, long len) {
  if (which < 0 || which > 1 || !data || len < 1084) return;
  const char *name = which ? "STAGE1.MOD" : "ELEVATOR.MOD";
  long need = which ? micromod2_calculate_mod_file_len(data) : micromod_calculate_mod_file_len(data);
  if (need < 0) { snprintf(tuneProblem[which], 48, "%s: NOT A MOD FILE", name); return; }
  if (need > len) { snprintf(tuneProblem[which], 48, "%s: FILE CUT SHORT", name); return; }
  long ok = which ? micromod2_initialise(data, AUDIO_RATE) : micromod_initialise(data, AUDIO_RATE);
  if (ok != 0) { snprintf(tuneProblem[which], 48, "%s: WON'T PLAY", name); return; }
  if (which) micromod2_set_gain(64);
  else micromod_set_gain(64);
  tuneProblem[which][0] = 0;
  tuneReady[which] = true;
}

void gameAudio(short *out, int frames) {
  if (frames > 2048) frames = 2048;
  // new effects from the game
  int t = sfxTail.load(std::memory_order_relaxed);
  while (t != sfxHead.load(std::memory_order_acquire)) {
    startEffect(sfxRing[t]);
    t = (t + 1) & 63;
    sfxTail.store(t, std::memory_order_release);
  }
  // the effects and the lift's hum (mono)
  int hum = liftHum.load(std::memory_order_relaxed);
  int drone = droneHum.load(std::memory_order_relaxed);
  static float fx[2048];
  for (int n = 0; n < frames; n++) {
    float mix = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;
      if (v.delay > 0) {
        v.delay--;
        continue;
      }
      mix += voiceSample(v);
      if (++v.pos >= v.length) v.active = false;
    }
    if (screamPos >= 0) {                    // the scream: 24 kHz, stepped through at 48 kHz (in between: blended)
      int i = (int)screamPos;
      if (i + 1 >= SCREAM_LEN) {
        screamPos = -1;
      } else {
        float k = screamPos - i;
        mix += (screamSamples[i] + (screamSamples[i + 1] - screamSamples[i]) * k) * (SCREAM_VOLUME / 32768.0f * 3.6f);
        screamPos += (float)SCREAM_RATE / AUDIO_RATE;
      }
    }
    fx[n] = mix * SFX_VOLUME + liftHumSample(hum) + droneSample(drone);
  }
  // the two tunes, each fading towards its level (a tune at nothing waits where it was)
  static int32_t acc[2048 * 2];
  memset(acc, 0, (size_t)frames * 2 * sizeof(int32_t));
  int want = tuneWanted.load(std::memory_order_relaxed);
  for (int k = 0; k < 2; k++) {
    float vol = k ? STAGE1_MUSIC_VOLUME : ELEVATOR_MUSIC_VOLUME;
    float target = (tuneReady[k] && want == k + 1) ? vol : 0.0f;
    if (!tuneReady[k] || (tuneGain[k] <= 0 && target <= 0)) continue;
    memset(tuneBuf, 0, (size_t)frames * 4);
    if (k) micromod2_get_audio(tuneBuf, frames);
    else micromod_get_audio(tuneBuf, frames);
    float step = vol / (MUSIC_FADE_SECONDS * AUDIO_RATE);
    for (int n = 0; n < frames; n++) {
      if (tuneGain[k] < target) tuneGain[k] = fminf(tuneGain[k] + step, target);
      else if (tuneGain[k] > target) tuneGain[k] = fmaxf(tuneGain[k] - step, target);
      // the MOD's channels are panned hard left and right: each side gets a little of the other
      float l = tuneBuf[n * 2], r = tuneBuf[n * 2 + 1];
      acc[n * 2] += (int32_t)((l * 0.62f + r * 0.25f) * tuneGain[k]);
      acc[n * 2 + 1] += (int32_t)((r * 0.62f + l * 0.25f) * tuneGain[k]);
    }
  }
  for (int n = 0; n < frames; n++) {
    int32_t e = (int32_t)(fx[n] * 9000.0f);
    int32_t l = acc[n * 2] + e, r = acc[n * 2 + 1] + e;
    out[n * 2] = (short)(l > 32767 ? 32767 : (l < -32768 ? -32768 : l));
    out[n * 2 + 1] = (short)(r > 32767 ? 32767 : (r < -32768 ? -32768 : r));
  }
}

// ============================================================ painting: rooms (into roomBg, once per room)
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

// Where a door's EXIT lamp is (its top left corner): over the doorway, or
// beside it where the ceiling is too close
static void exitLampAt(bool right, int doorY, int &lx, int &ly) {
  int top = doorY - DOOR_H;
  bool above = top - 36 > CEIL_Y + 20;
  lx = above ? (right ? SCREEN_W - 36 : 6) : (right ? SCREEN_W - 112 : 82);
  ly = above ? top - 34 : top + 6;
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
  int lx, ly;
  exitLampAt(right, doorY, lx, ly);
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

// ---- from NUCLEAR MISSION's offices
static void paintDesk(int x, int fy) {
  uint16_t wood = rgb565(120, 86, 54), dark = rgb565(84, 58, 36), top = rgb565(150, 112, 74);
  fillRect(x - 52, fy - 70, x + 52, fy - 62, top);
  fillRect(x - 50, fy - 62, x - 14, fy - 1, wood);                 // drawers
  for (int k = 0; k < 3; k++) {
    fillRect(x - 46, fy - 58 + k * 19, x - 18, fy - 43 + k * 19, rgb565(132, 96, 62));
    fillRect(x - 36, fy - 52 + k * 19, x - 28, fy - 50 + k * 19, rgb565(200, 190, 150));
  }
  fillRect(x + 42, fy - 62, x + 50, fy - 1, dark);                 // leg
  fillRect(x - 14, fy - 62, x + 42, fy - 56, dark);
  fillRect(x + 4, fy - 76, x + 34, fy - 71, rgb565(236, 234, 224)); // papers
  fillRect(x + 8, fy - 79, x + 36, fy - 76, rgb565(220, 218, 206));
  drawLine(x - 30, fy - 71, x - 24, fy - 98, rgb565(60, 60, 64), true);   // a desk lamp
  drawLine(x - 24, fy - 98, x - 8, fy - 92, rgb565(60, 60, 64), true);
  fillCircle(x - 6, fy - 90, 6, rgb565(70, 110, 70));
  fillRect(x - 36, fy - 72, x - 22, fy - 70, rgb565(60, 60, 64));
}

static void paintCooler(int x, int fy) {
  fillRect(x - 15, fy - 60, x + 15, fy - 1, rgb565(210, 210, 206));
  fillRect(x - 15, fy - 60, x + 15, fy - 57, rgb565(236, 236, 232));
  fillRect(x + 11, fy - 56, x + 15, fy - 1, rgb565(170, 170, 168));
  fillRect(x - 6, fy - 40, x + 2, fy - 34, rgb565(60, 120, 200));   // taps
  fillRect(x - 6, fy - 30, x + 2, fy - 26, rgb565(200, 60, 50));
  int xs[6] = { x - 12, x + 12, x + 14, x + 8, x - 8, x - 14 };
  int ys[6] = { fy - 96, fy - 96, fy - 70, fy - 60, fy - 60, fy - 70 };
  fillPolygon(xs, ys, 6, rgb565(110, 170, 220));                   // the bottle
  fillRect(x - 6, fy - 92, x - 2, fy - 66, rgb565(170, 210, 240));
}

static void paintShelf(int x, int fy) {
  uint16_t wood = rgb565(110, 80, 52), dark = rgb565(70, 50, 32);
  fillRect(x - 35, fy - 110, x + 35, fy - 1, wood);
  fillRect(x - 31, fy - 106, x + 31, fy - 4, dark);
  static const uint8_t bookRGB[6][3] = { { 160, 40, 36 }, { 40, 80, 150 }, { 200, 170, 80 }, { 50, 110, 60 }, { 180, 180, 170 }, { 120, 60, 120 } };
  uint32_t h = (uint32_t)x * 2654435761u;
  for (int sh = 0; sh < 3; sh++) {
    int by = fy - 106 + sh * 35 + 34;
    fillRect(x - 31, by - 2, x + 31, by, wood);
    int bx = x - 30;
    while (bx < x + 26) {
      h = h * 1664525u + 1013904223u;
      int w = 5 + (h >> 28), ht = 20 + ((h >> 20) & 7);
      if (bx + w > x + 30) break;
      const uint8_t *c = bookRGB[(h >> 12) % 6];
      fillRect(bx, by - 2 - ht, bx + w - 1, by - 3, rgb565(c[0], c[1], c[2]));
      bx += w + 1;
    }
  }
}

// A coat stand with a jacket and a cap hanging on it
static void paintCoatStand(int x, int fy) {
  uint16_t wood = rgb565(96, 66, 40), dark = rgb565(66, 44, 26);
  fillRect(x - 3, fy - 122, x + 3, fy - 6, wood);
  fillRect(x - 1, fy - 122, x, fy - 6, rgb565(130, 92, 58));
  int bx[4] = { x - 26, x + 26, x + 20, x - 20 }, by[4] = { fy - 1, fy - 1, fy - 8, fy - 8 };
  fillPolygon(bx, by, 4, dark);
  fillCircle(x, fy - 124, 4, dark);
  drawLine(x, fy - 116, x - 14, fy - 124, wood, true);
  drawLine(x, fy - 116, x + 14, fy - 124, wood, true);
  uint16_t jk = rgb565(70, 82, 56), jkd = rgb565(50, 60, 40);
  int jx[6] = { x - 26, x - 6, x - 2, x - 4, x - 30, x - 32 };
  int jy[6] = { fy - 116, fy - 120, fy - 100, fy - 52, fy - 50, fy - 100 };
  fillPolygon(jx, jy, 6, jk);
  int cx[3] = { x - 22, x - 8, x - 15 }, cy[3] = { fy - 118, fy - 120, fy - 100 };
  fillPolygon(cx, cy, 3, jkd);
  drawLine(x - 15, fy - 100, x - 16, fy - 54, jkd, false);
  for (int k = 0; k < 3; k++) fillCircle(x - 13, fy - 92 + k * 14, 1, rgb565(190, 170, 110));
  fillRect(x - 28, fy - 84, x - 20, fy - 80, jkd);
  int sx[4] = { x - 32, x - 26, x - 26, x - 34 }, sy[4] = { fy - 100, fy - 100, fy - 58, fy - 60 };
  fillPolygon(sx, sy, 4, jkd);
  fillRect(x + 6, fy - 132, x + 24, fy - 124, rgb565(60, 64, 50));
  fillRect(x + 18, fy - 125, x + 30, fy - 122, rgb565(40, 42, 34));
}

// ---- new
// A steel desk with a computer on it: a monitor (its screen moves: drawDecorAnim),
// a keyboard, and the computer box underneath
static void paintComputerDesk(int x, int fy) {
  uint16_t dark = rgb565(56, 60, 70), top = rgb565(140, 146, 156);
  fillRect(x - 52, fy - 62, x + 52, fy - 56, top);                 // the desk top
  fillRect(x - 50, fy - 56, x - 44, fy - 1, dark);                 // legs
  fillRect(x + 44, fy - 56, x + 50, fy - 1, dark);
  fillRect(x + 10, fy - 50, x + 38, fy - 4, rgb565(196, 192, 176)); // the computer box, under it
  fillRect(x + 14, fy - 44, x + 34, fy - 40, rgb565(60, 60, 64));
  fillCircle(x + 30, fy - 12, 2, rgb565(80, 230, 120));
  // the monitor: a beige case round a dark screen
  fillRect(x - 34, fy - 108, x + 18, fy - 68, rgb565(196, 192, 176));
  fillRect(x - 30, fy - 104, x + 14, fy - 74, rgb565(8, 24, 12));
  fillRect(x - 14, fy - 68, x - 2, fy - 63, rgb565(160, 156, 142));   // its stand
  fillRect(x - 40, fy - 66, x + 20, fy - 63, rgb565(220, 218, 206)); // the keyboard
  for (int k = 0; k < 9; k++) fillRect(x - 37 + k * 6, fy - 65, x - 34 + k * 6, fy - 64, rgb565(150, 150, 146));
}

// A tall steel cupboard with two doors
static void paintCupboard(int x, int fy) {
  int x0 = x - 36, x1 = x + 36, top = fy - 118;
  uint16_t body = rgb565(118, 124, 112), edge = rgb565(70, 74, 66), face = rgb565(132, 138, 126);
  fillRect(x0 + 3, fy - 5, x0 + 9, fy - 1, edge);
  fillRect(x1 - 9, fy - 5, x1 - 3, fy - 1, edge);
  fillRect(x0, top, x1, fy - 6, body);
  fillRect(x0, top, x1, top + 3, rgb565(160, 166, 154));
  fillRect(x0 + 4, top + 7, x - 2, fy - 11, face);                 // the doors
  fillRect(x + 2, top + 7, x1 - 4, fy - 11, face);
  fillRect(x - 1, top + 7, x + 1, fy - 11, edge);
  for (int k = 0; k < 4; k++) fillRect(x0 + 10, top + 14 + k * 5, x - 8, top + 15 + k * 5, edge);   // vents
  for (int k = 0; k < 4; k++) fillRect(x + 8, top + 14 + k * 5, x1 - 10, top + 15 + k * 5, edge);
  fillRect(x - 8, top + 52, x - 5, top + 70, rgb565(60, 62, 58));  // handles
  fillRect(x + 5, top + 52, x + 8, top + 70, rgb565(60, 62, 58));
  fillRect(x + 12, top + 30, x + 26, top + 40, rgb565(220, 214, 180));   // a label
}

// A computer rack: tape reels, rows of lamps (they blink: drawDecorAnim)
static void paintServer(int x, int fy) {
  int x0 = x - 29, x1 = x + 29, top = fy - 120;
  fillRect(x0, top, x1, fy - 1, rgb565(38, 42, 50));
  fillRect(x0, top, x1, top + 3, rgb565(90, 96, 108));
  fillRect(x1 - 4, top + 4, x1, fy - 1, rgb565(26, 28, 34));
  for (int k = 0; k < 2; k++) {                                    // two tape reels
    int cx = x - 12 + k * 24, cy = top + 24;
    fillCircle(cx, cy, 10, rgb565(150, 154, 164));
    fillCircle(cx, cy, 7, rgb565(60, 52, 44));
    fillCircle(cx, cy, 2, rgb565(150, 154, 164));
  }
  for (int r = 0; r < 4; r++) fillRect(x0 + 6, top + 44 + r * 14, x1 - 8, top + 52 + r * 14, rgb565(22, 24, 30));   // lamp rows
  fillRect(x0 + 6, fy - 22, x1 - 8, fy - 8, rgb565(54, 58, 68));   // a vent
  for (int k = 0; k < 6; k++) fillRect(x0 + 9 + k * 7, fy - 20, x0 + 12 + k * 7, fy - 10, rgb565(26, 28, 34));
}

// A first aid kit on the wall: a white case with a red cross (open and empty once used)
static void paintFirstAid(int x, int fy, bool used) {
  int x0 = x - 24, x1 = x + 24, y0 = fy - 100, y1 = fy - 56;
  fillRect(x - 3, y1 + 1, x + 3, fy - 40, rgb565(70, 76, 88));          // the bracket
  fillRect(x - 12, fy - 42, x + 12, fy - 38, rgb565(70, 76, 88));
  fillRect(x0, y0, x1, y1, rgb565(30, 32, 36));
  if (used) {
    fillRect(x0 + 3, y0 + 3, x1 - 3, y1 - 3, rgb565(60, 64, 70));        // open, empty
    int dx[4] = { x0, x0 - 18, x0 - 18, x0 }, dy[4] = { y0, y0 + 6, y1 + 6, y1 };
    fillPolygon(dx, dy, 4, rgb565(200, 200, 204));                       // the door, swung open
    fillRect(x0 - 14, y0 + 18, x0 - 6, y0 + 26, rgb565(170, 40, 34));
  } else {
    fillRect(x0 + 2, y0 + 2, x1 - 2, y1 - 2, rgb565(236, 236, 240));
    fillRect(x0 + 2, y0 + 2, x1 - 2, y0 + 5, rgb565(255, 255, 255));
    fillRect(x - 5, y0 + 9, x + 4, y1 - 9, rgb565(210, 36, 30));        // the cross
    fillRect(x - 14, y0 + 18, x + 13, y0 + 26, rgb565(210, 36, 30));
    fillRect(x1 - 6, y0 + 16, x1 - 4, y0 + 28, rgb565(150, 150, 156));  // the catch
  }
}

static void paintDecor(const Decor &d) {
  switch (d.kind) {
    case DEC_CABINET: paintCabinet(d.x, d.y); break;
    case DEC_SAFE:    paintSafe(d.x, d.y); break;
    case DEC_PLANT:   paintPlant(d.x, d.y); break;
    case DEC_RADAR:   paintRadar(d.x, d.y); break;
    case DEC_PANEL:   paintPanel(d.x, d.y); break;
    case DEC_DESK:    paintDesk(d.x, d.y); break;
    case DEC_COOLER:  paintCooler(d.x, d.y); break;
    case DEC_SHELF:   paintShelf(d.x, d.y); break;
    case DEC_COAT:    paintCoatStand(d.x, d.y); break;
    case DEC_COMPUTER: paintComputerDesk(d.x, d.y); break;
    case DEC_CUPBOARD: paintCupboard(d.x, d.y); break;
    case DEC_SERVER:  paintServer(d.x, d.y); break;
    case DEC_FIRSTAID: paintFirstAid(d.x, d.y, rooms[curRoom].kitUsed); break;
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
    } else if (d.kind == DEC_COMPUTER) {     // lines of green text scrolling up the screen, a cursor blinking
      int sx0 = d.x - 28, sy0 = d.y - 102;
      int scroll = (int)(clockT * 2.5f) + d.seed;
      for (int row = 0; row < 5; row++) {
        uint32_t h = (uint32_t)(scroll + row) * 2654435761u;
        int len = 10 + (int)((h >> 20) % 28);
        fillRect(sx0 + 2, sy0 + 3 + row * 5, sx0 + 2 + len, sy0 + 4 + row * 5, rgb565(70, 220, 110));
      }
      if (fmodf(clockT, 0.8f) < 0.4f) fillRect(sx0 + 4, sy0 + 27, sx0 + 8, sy0 + 28, rgb565(140, 255, 170));
    } else if (d.kind == DEC_SERVER) {       // rows of lamps winking
      int x0 = d.x - 23, top = d.y - 120;
      for (int r = 0; r < 4; r++)
        for (int k = 0; k < 6; k++) {
          int ph = (int)(clockT * 4.0f) + k * 5 + r * 11 + d.seed;
          if (!(((ph * 2654435761u) >> 12) & 1)) continue;
          uint16_t col = (k + r) % 3 == 0 ? rgb565(255, 80, 60) : ((k + r) % 3 == 1 ? rgb565(80, 230, 120) : rgb565(255, 200, 70));
          fillRect(x0 + k * 7, top + 46 + r * 14, x0 + k * 7 + 3, top + 49 + r * 14, col);
        }
      // the tape reels turning
      for (int k = 0; k < 2; k++) {
        float a = clockT * (k ? 3.0f : -2.2f);
        int cx = d.x - 12 + k * 24, cy = top + 24;
        drawLineF(cx + 0.5f, cy + 0.5f, cx + 0.5f + cosf(a) * 6, cy + 0.5f + sinf(a) * 6, rgb565(150, 154, 164), 1.5f);
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
  for (int i = 0; i < t.spotCount && decorCount < ri.decorN; i++) {
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
  // the first aid kit, on the next spot
  if (ri.kit && ri.decorN < t.spotCount) {
    const Spot &s = t.spots[order[ri.decorN]];
    Decor &d = decor[decorCount++];
    d.kind = DEC_FIRSTAID;
    d.x = s.x;
    d.y = s.y;
    d.seed = 0;
  }
  rngState = save;
}

// Where a room's security cameras hang (top left / top right corners)
static float cameraX(const RoomInfo &ri, bool right) {   // in from the corner, clear of any lift shaft
  const WideRoom &t = wideRooms[ri.tpl];
  const int tryAt[3] = { 150, 300, 110 };
  for (int k = 0; k < 3; k++) {
    int x = right ? SCREEN_W - tryAt[k] : tryAt[k];
    bool clear = true;
    for (int i = 0; i < t.liftCount; i++)
      if (x > t.lifts[i].x0 - 44 && x < t.lifts[i].x1 + 44) clear = false;
    if (clear) return (float)x;
  }
  return right ? SCREEN_W - 150.0f : 150.0f;
}
static int roomCameras(const RoomInfo &ri, float *xs) {
  if (ri.hazard == HZ_CAMERA2) {
    xs[0] = cameraX(ri, false);
    xs[1] = cameraX(ri, true);
    return 2;
  }
  if (ri.hazard == HZ_CAMERA) {
    xs[0] = cameraX(ri, (ri.seed >> 5) & 1);
    return 1;
  }
  return 0;
}
#define CAM_Y (CEIL_Y + 40)

// ---- the walls' ornaments: where they go
// Kept clear: the platforms, the lift shafts, the equipment (and the space
// over it for the search bar), the doorways and their lamps, the room's number
struct Box { int16_t x0, y0, x1, y1; };
static Box keepClear[48];
static int keepClearN = 0;
static void clearBox(int x0, int y0, int x1, int y1) {
  if (keepClearN < 48) keepClear[keepClearN++] = { (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1 };
}
static bool wallFree(int x0, int y0, int x1, int y1) {
  if (x0 < 26 || x1 > SCREEN_W - 26 || y0 < CEIL_Y + 26 || y1 > ROOM_FLOOR_Y - 6) return false;
  for (int i = 0; i < keepClearN; i++) {
    const Box &b = keepClear[i];
    if (x0 <= b.x1 && x1 >= b.x0 && y0 <= b.y1 && y1 >= b.y0) return false;
  }
  return true;
}
static void ornHalf(int kind, int r, int &hw, int &hh) {
  switch (kind) {
    case ORN_REDLIGHT: hw = hh = 24; break;
    case ORN_FAN:      hw = hh = 34; break;
    case ORN_BIGFAN:   hw = hh = r + 16; break;
    case ORN_TUBE:     hw = 26; hh = r + 18; break;
    case ORN_GAUGE:    hw = hh = 26; break;
    case ORN_NOTICE:   hw = 78; hh = 34; break;
    default:           hw = 60; hh = 66; break;   // ORN_VOLTAGE
  }
}
static bool addOrnament(int kind, int x, int y, int r) {
  int hw, hh;
  ornHalf(kind, r, hw, hh);
  if (ornCount >= MAX_ORNAMENTS || !wallFree(x - hw, y - hh, x + hw, y + hh)) return false;
  orn[ornCount++] = { (uint8_t)kind, (int16_t)x, (int16_t)y, (int16_t)r, (uint16_t)rnd32() };
  clearBox(x - hw, y - hh, x + hw, y + hh);
  return true;
}
static void tryOrnament(int kind, int r) {   // a few goes at a random free place
  for (int k = 0; k < 80; k++)
    if (addOrnament(kind, 40 + rnd(SCREEN_W - 80), CEIL_Y + 40 + rnd(ROOM_FLOOR_Y - CEIL_Y - 80), r)) return;
}

static void placeOrnaments(const RoomInfo &ri) {
  uint32_t save = rngState;
  rngState = ri.seed * 2246822519u + 777u;
  ornCount = keepClearN = 0;
  for (int i = 0; i < platCount; i++) clearBox(plats[i].x0 - 2, plats[i].y - 4, plats[i].x1 + 2, plats[i].y >= ROOM_FLOOR_Y ? SCREEN_H : plats[i].y + PLAT_T + 4);
  for (int i = 0; i < liftCount; i++) clearBox(lifts[i].x0 - 10, shaftTop(lifts[i]) - 20, lifts[i].x1 + 10, SCREEN_H);
  for (int i = 0; i < decorCount; i++) {
    int hw = decorW[decor[i].kind] / 2;
    clearBox(decor[i].x - hw - 6, decor[i].y - decorH[decor[i].kind] - 44, decor[i].x + hw + 6, decor[i].y);
  }
  float cx[2];
  for (int k = roomCameras(ri, cx) - 1; k >= 0; k--) clearBox((int)cx[k] - 44, CEIL_Y, (int)cx[k] + 44, CAM_Y + 40);
  clearBox(0, doorYL - DOOR_H - 52, 124, doorYL);
  clearBox(SCREEN_W - 125, doorYR - DOOR_H - 52, SCREEN_W, doorYR);
  char b[16];
  snprintf(b, sizeof(b), "%d", (int)(&ri - rooms) % 100 + 1);
  int tw = (int)(vectorTextWidth(b, 5.0f) / 2) + 16, sx = signX > 0 ? signX : SCREEN_W / 2;
  clearBox(sx - tw, signY - 26, sx + tw, signY + 26);
  // HIGH VOLTAGE: where it is seen, high on the wall
  if (ri.hazard == HZ_ELECTRIC) {
    bool put = false;
    for (int k = 0; k < 60 && !put; k++) put = addOrnament(ORN_VOLTAGE, 160 + rnd(SCREEN_W - 320), CEIL_Y + 100 + rnd(300), 0);
    for (int y = CEIL_Y + 96; y < ROOM_FLOOR_Y - 60 && !put; y += 6)   // (then anywhere it fits)
      for (int x = 90; x < SCREEN_W - 90 && !put; x += 6) put = addOrnament(ORN_VOLTAGE, x, y, 0);
  }
  // a big fan, wherever there is the biggest open space (in some rooms there is none)
  int bestR = 0, bx = 0, by = 0;
  for (int r = 104; r >= BIG_FAN_MIN && !bestR; r -= 6)
    for (int y = CEIL_Y + 26 + r + 16; y <= ROOM_FLOOR_Y - 6 - r - 16 && !bestR; y += 8)
      for (int x = 26 + r + 16; x <= SCREEN_W - 26 - r - 16; x += 8)
        if (wallFree(x - r - 16, y - r - 16, x + r + 16, y + r + 16)) {
          bestR = r; bx = x; by = y;
          break;
        }
  if (bestR) addOrnament(ORN_BIGFAN, bx, by, bestR);
  // and the rest, a few of each
  for (int n = 2 + rnd(2); n > 0; n--) tryOrnament(ORN_REDLIGHT, 0);
  for (int n = 1 + rnd(2); n > 0; n--) tryOrnament(ORN_FAN, 24);
  for (int n = rnd(3); n > 0; n--) tryOrnament(ORN_TUBE, 52 + rnd(3) * 8);
  for (int n = rnd(3); n > 0; n--) tryOrnament(ORN_GAUGE, 0);
  if (rnd(3)) tryOrnament(ORN_NOTICE, 0);
  rngState = save;
}

// ---- the walls' ornaments: painted (the parts that stay still)
static const char *const noticeText[][2] = {
  { "RESTRICTED", "AREA" }, { "QUANTUM CORE", "LEVEL 4" }, { "AUTHORISED", "STAFF ONLY" },
  { "WARNING", "TIME FIELD" }, { "SECTOR", "Q-7" }, { "WEAR", "GOGGLES" }, { "NO", "ENTRY" }, { "CAUTION", "COOLANT" },
};

static void paintVoltageSign(int cx, int cy) {   // NUCLEAR MISSION's HIGH VOLTAGE sign, on a plate
  fillRect(cx - 56, cy - 62, cx + 56, cy + 62, rgb565(36, 38, 44));
  fillRect(cx - 52, cy - 58, cx + 52, cy + 58, rgb565(226, 222, 206));
  for (int k = 0; k < 4; k++) fillCircle(cx + (k & 1 ? 46 : -46), cy + (k & 2 ? 52 : -52), 2, rgb565(120, 120, 120));
  int ty = cy - 14;
  int xs[3] = { cx, cx + 48, cx - 48 }, ys[3] = { ty - 44, ty + 38, ty + 38 };
  fillPolygon(xs, ys, 3, rgb565(16, 16, 18));
  int xi[3] = { cx, cx + 38, cx - 38 }, yi[3] = { ty - 30, ty + 32, ty + 32 };
  fillPolygon(xi, yi, 3, rgb565(236, 196, 40));
  int bx[6] = { cx + 4, cx - 12, cx - 1, cx - 8, cx + 12, cx + 2 };
  int by[6] = { ty - 18, ty + 6, ty + 6, ty + 26, ty - 2, ty - 2 };
  fillPolygon(bx, by, 6, rgb565(16, 16, 18));
  drawVectorText("HIGH", cx, cy + 38, 1.7f, rgb565(190, 30, 24), true);
  drawVectorText("VOLTAGE", cx, cy + 52, 1.7f, rgb565(190, 30, 24), true);
}

static void paintOrnament(const Ornament &o) {
  int x = o.x, y = o.y;
  uint16_t metal = rgb565(84, 90, 102), metalHi = rgb565(140, 146, 158), metalLo = rgb565(44, 48, 56), bolt = rgb565(160, 166, 176);
  switch (o.kind) {
    case ORN_REDLIGHT:               // NUCLEAR MISSION's warning light: the plate and the cage
      fillRect(x - 20, y - 20, x + 19, y + 19, rgb565(52, 56, 64));
      fillRect(x - 20, y - 20, x + 19, y - 19, rgb565(150, 156, 164));
      for (int k = 0; k < 4; k++) fillCircle(x + (k & 1 ? 14 : -15), y + (k & 2 ? 14 : -15), 1, rgb565(150, 156, 164));
      fillCircle(x, y, 13, rgb565(30, 30, 34));
      fillCircle(x, y, 10, rgb565(70, 12, 10));
      break;
    case ORN_FAN: {                  // a wall fan: a square housing round a round hole
      fillRect(x - 32, y - 32, x + 31, y + 31, metal);
      fillRect(x - 32, y - 32, x + 31, y - 30, metalHi);
      fillRect(x - 32, y + 29, x + 31, y + 31, metalLo);
      for (int k = 0; k < 4; k++) fillCircle(x + (k & 1 ? 26 : -27), y + (k & 2 ? 26 : -27), 2, bolt);
      fillCircle(x, y, o.r + 3, metalLo);
      fillCircle(x, y, o.r, rgb565(10, 12, 16));
      break;
    }
    case ORN_BIGFAN: {               // a big extractor fan: a heavy ring, struts out to the wall
      int r = o.r;
      for (int k = 0; k < 4; k++) {
        float a = k * HALF_PI + 0.785f;
        drawLineF(x + cosf(a) * (r + 6), y + sinf(a) * (r + 6), x + cosf(a) * (r + 15), y + sinf(a) * (r + 15), metalLo, 6);
      }
      fillCircle(x, y, r + 10, metalLo);
      fillCircle(x, y, r + 8, metal);
      fillCircle(x - 2, y - 2, r + 3, metalHi);
      fillCircle(x, y, r + 3, rgb565(64, 70, 80));
      fillCircle(x, y, r, rgb565(8, 10, 14));
      for (int k = 0; k < 12; k++) {
        float a = k * TWO_PI / 12;
        fillCircle(x + (int)(cosf(a) * (r + 6)), y + (int)(sinf(a) * (r + 6)), 2, bolt);
      }
      break;
    }
    case ORN_TUBE: {                 // a coolant tube: glass between two steel caps, the liquid glowing
      int h = o.r;
      fillRect(x - 6, y - h - 18, x + 5, y + h + 18, metalLo);          // its pipe, in and out
      fillRect(x - 20, y - h - 10, x + 19, y - h, metal);
      fillRect(x - 20, y - h - 10, x + 19, y - h - 9, metalHi);
      fillRect(x - 20, y + h, x + 19, y + h + 10, metal);
      fillRect(x - 20, y + h + 9, x + 19, y + h + 10, metalLo);
      for (int yy = y - h + 1; yy < y + h; yy++) {
        int k = (yy - (y - h)) * 60 / (2 * h);
        fillRect(x - 15, yy, x + 14, yy, rgb565(20 + k / 3, 120 + k, 170 + k));
      }
      fillRect(x - 12, y - h + 3, x - 9, y + h - 3, rgb565(170, 240, 255));   // the glass shines
      break;
    }
    case ORN_GAUGE: {                // a pressure gauge: the dial, its marks, a red part at the top end
      fillCircle(x, y, 22, metalLo);
      fillCircle(x, y, 20, metalHi);
      fillCircle(x, y, 17, rgb565(232, 230, 220));
      for (int k = 0; k <= 8; k++) {
        float a = 2.36f + k * 0.589f;   // (from bottom left round to bottom right)
        uint16_t c = k >= 7 ? rgb565(210, 40, 30) : rgb565(40, 40, 44);
        drawLineF(x + 0.5f + cosf(a) * 12, y + 0.5f + sinf(a) * 12, x + 0.5f + cosf(a) * 16, y + 0.5f + sinf(a) * 16, c, k >= 7 ? 2.0f : 1.2f);
      }
      fillRect(x - 4, y + 22, x + 3, y + 26, metalLo);
      break;
    }
    case ORN_NOTICE: {               // a notice: yellow and black edge, two lines
      const char *const *t = noticeText[o.seed % (sizeof(noticeText) / sizeof(noticeText[0]))];
      fillRect(x - 74, y - 30, x + 73, y + 29, rgb565(20, 20, 22));
      hazardBand(x - 72, x + 71, y - 28, y - 21);
      hazardBand(x - 72, x + 71, y + 20, y + 27);
      fillRect(x - 72, y - 20, x + 71, y + 19, rgb565(232, 228, 210));
      drawVectorText(t[0], x, y - 7, 1.8f, rgb565(20, 20, 24), true);
      drawVectorText(t[1], x, y + 9, 1.8f, rgb565(180, 30, 24), true);
      break;
    }
    case ORN_VOLTAGE:
      paintVoltageSign(x, y);
      break;
  }
}

// ---- the walls' ornaments: what moves (each frame)
static void drawOrnamentsAnim() {
  for (int i = 0; i < ornCount; i++) {
    const Ornament &o = orn[i];
    int x = o.x, y = o.y;
    switch (o.kind) {
      case ORN_REDLIGHT: {           // pulsing red (quicker in the last minute)
        float period = timeLeft < 60 ? 0.8f : 1.6f;
        float k = 0.5f - 0.5f * cosf(TWO_PI * (clockT + (o.seed % 100) * 0.016f) / period);
        int r = 80 + (int)(175 * k), g = 10 + (int)(40 * k), b = 8 + (int)(30 * k);
        glowDisc(x + 0.5f, y + 0.5f, 60, 255, 30, 20, 0.45f * k);
        fillCircle(x, y, 10, rgb565(r, g, b));
        fillCircle(x - 3, y - 3, 3, rgb565(r + (255 - r) / 2, g + (255 - g) / 2, b + (255 - b) / 2));
        drawLine(x - 10, y, x + 10, y, rgb565(30, 30, 34), false);
        drawLine(x, y - 10, x, y + 10, rgb565(30, 30, 34), false);
        break;
      }
      case ORN_FAN:
      case ORN_BIGFAN: {             // the blades turning behind a guard
        bool big = o.kind == ORN_BIGFAN;
        int r = o.r;
        int blades = big ? 5 : 4;
        float spin = clockT * (big ? 2.6f : 9.0f) * ((o.seed & 1) ? 1 : -1) + o.seed;
        uint16_t blade = big ? rgb565(112, 118, 130) : rgb565(100, 106, 118), edge = rgb565(150, 156, 168);
        for (int k = 0; k < blades; k++) {
          float a = spin + k * TWO_PI / blades;
          float w0 = big ? 0.16f : 0.22f, w1 = big ? 0.42f : 0.5f;   // narrow at the hub, wide at the tip
          float hub = r * 0.2f, tip = r - 3.0f;
          float xs[4] = { x + 0.5f + cosf(a - w0) * hub, x + 0.5f + cosf(a - w1) * tip, x + 0.5f + cosf(a + w1 * 0.4f) * tip, x + 0.5f + cosf(a + w0) * hub };
          float ys[4] = { y + 0.5f + sinf(a - w0) * hub, y + 0.5f + sinf(a - w1) * tip, y + 0.5f + sinf(a + w1 * 0.4f) * tip, y + 0.5f + sinf(a + w0) * hub };
          fillPolygonF(xs, ys, 4, blade);
          drawLineF(xs[0], ys[0], xs[1], ys[1], edge, big ? 1.5f : 1.0f);
        }
        fillCircle(x, y, (int)(r * 0.24f), rgb565(60, 64, 74));
        fillCircle(x, y, (int)(r * 0.12f), rgb565(150, 156, 168));
        uint16_t guard = rgb565(70, 76, 88);   // the guard in front
        if (big) {
          drawCircle(x, y, r * 2 / 3, guard);
          drawCircle(x, y, r / 3, guard);
          for (int k = 0; k < 4; k++) {
            float a = k * HALF_PI;
            drawLineF(x + 0.5f + cosf(a) * r * 0.24f, y + 0.5f + sinf(a) * r * 0.24f, x + 0.5f + cosf(a) * r, y + 0.5f + sinf(a) * r, guard, 2.5f);
          }
        } else {
          for (int k = -2; k <= 2; k++) drawLineF(x - r + 1.0f, y + 0.5f + k * 8, x + r, y + 0.5f + k * 8, guard, 1.0f);
        }
        break;
      }
      case ORN_TUBE: {               // bubbles rising through the coolant
        int h = o.r;
        for (int k = 0; k < 5; k++) {
          float sp = 30 + (o.seed >> k & 7) * 6;
          float p = fmodf(clockT * sp + k * 37 + o.seed, (float)(2 * h - 12));
          int by = y + h - 6 - (int)p, bx = x - 4 + (int)(6 * sinf(clockT * 3 + k * 1.7f)) + (k - 2) * 2;
          drawCircle(bx, by, 1 + (k & 1) + (p > h ? 1 : 0), rgb565(210, 250, 255));
        }
        break;
      }
      case ORN_GAUGE: {              // the needle twitching near the top of its range
        float a = 2.36f + 0.589f * (5.6f + 0.6f * sinf(clockT * 1.3f + o.seed) + 0.25f * sinf(clockT * 11.0f + o.seed * 0.7f));
        drawLineF(x + 0.5f, y + 0.5f, x + 0.5f + cosf(a) * 14, y + 0.5f + sinf(a) * 14, rgb565(200, 30, 24), 1.6f);
        fillCircle(x, y, 2, rgb565(40, 40, 44));
        break;
      }
    }
  }
}

// The back wall's colour for each column of rooms (A-B ... I-J), the panels; the wall
// between them is a little darker. To try out: change these and build again.
static const uint8_t wallColours[COLS][3] = {
  {  30,  70,  82 },   // column 1: teal
  {  86,  40,  44 },   // 2: maroon
  {  64,  74,  36 },   // 3: olive
  {  34,  46,  96 },   // 4: navy blue
  {  70,  44,  88 },   // 5: purple
  {  96,  62,  34 },   // 6: rust brown
  {  30,  78,  50 },   // 7: forest green
  {  90,  78,  44 },   // 8: mustard
  {  70,  40,  70 },   // 9: plum
};

static void paintRoom() {
  gfxTarget = roomBg;
  gfxClipAll();
  // the back wall: each column of rooms has its own colour (wallColours, to try out)
  const uint8_t *wc = wallColours[rooms[curRoom].col % COLS];
  int wr = wc[0], wg = wc[1], wb = wc[2];
  fillRect(0, HUD_H, SCREEN_W - 1, PLAY_Y1, rgb565(wr * 3 / 4, wg * 3 / 4, wb * 3 / 4));
  uint16_t panel = rgb565(wr, wg, wb), seam = rgb565(wr / 3, wg / 3, wb / 3);
  for (int x = 0; x < SCREEN_W; x += 144) {
    fillRect(x + 6, CEIL_Y + 24, x + 137, ROOM_FLOOR_Y - 8, panel);
    fillRect(x, HUD_H, x + 3, ROOM_FLOOR_Y - 1, seam);
  }
  uint16_t rib = rgb565(wr * 5 / 8, wg * 5 / 8, wb * 5 / 8), rivet = rgb565(wr + 40, wg + 40, wb + 40);
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
  // the room's number, stencilled on the back wall
  char b[16];
  snprintf(b, sizeof(b), "%d", curRoom + 1);
  drawVectorText(b, signX > 0 ? signX : SCREEN_W / 2, signY, 5.0f, rgb565(70, 80, 96), true);
  for (int i = 0; i < ornCount; i++) paintOrnament(orn[i]);
  float cx[2];
  for (int k = roomCameras(rooms[curRoom], cx) - 1; k >= 0; k--) {   // the cameras' ceiling mounts and arms
    int x = (int)cx[k];
    fillRect(x - 22, CEIL_Y - 4, x + 22, CEIL_Y + 2, rgb565(70, 76, 88));
    fillRect(x - 22, CEIL_Y - 4, x + 22, CEIL_Y - 3, rgb565(130, 136, 148));
    fillRect(x - 3, CEIL_Y + 2, x + 3, CAM_Y - 4, rgb565(90, 96, 108));
    fillCircle(x, CAM_Y, 6, rgb565(50, 54, 64));
  }
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
  char b[24];
  snprintf(b, sizeof(b), "ROOM %d", room + 1);
  float w = vectorTextWidth(b, 2.2f);
  drawVectorText(b, right ? x0 - 22 - w / 2 : x1 + 22 + w / 2, top + 14, 2.2f, rgb565(170, 230, 180), false);
  fillRect(fx0, top, fx0 + 9, fy - 1, frame);
  fillRect(right ? fx0 : fx0 + 7, top, right ? fx0 + 2 : fx0 + 9, fy - 1, frameHi);
  fillRect(x0, top - 4, x1, top - 1, frame);
}

// The air-conditioning duct in the catwalk's ceiling: the duct coming out of
// the wall, its grille rattling and falling out, then lying on the catwalk
static void drawVent(int fy, int cy) {
  int vx = ventX;
  fillRect(vx - 34, cy - 40, vx + 34, cy - 6, rgb565(156, 162, 170));    // the duct
  fillRect(vx - 34, cy - 40, vx - 31, cy - 6, rgb565(196, 202, 210));
  fillRect(vx + 31, cy - 40, vx + 34, cy - 6, rgb565(98, 104, 112));
  for (int x = vx - 26; x < vx + 26; x += 13) fillRect(x, cy - 36, x + 1, cy - 10, rgb565(120, 126, 136));
  fillRect(vx - 38, cy - 8, vx + 38, cy, rgb565(124, 130, 138));        // its frame in the ceiling
  float t = introT;
  bool fallen = ventOpen || (t >= 0.8f);
  if (!fallen) {
    int sh = (t >= 0 && t < 0.8f) ? (int)(2 * sinf(t * 60)) : 0;
    fillRect(vx - 30 + sh, cy - 6, vx + 30 + sh, cy, rgb565(180, 186, 194));
    for (int x = vx - 26; x < vx + 26; x += 6) fillRect(x + sh, cy - 5, x + 2 + sh, cy - 1, rgb565(90, 96, 106));
    return;
  }
  fillRect(vx - 30, cy - 6, vx + 30, cy, rgb565(10, 10, 12));            // the hole
  float ft = (t >= 0) ? t - 0.8f : 9.0f;                                 // the grille: falling, then on the floor
  float gx, gy, ang = 0;
  if (ft < 0.42f) {
    gx = vx + ft * 70;
    gy = cy - 3 + 0.5f * 1100 * ft * ft;
    ang = ft * 5;
  } else {
    gx = vx + 40;
    gy = fy - 3;
  }
  if (gy > fy - 3) gy = fy - 3;
  float c = cosf(ang), sn = sinf(ang);
  float px[4] = { -30, 30, 30, -30 }, py[4] = { -3, -3, 3, 3 }, xs[4], ys[4];
  for (int k = 0; k < 4; k++) {
    xs[k] = gx + px[k] * c - py[k] * sn;
    ys[k] = gy + px[k] * sn + py[k] * c;
  }
  fillPolygonF(xs, ys, 4, rgb565(110, 116, 128));
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
      if (curShaft == ventShaft && L == ventFloor && (ventX > SHAFT_X1) == (side == 1)) drawVent(fy, cy);
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
  placeOrnaments(ri);
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

// Coming into a room, or back in at its door: the cameras give him a moment,
// the drone starts again from the far side
static void resetHazardsForEntry(bool fromLeft) {
  camGrace = CAM_ENTRY_GRACE;
  camCaughtPending = false;
  for (int k = 0; k < camCount; k++) {
    cams[k].movingT = 0;
    cams[k].view = 0;
  }
  drX = fromLeft ? SCREEN_W - 180.0f : 180.0f;
  drY = CEIL_Y + 90.0f;
  drVX = drVY = 0;
  drGrace = DRONE_GRACE;
  drFireT = 0.45f;
  drFlashT = 0;
  drLight = 0;
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].life = 0;
}

static void enterRoom(int idx, bool fromLeft) {
  mode = MODE_ROOM;
  curRoom = idx;
  buildRoom(idx, fromLeft);
  paintRoom();
  rooms[idx].visited = true;
  curHazard = rooms[idx].hazard;
  elecState = 0;
  elecT = 0;
  elecSafeFor = ELEC_SAFE_MIN + fxRnd(0, (int)((ELEC_SAFE_MAX - ELEC_SAFE_MIN) * 100)) / 100.0f;
  darkT = 0;
  lightsOut = false;
  int y = fromLeft ? doorYL : doorYR;
  placeAgent(fromLeft ? 36 : SCREEN_W - 36, (float)y, fromLeft ? 1 : -1);
  // the cameras: each starts its sweep at the far end of the room
  float cx[2];
  camCount = roomCameras(rooms[idx], cx);
  for (int k = 0; k < camCount; k++) {
    Camera &c = cams[k];
    c.x = cx[k];
    c.y = CAM_Y;
    c.mid = c.x < SCREEN_W / 2 ? CAM_MID_DEG : -CAM_MID_DEG;
    c.t = CAM_PERIOD * (k ? 0.75f : 0.25f);
    c.angle = c.mid + (c.mid > 0 ? 1 : -1) * CAM_SWING_DEG * sinf(TWO_PI * c.t / CAM_PERIOD);
    c.movingT = 0;
    c.view = 0;
  }
  resetHazardsForEntry(fromLeft);
}

static void enterShaft(int s, int f, bool fromLeft) {
  mode = MODE_SHAFT;
  curShaft = s;
  carFloor = f;
  carWorldY = (float)(f * LEVEL_GAP);
  carMoving = false;
  platCount = liftCount = pitCount = 0;
  decorCount = ornCount = 0;
  curHazard = HZ_NONE;
  camCount = 0;
  lightsOut = false;
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
    case ST_SHOCK: return ANIM_FALL;   // (arms thrown up)
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

static void endGame(EndReason why) {
  if (gameOver == END_NONE) {
    gameOver = why;
    gameOverT = 0;
  }
}

static void hurt(int amount) {
  health -= amount;
  hurtFlashT = 1.0f;
  if (health <= 0) {
    health = 0;
    endGame(END_HEALTH);
  }
}

// Down: back to the door he came in by (after a pit), or getting up where he lies (a long drop)
static void knockOut(bool getUp = false) {
  agLift = -1;
  koGetUp = getUp;
  setState(ST_KO, 0.10f);
}

static void respawn() {
  if (mode == MODE_ROOM) {
    buildRoom(curRoom, entryFacing > 0);   // (the lift pads back where they were)
  }
  float x = clampv(entryX, edgeMin(), edgeMax());
  placeAgent(x, entryY, entryFacing);
  if (mode == MODE_ROOM) resetHazardsForEntry(entryFacing > 0);
}

// UP / DOWN beside a lift shaft, at one of its stops, with the pad somewhere
// else: the pad comes to him
static bool summonLift() {
  for (int i = 0; i < liftCount; i++) {
    Lift &l = lifts[i];
    if (agX < l.x0 - 40 || agX > l.x1 + 40) continue;   // (an empty pad on its way can be sent somewhere else)
    for (int k = 0; k < l.stopCount; k++) {
      if (fabsf(l.stops[k] - agY) > 0.5f || k == l.at) continue;
      l.at = (int8_t)k;
      l.moving = true;
      return true;
    }
  }
  return false;
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
    bool carrying = (agLift == i && agState == ST_LIFT);
    float step = LIFT_SPEED * (carrying ? 1.0f : 2.0f) * dt;   // (an empty pad comes twice as fast)
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

// A piece that can't be searched (any more): searched already, or a used first aid kit
static bool pieceDone(int i) {
  const RoomInfo &ri = rooms[curRoom];
  return decor[i].kind == DEC_FIRSTAID ? ri.kitUsed : ri.searched[i];
}
// How far it has been searched (the kit's is kept after the equipment's)
static float &pieceProgress(int i) {
  RoomInfo &ri = rooms[curRoom];
  return decor[i].kind == DEC_FIRSTAID ? ri.progress[MAX_DECOR] : ri.progress[i];
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
      } else if (upDown != 0 && agLift < 0 && mode == MODE_ROOM && summonLift()) {
        // (the pad is on its way: he waits)
      } else if (pressed & BTN_JUMP) {
        if (want) agFacing = want;
        startJump();
      } else if ((held & BTN_USE) && mode == MODE_ROOM && decorAt() >= 0 && pieceDone(decorAt())) {
        // searched already: it isn't searched again
        if (pressed & BTN_USE) {
          int i = decorAt();
          snprintf(floatText, sizeof(floatText), decor[i].kind == DEC_FIRSTAID ? "EMPTY" : "SEARCHED");
          floatColour = rgb565(150, 156, 170);
          nothingShowT = NOTHING_SHOW;
          nothingX = decor[i].x;
          nothingY = decor[i].y - decorH[decor[i].kind] - 22;
        }
      } else if ((held & BTN_USE) && mode == MODE_ROOM) {   // (nothing to search in the lift)
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
        break;
      }
      float p0 = fmodf(oldT / AG_RUN_CYCLE, 1.0f), p1 = fmodf(agT / AG_RUN_CYCLE, 1.0f);   // a footstep each stride
      if (p1 < p0 || (p0 < 0.5f && p1 >= 0.5f)) sfxPlay(SFX_STEP);
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
        int levels = (int)lroundf((agY - fallFromY) / 160.0f);   // how many levels he dropped
        int harm = levels >= 3 ? FALL_HURT_3 : (levels == 2 ? FALL_HURT_2 : 0);
        if (harm > 0) {
          hurt(harm);
          knockOut(true);
        } else {
          setState(ST_LAND, 0.06f);
        }
      } else if (agY > ROOM_FLOOR_Y + 2 && y0 <= ROOM_FLOOR_Y + 2) {   // below the floor: down a pit (nothing to land on now)
        sfxPlay(SFX_SCREAM);
      } else if (agY > SCREEN_H + 160) {   // down a pit
        hurt(FALL_HURT_3);
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
      if (i >= 0 && pieceDone(i)) i = -1;
      searchItem = i;
      if (i >= 0 && pieceProgress(i) < SEARCH_SECONDS) {   // (carrying on from wherever it was left)
        float &searchT = pieceProgress(i);
        searchBeepT += dt;                   // NUCLEAR MISSION's searching beeps
        if (searchBeepT >= SEARCH_BEEP_SECONDS) {
          searchBeepT = 0;
          sfxPlay(SFX_BEEP);
        }
        searchT += dt;
        if (searchT >= SEARCH_SECONDS) {
          RoomInfo &ri = rooms[curRoom];
          float px = decor[i].x, py = decor[i].y - decorH[decor[i].kind] - 22;
          if (decor[i].kind == DEC_FIRSTAID) {   // the first aid kit
            if (health >= HEALTH_MAX) {
              snprintf(floatText, sizeof(floatText), "HEALTH FULL");
              floatColour = rgb565(170, 170, 180);
              searchT = 0;                    // (not used: it can be searched again later)
            } else {
              health = HEALTH_MAX;
              ri.kitUsed = true;
              paintRoom();                    // (it hangs open now)
              snprintf(floatText, sizeof(floatText), "HEALTH RESTORED");
              sfxPlay(SFX_FANFARE);
              floatColour = rgb565(100, 240, 140);
            }
            nothingShowT = NOTHING_SHOW;
            nothingX = px;
            nothingY = py;
            break;
          }
          bool first = !ri.searched[i];
          ri.searched[i] = true;
          if (first) addScore(SEARCH_SCORE, px, py - 26);
          if (first && ri.holds[i]) {        // something in it
            sfxPlay(SFX_FOUND);
            sfxPlay(SFX_FANFARE);
            itemsFound++;
            addScore(FOUND_SCORE, px, py - 56);
            itemShowT = ITEM_SHOW;
            nothingShowT = 0;
          } else {
            nothingShowT = NOTHING_SHOW;
            nothingX = px;
            nothingY = py;
            snprintf(floatText, sizeof(floatText), "NOTHING");
            sfxPlay(SFX_CLUNK);
            floatColour = rgb565(170, 170, 180);
          }
        }
      }
      if (!(held & BTN_USE)) {
        searchItem = -1;
        setState(ST_IDLE, 0.18f);
      }
      break;
    }

    case ST_CAUGHT:                    // caught on camera: hands up, then back at the door
      if (gameOver == END_NONE && agT >= CAUGHT_SECONDS) respawn();
      break;

    case ST_SHOCK:                     // caught on the live floor: jolting, then down
      elecSoundT -= dt;
      if (elecSoundT <= 0) {
        elecSoundT = 0.12f;
        sfxPlay(SFX_ZAP);
      }
      if (agT >= SHOCK_SECONDS) {
        elecState = 0;                 // the shock trips the power: safe for a while
        elecT = 0;
        elecSafeFor = ELEC_AFTER_SHOCK;
        hurt(ELEC_HURT);
        knockOut();                    // (he wakes at the door he came in by)
      }
      break;

    case ST_KO:
      if (gameOver != END_NONE) break;
      if (koGetUp && agT >= HURT_SECONDS) setState(ST_IDLE, 0.35f);
      else if (!koGetUp && agT >= RESPAWN_SECONDS) respawn();
      break;
  }

  AgentPose target;
  float animT = agT;
  if (agState == ST_LAND) animT = AG_JUMP_DOWN + agT;
  agentSample(animFor(agState), animT, target);
  if (agState == ST_JUMP) target.lift *= jumpLiftScale;
  if (agState == ST_LAND) target.spin = 0;
  if (agState == ST_CAUGHT) {          // hands up
    target.armF = 168; target.elbowF = 30;
    target.armN = 162; target.elbowN = 36;
    target.head += 6;
  }
  if (agState == ST_SHOCK) {           // stiff and jolting on the spot
    target.plant = 1;
    target.torso += fxRnd(-12, 13);
    target.head += fxRnd(-15, 16);
    target.armN += fxRnd(-25, 26);
    target.armF += fxRnd(-25, 26);
    target.legN += fxRnd(-10, 11);
    target.legF += fxRnd(-10, 11);
    target.lift = fxRnd(0, 5);
  }
  if (blendLen > 0 && blendT < blendLen) {
    float k = blendT / blendLen;
    k = k * k * (3.0f - 2.0f * k);
    agentLerp(blendFrom, target, k, shownPose);
  } else {
    shownPose = target;
  }
}

// ============================================================ the hazards
// Electrified floor (NUCLEAR MISSION's): is he on the floor (or a lift pad down at it)?
static bool onRoomFloor() {
  if (fabsf(agY - ROOM_FLOOR_Y) > 0.5f) return false;
  switch (agState) {
    case ST_IDLE: case ST_RUN: case ST_USE: case ST_LAND: case ST_LIFT:
      return true;
    case ST_JUMP:
      return agT < AG_JUMP_UP || agT >= AG_JUMP_DOWN;   // (not while in the air)
    default:
      return false;
  }
}

// The lights flickering while the floor's warning runs: dim or not, at time t
static bool flickerDim(float t) {
  static const float on[][2] = { { 0.00f, 0.07f }, { 0.18f, 0.24f }, { 0.30f, 0.34f }, { 0.62f, 0.72f },
                                 { 0.90f, 0.95f }, { 1.02f, 1.10f }, { 1.30f, 1.36f }, { 1.42f, 1.52f } };
  for (int i = 0; i < 8; i++)
    if (t >= on[i][0] && t < on[i][1]) return true;
  return false;
}

// ---- security cameras
static bool agentMoving() {
  switch (agState) {
    case ST_RUN: case ST_JUMP: case ST_FALL: case ST_LAND: case ST_LIFT:
      return true;
    default:
      return false;
  }
}
static float feetY() { return agState == ST_JUMP ? jumpY0 - shownPose.lift * myScale : agY; }
static bool inBeam(const Camera &c, float px, float py) {
  float dx = px - c.x, dy = py - c.y;
  if (dy <= 0) return false;
  return fabsf(atan2f(dx, dy) * 57.29578f - c.angle) <= CAM_HALF_DEG;
}
static void startCaught() {
  if (agState == ST_LIFT && agLift >= 0) lifts[agLift].moving = false;
  searchItem = -1;
  camCaughtPending = false;
  setState(ST_CAUGHT, 0.15f);
}
static void updateCameras(float dt) {
  bool caught = agState == ST_CAUGHT || camCaughtPending;
  if (camCaughtPending && agState != ST_JUMP && agState != ST_FALL) {
    if (agState == ST_KO) camCaughtPending = false;   // (down a pit: back at the door anyway)
    else startCaught();
  }
  if (camGrace > 0) camGrace -= dt;
  float h = 100.0f * myScale, fy = feetY();
  for (int k = 0; k < camCount; k++) {
    Camera &c = cams[k];
    if (caught) {                      // it follows him
      if (c.view == 2) {
        float want = atan2f(agX - c.x, fmaxf(fy - 60 - c.y, 10.0f)) * 57.29578f;
        c.angle += (want - c.angle) * fminf(1.0f, dt * 6.0f);
      }
      continue;
    }
    c.t += dt;
    c.angle = c.mid + (c.mid > 0 ? 1 : -1) * CAM_SWING_DEG * sinf(TWO_PI * c.t / CAM_PERIOD);
    bool seen = inBeam(c, agX, fy - 8) || inBeam(c, agX, fy - h * 0.5f) || inBeam(c, agX, fy - h * 0.9f);
    if (camGrace > 0 || agState == ST_KO || agState == ST_SHOCK) seen = false;
    if (seen && agentMoving()) {
      c.movingT += dt;
      c.view = 1;
      if (c.movingT >= CAM_REACT) {    // CAUGHT
        c.view = 2;
        timeLeft -= CAM_PENALTY;
        if (timeLeft <= 0) {
          timeLeft = 0;
          endGame(END_TIME);
        }
        sfxPlay(SFX_ALARM);
        char b[48];
        snprintf(b, sizeof(b), "CAUGHT ON CAMERA!  -%d SECONDS", CAM_PENALTY);
        showMsg(b, 2.6f);
        if (agState == ST_JUMP || agState == ST_FALL) camCaughtPending = true;   // (hands up when he lands)
        else startCaught();
        return;
      }
    } else {
      c.movingT = 0;
      c.view = seen ? 1 : 0;
    }
  }
}

// ---- the drone: it drifts after him, and fires once he is in range
static float hitFlashT = 0, hitX = 0, hitY = 0;
static void updateDrone(float dt) {
  float fy = feetY();
  float ax = agX, ay = fy - 65 * myScale;           // his chest
  // where it wants to be: a little above him and off to its side of him
  float side = drX > agX ? 1.0f : -1.0f;
  float tx = agX + side * 150, ty = clampv(fy - 230.0f, CEIL_Y + 50.0f, ROOM_FLOOR_Y - 80.0f);
  if (drGrace > 0) {
    drGrace -= dt;
    tx = drX;
    ty = drY;
  }
  float dx = tx - drX, dy = ty - drY, d = sqrtf(dx * dx + dy * dy);
  float sp = DRONE_SPEED * fminf(1.0f, d / 40.0f);
  float wx = d > 0.1f ? dx / d * sp : 0, wy = d > 0.1f ? dy / d * sp : 0;
  float k = fminf(1.0f, dt * 1.5f);
  drVX += (wx - drVX) * k;
  drVY += (wy - drVY) * k;
  drX = clampv(drX + drVX * dt, 40.0f, SCREEN_W - 40.0f);
  drY = clampv(drY + drVY * dt, CEIL_Y + 30.0f, ROOM_FLOOR_Y - 30.0f);
  // its light, and its gun
  float ex = ax - drX, ey = ay - drY, dist = sqrtf(ex * ex + ey * ey);
  drLight = dist > DRONE_WARN ? 0 : (dist > DRONE_RANGE ? 1 : 2);
  bool canFire = drLight == 2 && drGrace <= 0 && agState != ST_KO && agState != ST_CAUGHT && agState != ST_SHOCK;
  if (drFlashT > 0) drFlashT -= dt;
  if (!canFire) {
    drFireT = 0.45f;                   // (a moment's warning before the first shot)
  } else if ((drFireT -= dt) <= 0) {
    drFireT = DRONE_FIRE_EVERY;
    for (int i = 0; i < MAX_BULLETS; i++) {
      Bullet &b = bullets[i];
      if (b.life > 0) continue;
      float gx = drX, gy = drY + 17;
      float bx = ax - gx, by = ay - gy, bl = fmaxf(1.0f, sqrtf(bx * bx + by * by));
      b.x = gx + bx / bl * 18;
      b.y = gy + by / bl * 18;
      b.vx = bx / bl * BULLET_SPEED;
      b.vy = by / bl * BULLET_SPEED;
      b.life = 1.6f;
      break;
    }
    drFlashT = 0.07f;
    sfxPlay(SFX_SHOT);
  }
  // the bullets (through anything, like the drone)
  if (hitFlashT > 0) hitFlashT -= dt;
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet &b = bullets[i];
    if (b.life <= 0) continue;
    b.life -= dt;
    b.x += b.vx * dt;
    b.y += b.vy * dt;
    if (b.x < -20 || b.x > SCREEN_W + 20 || b.y < HUD_H || b.y > SCREEN_H) b.life = 0;
    bool hittable = agState != ST_KO && agState != ST_CAUGHT;
    if (b.life > 0 && hittable && fabsf(b.x - agX) < 15 * myScale && b.y > fy - 125 * myScale && b.y < fy) {
      b.life = 0;
      hurt(BULLET_HURT);
      sfxPlay(SFX_HIT);
      hitFlashT = 0.15f;
      hitX = b.x;
      hitY = b.y;
    }
  }
}

static void updateHazards(float dt) {
  if (mode != MODE_ROOM || gameOver != END_NONE) return;
  if (camCount > 0) updateCameras(dt);
  if (curHazard == HZ_DRONE) updateDrone(dt);
  if (curHazard == HZ_DARK && !lightsOut) {   // a couple of flickers, then out
    darkT += dt;
    if (darkT >= 0.75f) {
      lightsOut = true;
      sfxPlay(SFX_POWERDOWN);
      showMsg("NIGHT VISION ON", 2.5f);
    }
  }
  if (curHazard != HZ_ELECTRIC || agState == ST_SHOCK || agState == ST_KO) return;
  elecT += dt;
  switch (elecState) {
    case 0:
      if (elecT >= elecSafeFor) {
        elecState = 1;
        elecT = 0;
        sfxPlay(SFX_BUZZ);
      }
      break;
    case 1:
      if (elecT >= ELEC_WARN_SECONDS) {
        elecState = 2;
        elecT = 0;
        elecSoundT = 0;
      }
      break;
    case 2:
      elecSoundT -= dt;
      if (elecSoundT <= 0) {
        elecSoundT = 0.28f;
        sfxPlay(SFX_ZAP);
      }
      if (onRoomFloor()) {             // caught on the live floor
        if (agState == ST_LIFT && agLift >= 0) lifts[agLift].moving = false;
        searchItem = -1;
        elecSoundT = 0;
        setState(ST_SHOCK, 0.05f);
        break;
      }
      if (elecT >= ELEC_LIVE_SECONDS) {
        elecState = 0;
        elecT = 0;
        elecSafeFor = ELEC_SAFE_MIN + fxRnd(0, (int)((ELEC_SAFE_MAX - ELEC_SAFE_MIN) * 100)) / 100.0f;
      }
      break;
  }
}

// A jagged arc from (x0, y0) to (x1, y1)
static void drawArc(int x0, int y0, int x1, int y1, int jag, uint16_t c, bool thick) {
  const int SEG = 6;
  int px = x0, py = y0;
  for (int i = 1; i <= SEG; i++) {
    int x = x0 + (x1 - x0) * i / SEG, y = y0 + (y1 - y0) * i / SEG;
    if (i < SEG) {
      x += fxRnd(-jag, jag + 1);
      y += fxRnd(-jag, jag + 1);
    }
    drawLine(px, py, x, y, c, thick);
    px = x;
    py = y;
  }
}

// Arcs along every stretch of floor (and the lift shafts' floors); live, or the odd spark of the warning
static void drawFloorArcs(bool live) {
  uint16_t glow = rgb565(120, 200, 255), core = rgb565(230, 245, 255);
  int y = ROOM_FLOOR_Y;
  for (int i = 0; i < platCount + liftCount; i++) {
    int x0, x1;
    if (i < platCount) {
      if (plats[i].y != ROOM_FLOOR_Y) continue;
      x0 = plats[i].x0;
      x1 = plats[i].x1 - 1;
    } else {
      const Lift &l = lifts[i - platCount];
      if (l.stops[l.stopCount - 1] != ROOM_FLOOR_Y) continue;
      x0 = l.x0;
      x1 = l.x1 - 1;
    }
    if (live) {
      fillRect(x0, y - 2, x1, y, glow);
      glowDisc((x0 + x1) * 0.5f, (float)y, (x1 - x0) * 0.35f + 30, 60, 120, 255, 0.18f);
      for (int x = x0; x < x1 - 20; x += 50) {
        int len = fxRnd(25, 60);
        int xe = x + len > x1 ? x1 : x + len;
        drawArc(x, y - 1, xe, y - 1, 10, fxRnd(0, 3) == 0 ? core : glow, fxRnd(0, 2) == 0);
        if (fxRnd(0, 4) == 0) drawArc(x + len / 2, y - 1, x + len / 2 + fxRnd(-20, 21), y - fxRnd(20, 45), 6, core, false);
      }
    } else if (fxRnd(0, 6) == 0) {
      int x = fxRnd(x0, x1 + 1);
      drawArc(x, y - 1, x + fxRnd(-30, 31), y - fxRnd(8, 26), 5, core, false);
    }
  }
}

// Sparks all over him while he is shocked
static void drawShockSparks() {
  float h = 100.0f * myScale;
  int cx = (int)agX, cy = (int)(agY - h * 0.5f);
  for (int i = 0; i < 7; i++) {
    float a = fxRnd(0, 628) / 100.0f;
    int r = fxRnd((int)(h * 0.3f), (int)(h * 0.7f));
    drawArc(cx, cy, cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r), 7, (i & 1) ? rgb565(230, 245, 255) : rgb565(120, 200, 255), i < 2);
  }
}

// The whole room at half brightness (the lights flickering)
static void dimRoom() {
  for (size_t i = (size_t)(HUD_H * UNIT_K) * FB_W; i < (size_t)FB_W * FB_H; i++)
    g_frame[i] = ((g_frame[i] >> 1) & 0x007f7f7fu) | 0xff000000u;
}

// Dark room: what his night vision goggles show. Everything is nearly black
// but a cone ahead of his eyes, a little below level, and a circle round him,
// shown in bright goggle green with faint scan lines (as NUCLEAR MISSION's)
static uint32_t nvgTab[2][256];          // brightness -> goggle green (plain, scan line)
static void eyePos(float &ex, float &ey) {
  float feet = (agState == ST_JUMP) ? jumpY0 - shownPose.lift * myScale : agY;
  float eye = 90.0f;
  if (agState == ST_USE) eye = 62.0f;
  else if (agState == ST_KO) eye = 30.0f;
  else if (agState == ST_JUMP || agState == ST_FALL) eye = 70.0f;
  ex = agX + agFacing * 8 * myScale;
  ey = feet - eye * myScale;
}
static void nightVision() {
  if (!nvgTab[0][255]) {
    for (int l = 0; l < 256; l++)
      for (int s = 0; s < 2; s++) {
        int v = (l - 6) * 5 / 2 + 8 - (s ? 22 : 0);   // (more contrast than the eye, not so much that it all turns flat green)
        v = clampv(v, 10, 255);
        nvgTab[s][l] = rgba(v / 5, v, v / 3);
      }
  }
  float ex, ey;
  eyePos(ex, ey);
  // the cone, in pixels
  const int N = 10;
  float cx[N + 2], cy[N + 2];
  cx[0] = ex * UNIT_K;
  cy[0] = ey * UNIT_K;
  float mid = agFacing > 0 ? 78.0f : -78.0f;   // degrees from straight down
  for (int i = 0; i <= N; i++) {
    float a = (mid - NVG_HALF_DEG + 2 * NVG_HALF_DEG * i / N) * 0.017453293f;
    cx[i + 1] = (ex + sinf(a) * NVG_RANGE) * UNIT_K;
    cy[i + 1] = (ey + cosf(a) * NVG_RANGE) * UNIT_K;
  }
  float rcx = agX * UNIT_K, rcy = (ey + 44 * myScale) * UNIT_K, rr = NVG_AROUND * myScale * UNIT_K;
  float ecx = cx[0], ecy = cy[0];
  float fadeFrom = NVG_RANGE * 0.65f * UNIT_K, fadeLen = NVG_RANGE * 0.35f * UNIT_K;
  int y0 = (int)(HUD_H * UNIT_K);
  for (int y = y0; y < FB_H; y++) {
    uint32_t *row = g_frame + (size_t)y * FB_W;
    float fy = y + 0.5f;
    // the cone's span on this row (it is convex)
    float a0 = 1e9f, a1 = -1e9f;
    for (int i = 0; i < N + 2; i++) {
      int j = (i + 1) % (N + 2);
      float ya = cy[i], yb = cy[j];
      if ((ya <= fy && yb > fy) || (yb <= fy && ya > fy)) {
        float x = cx[i] + (fy - ya) * (cx[j] - cx[i]) / (yb - ya);
        if (x < a0) a0 = x;
        if (x > a1) a1 = x;
      }
    }
    // the circle's
    float b0 = 1e9f, b1 = -1e9f, dy = fy - rcy;
    if (dy * dy < rr * rr) {
      float h = sqrtf(rr * rr - dy * dy);
      b0 = rcx - h;
      b1 = rcx + h;
    }
    int la = (int)clampv(a0, 0.0f, (float)FB_W), lb = (int)clampv(a1, 0.0f, (float)FB_W);
    int ca = (int)clampv(b0, 0.0f, (float)FB_W), cb = (int)clampv(b1, 0.0f, (float)FB_W);
    const uint32_t *tab = nvgTab[(y % 6) == 0];
    float edy = fy - ecy;
    for (int x = 0; x < FB_W; x++) {
      uint32_t p = row[x];
      bool inCone = x >= la && x < lb, inRing = x >= ca && x < cb;
      if (!inCone && !inRing) {
        row[x] = ((p >> 4) & 0x000f0f0fu) - ((p >> 6) & 0x00030303u) + 0xff000000u;   // almost black
        continue;
      }
      int r = p & 255, g = (p >> 8) & 255, b = (p >> 16) & 255;
      int lum = (r * 2 + g * 5 + b) >> 3;
      if (inCone && !inRing) {         // the far end of the cone fades
        float ddx = x + 0.5f - ecx, d = sqrtf(ddx * ddx + edy * edy);
        if (d > fadeFrom) lum = (int)(lum * fmaxf(0.25f, 1.0f - (d - fadeFrom) / fadeLen));
      }
      row[x] = tab[lum];
    }
  }
}

// What still glows in the dark: the EXIT lamps, the red lights, the lift pads' lamps
static void drawDarkGlows() {
  int lx, ly;
  exitLampAt(false, doorYL, lx, ly);
  fillRect(lx + 3, ly + 3, lx + 27, ly + 13, rgb565(60, 220, 110));
  glowDisc(lx + 15.0f, ly + 8.0f, 40, 40, 200, 90, 0.35f);
  exitLampAt(true, doorYR, lx, ly);
  fillRect(lx + 3, ly + 3, lx + 27, ly + 13, rgb565(60, 220, 110));
  glowDisc(lx + 15.0f, ly + 8.0f, 40, 40, 200, 90, 0.35f);
  for (int i = 0; i < ornCount; i++)
    if (orn[i].kind == ORN_REDLIGHT) {
      float k = 0.5f - 0.5f * cosf(TWO_PI * (clockT + (orn[i].seed % 100) * 0.016f) / (timeLeft < 60 ? 0.8f : 1.6f));
      fillCircle(orn[i].x, orn[i].y, 8, rgb565(80 + (int)(175 * k), 10, 8));
      glowDisc(orn[i].x + 0.5f, orn[i].y + 0.5f, 50, 255, 30, 20, 0.3f * k);
    }
  for (int i = 0; i < liftCount; i++) {
    int cx = (lifts[i].x0 + lifts[i].x1) / 2, y = (int)lroundf(lifts[i].y);
    fillRect(cx - 4, y + 4, cx + 3, y + 6, lifts[i].moving ? rgb565(255, 170, 40) : rgb565(80, 230, 120));
  }
  // the goggles' read-out
  drawVectorText("NIGHT VISION", 112, HUD_H + 22, 1.8f, rgb565(120, 255, 150), true);
  if (fmodf(clockT, 1.0f) < 0.6f) fillCircle(36, HUD_H + 22, 5, rgb565(120, 255, 150));
  uint16_t gc = rgb565(90, 220, 120);
  const int m = 14, L = 40, yt = HUD_H + 8, yb = PLAY_Y1 - 8;
  fillRect(m, yt, m + L, yt + 2, gc); fillRect(m, yt, m + 2, yt + L, gc);
  fillRect(SCREEN_W - 1 - m - L, yt, SCREEN_W - 1 - m, yt + 2, gc); fillRect(SCREEN_W - 3 - m, yt, SCREEN_W - 1 - m, yt + L, gc);
  fillRect(m, yb - 2, m + L, yb, gc); fillRect(m, yb - L, m + 2, yb, gc);
  fillRect(SCREEN_W - 1 - m - L, yb - 2, SCREEN_W - 1 - m, yb, gc); fillRect(SCREEN_W - 3 - m, yb - L, SCREEN_W - 1 - m, yb, gc);
}

// The cameras: each beam (see-through, yellow; amber on him; red when it has caught him), then the camera
static void drawCameras() {
  for (int k = 0; k < camCount; k++) {
    const Camera &c = cams[k];
    const int N = 6;
    float xs[N + 2], ys[N + 2];
    xs[0] = c.x;
    ys[0] = c.y;
    for (int i = 0; i <= N; i++) {
      float a = (c.angle - CAM_HALF_DEG + 2 * CAM_HALF_DEG * i / N) * 0.017453293f;
      xs[i + 1] = c.x + sinf(a) * 1500;
      ys[i + 1] = c.y + cosf(a) * 1500;
    }
    if (c.view == 2) blendPolygonF(xs, ys, N + 2, 255, 40, 30, 0.30f);
    else if (c.view == 1) blendPolygonF(xs, ys, N + 2, 255, 170, 40, 0.24f);
    else blendPolygonF(xs, ys, N + 2, 255, 240, 170, 0.15f);
    uint16_t edge = c.view == 2 ? rgb565(255, 90, 70) : (c.view == 1 ? rgb565(255, 190, 90) : rgb565(170, 160, 110));
    drawLineF(xs[0], ys[0], xs[1], ys[1], edge, 1.0f);
    drawLineF(xs[0], ys[0], xs[N + 1], ys[N + 1], edge, 1.0f);
    // the camera, pointing along its beam
    float a = c.angle * 0.017453293f, ux = sinf(a), uy = cosf(a), vx = uy, vy = -ux;
    const float body[6][2] = { { -10, -11 }, { 20, -10 }, { 27, -7 }, { 27, 7 }, { 20, 10 }, { -10, 11 } };
    float bx[6], by[6];
    for (int i = 0; i < 6; i++) {
      bx[i] = c.x + ux * body[i][0] + vx * body[i][1];
      by[i] = c.y + uy * body[i][0] + vy * body[i][1];
    }
    fillPolygonF(bx, by, 6, rgb565(190, 196, 206));
    drawLineF(bx[0], by[0], bx[1], by[1], rgb565(230, 234, 240), 1.5f);
    fillCircle((int)lroundf(c.x + ux * 27), (int)lroundf(c.y + uy * 27), 6, rgb565(20, 24, 30));
    fillCircle((int)lroundf(c.x + ux * 27), (int)lroundf(c.y + uy * 27), 2, rgb565(90, 130, 200));
    bool led = c.view != 0 || ((int)(clockT * 2) & 1);
    fillCircle((int)lroundf(c.x - ux * 3 + vx * 6), (int)lroundf(c.y - uy * 3 + vy * 6), 2, led ? rgb565(255, 40, 30) : rgb565(80, 20, 16));
  }
}

// The drone: a body, two rotors, its status light, its gun turned on him; and its bullets
static void drawDrone() {
  const float S = 1.45f;                 // (its size: the shapes below are at 1)
  float x = drX, y = drY + sinf(clockT * 3.1f) * 3;
  uint16_t dark = rgb565(46, 50, 58), body = rgb565(76, 82, 94), hi = rgb565(140, 146, 158);
  for (int s = -1; s <= 1; s += 2) {   // arms and rotors
    float rx = x + s * 34 * S, ry = y - 12 * S;
    drawLineF(x + s * 12 * S, y - 4 * S, rx, ry, dark, 4 * S);
    fillCircle((int)rx, (int)(ry - 3 * S), (int)(3 * S), dark);
    for (int k = 0; k < 2; k++) {        // the blades: a blur that flickers in width
      float w = S * (6 + 14 * fabsf(cosf(clockT * 47.0f + k * 1.7f + s)));
      drawLineF(rx - w, ry - (5 + k) * S, rx + w, ry - (5 + k) * S, k ? rgb565(170, 176, 186) : rgb565(120, 126, 138), 1.6f);
    }
  }
  const float px[8] = { -22, -14, 14, 22, 18, 8, -8, -18 }, py[8] = { -2, -10, -10, -2, 7, 11, 11, 7 };
  float xs[8], ys[8];
  for (int i = 0; i < 8; i++) { xs[i] = x + px[i] * S; ys[i] = y + py[i] * S; }
  fillPolygonF(xs, ys, 8, body);
  drawLineF(x - 13 * S, y - 9 * S, x + 13 * S, y - 9 * S, hi, 2.0f);
  fillRect((int)(x - 17 * S), (int)(y + 2 * S), (int)(x + 17 * S), (int)(y + 3 * S), dark);
  // the gun, turned towards him
  float ax = agX - x, ay = feetY() - 65 * myScale - (y + 12 * S), al = fmaxf(1.0f, sqrtf(ax * ax + ay * ay));
  float gx = x + ax / al * 18 * S, gy = y + 12 * S + ay / al * 18 * S;
  fillCircle((int)x, (int)(y + 10 * S), (int)(5 * S), dark);
  drawLineF(x, y + 12 * S, gx, gy, dark, 3.5f * S);
  if (drFlashT > 0) {
    fillCircle((int)gx, (int)gy, 6, rgb565(255, 230, 120));
    glowDisc(gx, gy, 30, 255, 200, 80, 0.6f);
  }
  // the status light: green out of range, yellow getting close, red firing (blinking)
  int r = 40, g = 230, b = 90;
  if (drLight == 1) { r = 255; g = 210; b = 40; }
  if (drLight == 2) { r = 255; g = 40; b = 30; }
  bool on = drLight < 2 || fmodf(clockT, 0.2f) < 0.13f;
  fillCircle((int)x, (int)(y - 2), 7, on ? rgb565(r, g, b) : rgb565(r / 3, g / 3, b / 3));
  fillCircle((int)x - 2, (int)(y - 4), 2, rgb565(255, 255, 255));
  if (on) glowDisc(x, y - 2, 50, r, g, b, 0.5f);
  // the bullets: short bright tracers
  for (int i = 0; i < MAX_BULLETS; i++) {
    const Bullet &bl = bullets[i];
    if (bl.life <= 0) continue;
    drawLineF(bl.x - bl.vx * 0.03f, bl.y - bl.vy * 0.03f, bl.x, bl.y, rgb565(255, 220, 120), 2.0f);
    fillCircle((int)bl.x, (int)bl.y, 1, rgb565(255, 255, 220));
  }
  if (hitFlashT > 0) glowDisc(hitX, hitY, 40, 255, 60, 30, hitFlashT * 4);
}

// The opening: the grille rattles and falls, he lowers himself out of the
// duct and drops onto the catwalk (X skips it)
static void updateIntro(float dt) {
  float before = introT;
  introT += dt;
  float t = introT;
  if (pressed & BTN_JUMP) t = introT = 2.0f;
  int cy = CAR_FLOOR_Y - CORR_H;
  agX = (float)ventX;
  agFacing = ventX > SHAFT_X1 ? -1 : 1;   // (facing the lift)
  AgentPose &p = shownPose;
  memset(&p, 0, sizeof(p));
  if ((before < 0.15f && t >= 0.15f) || (before < 0.5f && t >= 0.5f)) sfxPlay(SFX_CLUNK);
  if (before < 1.2f && t >= 1.2f) sfxPlay(SFX_LAND);   // the grille hits the catwalk
  if (t < 1.0f) {                      // still in the duct
    agY = cy - 20;
    p.armF = p.armN = 175;
  } else if (t < 1.7f) {               // lowering himself out, hanging from the edge
    float k = (t - 1.0f) / 0.7f;
    agY = cy + 20 + 128 * k;
    p.armF = 176; p.elbowF = 4;
    p.armN = 172; p.elbowN = 8;
    p.legN = 6 * sinf(t * 5); p.legF = -4;
  } else if (t < 1.95f) {              // let go
    float k = (t - 1.7f) / 0.25f;
    agY = cy + 148 + (CAR_FLOOR_Y - cy - 148) * k * k;
    p.armF = 150; p.armN = 140; p.elbowN = 20;
  } else {                             // on the catwalk: over to the player
    agY = CAR_FLOOR_Y;
    introT = -1;
    ventOpen = true;
    placeAgent(agX, CAR_FLOOR_Y, agFacing);
    agentSample(ANIM_JUMP, AG_JUMP_DOWN, shownPose);
    setState(ST_LAND, 0.0f);
    sfxPlay(SFX_LAND);
    showMsg("OPTIONS: THE MAP", 5.0f);
    return;
  }
  p.plant = 0;
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
    if (d.kind != DEC_FIRSTAID && ri.searched[i]) {
      uint16_t c = ri.holds[i] ? rgb565(255, 210, 70) : rgb565(80, 230, 120);   // (gold: something was in it)
      int tx = d.x - 6, ty = top - 12;
      drawLine(tx, ty, tx + 4, ty + 5, c, true);
      drawLine(tx + 4, ty + 5, tx + 13, ty - 6, c, true);
    }
  }
  // the search bar: over the piece being searched, and dimmer over any half searched and left
  for (int i = 0; i < decorCount; i++) {
    if (pieceDone(i)) continue;
    float prog = pieceProgress(i);
    bool now = (agState == ST_USE && searchItem == i);
    if (!now && prog <= 0) continue;
    const Decor &d = decor[i];
    int x0 = d.x - 36, y0 = d.y - decorH[d.kind] - 30;
    drawRectOutline(x0, y0, x0 + 72, y0 + 9, now ? rgb565(220, 220, 230) : rgb565(110, 116, 130), 1);
    int w = (int)(68 * fminf(prog, SEARCH_SECONDS) / SEARCH_SECONDS);
    if (w > 0) fillRect(x0 + 2, y0 + 2, x0 + 1 + w, y0 + 7, now ? rgb565(120, 200, 255) : rgb565(70, 110, 150));
  }
  if (nothingShowT > 0)
    drawVectorText(floatText, nothingX, fmaxf(nothingY, 112.0f) - (NOTHING_SHOW - nothingShowT) * 20, 2.5f, floatColour, false);
}

static void drawScorePops() {
  for (int i = 0; i < MAX_POPS; i++) {
    if (popT[i] <= 0) continue;
    char b[16];
    snprintf(b, sizeof(b), "+%ld", popV[i]);
    float y = popY[i] - (1.2f - popT[i]) * 40;
    if (y < HUD_H + 14) y = HUD_H + 14;
    float f = popT[i] < 0.3f ? popT[i] / 0.3f : 1.0f;
    drawVectorText(b, popX[i], y, 2.6f, dimmed(255, 230, 90, f), true);
  }
}

// FOUND OBJECT, as NUCLEAR MISSION's panel: a box with the find in it
static void drawItemPanel() {
  int x0 = 430, y0 = 120, x1 = 850, y1 = 310;
  fillRect(x0, y0, x1, y1, rgb565(10, 12, 20));
  drawRectOutline(x0, y0, x1, y1, rgb565(255, 210, 70), 3);
  drawVectorText("FOUND OBJECT", SCREEN_W / 2, y0 + 30, 3.5f, rgb565(255, 210, 70), true);
  // the object: a sealed case, for now
  int cx = SCREEN_W / 2, cy = y0 + 96;
  fillRect(cx - 44, cy - 26, cx + 44, cy + 26, rgb565(90, 96, 108));
  fillRect(cx - 44, cy - 26, cx + 44, cy - 22, rgb565(150, 156, 168));
  drawRectOutline(cx - 44, cy - 26, cx + 44, cy + 26, rgb565(40, 44, 52), 2);
  fillRect(cx - 14, cy - 36, cx + 14, cy - 27, rgb565(40, 44, 52));           // the handle
  fillRect(cx - 9, cy - 32, cx + 9, cy - 27, rgb565(10, 12, 20));
  fillRect(cx - 34, cy - 6, cx - 26, cy + 4, rgb565(200, 170, 80));           // the catches
  fillRect(cx + 26, cy - 6, cx + 34, cy + 4, rgb565(200, 170, 80));
  drawVectorText("?", cx, cy + 4, 3.4f, rgb565(255, 210, 70), true);
  char b[32];
  snprintf(b, sizeof(b), "+%d   %d OF %d FOUND", FOUND_SCORE, itemsFound, ITEMS_HIDDEN);
  drawVectorText(b, SCREEN_W / 2, y1 - 28, 2.6f, rgb565(100, 240, 140), true);
}

// The top bar: the score on the left, the clock in the middle, health on the right
static void drawHud() {
  gfxClipAll();
  fillRect(0, 0, SCREEN_W - 1, HUD_H - 1, rgb565(10, 12, 16));
  fillRect(0, HUD_H - 2, SCREEN_W - 1, HUD_H - 1, rgb565(44, 50, 62));
  char b[64];
  snprintf(b, sizeof(b), "SCORE %06ld", score);
  float w = vectorTextWidth(b, 2.6f);
  drawVectorText(b, 24 + w / 2, HUD_H / 2, 2.6f, rgb565(240, 240, 245), true);
  // the clock (red in the last minute, flashing in the last ten seconds)
  int secs = (int)ceilf(timeLeft > 0 ? timeLeft : 0);
  snprintf(b, sizeof(b), "%d:%02d", secs / 60, secs % 60);
  bool lastMin = secs <= 60, flash = secs <= 10 && fmodf(clockT, 0.5f) < 0.25f;
  drawVectorText(b, SCREEN_W / 2, HUD_H / 2, 3.2f, flash ? rgb565(120, 30, 24) : (lastMin ? rgb565(255, 70, 50) : rgb565(255, 190, 70)), true);
  // the health bar
  int hx1 = SCREEN_W - 26, hx0 = hx1 - 200, hy0 = HUD_H / 2 - 8, hy1 = HUD_H / 2 + 8;
  drawVectorText("HEALTH", hx0 - 50, HUD_H / 2, 2.2f, rgb565(170, 178, 196), false);
  drawRectOutline(hx0 - 2, hy0 - 2, hx1 + 2, hy1 + 2, rgb565(90, 100, 120), 1);
  fillRect(hx0, hy0, hx1, hy1, rgb565(30, 20, 20));
  int fill = (hx1 - hx0) * health / HEALTH_MAX;
  uint16_t hc = health > 25 ? rgb565(80, 220, 110) : (health > 10 ? rgb565(240, 190, 60) : rgb565(240, 60, 44));
  if (hurtFlashT > 0 && fmodf(hurtFlashT, 0.2f) < 0.1f) hc = rgb565(255, 255, 255);
  if (fill > 0) fillRect(hx0, hy0, hx0 + fill - 1, hy1, hc);
  for (int k = 1; k < 5; k++) fillRect(hx0 + (hx1 - hx0) * k / 5, hy0, hx0 + (hx1 - hx0) * k / 5, hy1, rgb565(20, 22, 28));   // a mark every 10
}

// ---- the lift area's panel, as Impossible Mission's: where you are on the
// left, the mini map in the middle (under the lift), the details on the right
static void bevelBox(int x0, int y0, int x1, int y1, uint16_t face) {
  fillRect(x0, y0, x1, y1, face);
  fillRect(x0, y0, x1, y0 + 2, rgb565(170, 184, 210));
  fillRect(x0, y0, x0 + 2, y1, rgb565(170, 184, 210));
  fillRect(x0, y1 - 2, x1, y1, rgb565(30, 34, 48));
  fillRect(x1 - 2, y0, x1, y1, rgb565(30, 34, 48));
}

static void drawArrowKey(int cx, int cy, bool up, bool lit) {
  bevelBox(cx - 34, cy - 26, cx + 34, cy + 26, rgb565(84, 88, 160));
  int d = up ? -1 : 1;
  int xs[7] = { cx, cx + 20, cx + 9, cx + 9, cx - 9, cx - 9, cx - 20 };
  int ys[7] = { cy + d * 16, cy, cy, cy - d * 14, cy - d * 14, cy, cy };
  fillPolygon(xs, ys, 7, lit ? rgb565(255, 220, 80) : rgb565(150, 130, 60));
}

static void drawMiniMap(int x0, int y0, int x1, int y1) {
  // the screen: a black bezel, a striped frame, yellow inside
  fillRect(x0, y0, x1, y1, rgb565(10, 10, 12));
  int fx0 = x0 + 10, fy0 = y0 + 10, fx1 = x1 - 10, fy1 = y1 - 10;
  fillRect(fx0, fy0, fx1, fy1, rgb565(210, 40, 32));             // the frame: red and black stripes
  for (int x = fx0 - 40; x < fx1 + 40; x += 16) {   // the black stripes, slanting
    for (int side = 0; side < 4; side++) {
      // top and bottom bands
      if (side < 2) {
        int yb0 = side ? fy1 - 7 : fy0, yb1 = side ? fy1 : fy0 + 7;
        int xs[4] = { x, x + 8, x + 8 + (yb1 - yb0), x + (yb1 - yb0) };
        int ys[4] = { yb0, yb0, yb1 + 1, yb1 + 1 };
        for (int k = 0; k < 4; k++) xs[k] = clampv(xs[k], fx0, fx1 + 1);
        fillPolygon(xs, ys, 4, rgb565(16, 16, 18));
      }
    }
  }
  for (int y = fy0 - 40; y < fy1 + 40; y += 16)
    for (int side = 0; side < 2; side++) {   // left and right bands
      int xb0 = side ? fx1 - 7 : fx0, xb1 = side ? fx1 : fx0 + 7;
      int xs[4] = { xb0, xb1 + 1, xb1 + 1, xb0 };
      int ys[4] = { y, y + (xb1 - xb0), y + 8 + (xb1 - xb0), y + 8 };
      for (int k = 0; k < 4; k++) ys[k] = clampv(ys[k], fy0, fy1 + 1);
      fillPolygon(xs, ys, 4, rgb565(16, 16, 18));
    }
  int gx0 = fx0 + 8, gy0 = fy0 + 8, gx1 = fx1 - 8, gy1 = fy1 - 8;
  fillRect(gx0, gy0, gx1, gy1, rgb565(236, 206, 64));            // the screen: yellow
  // the complex, scaled into the screen: the rooms you've been in, the shafts as far as you've seen them
  const int RW = 36, SW = 12, RH = 18, GAP = 7;
  int mw = COLS * RW + SHAFTS * SW, mh = ROWS * RH + (ROWS - 1) * GAP;
  int left = (gx0 + gx1 - mw) / 2, top = (gy0 + gy1 - mh) / 2;
  uint16_t ink = rgb565(40, 30, 10);
  auto floorY = [&](int f) { int r = f / 2; return top + r * (RH + GAP) + ((f & 1) ? RH - 4 : 4); };
  for (int sh = 0; sh < SHAFTS; sh++) {
    int lo = -1, hi = -1;
    for (int f = 0; f < FLOORS; f++)
      if (shaftSeen[sh][f]) { if (lo < 0) lo = f; hi = f; }
    if (lo < 0) continue;
    int x = left + sh * (RW + SW) + SW / 2;
    fillRect(x - 1, floorY(lo) - 2, x + 1, floorY(hi) + 2, ink);
  }
  for (int i = 0; i < ROOM_COUNT; i++) {
    const RoomInfo &ri = rooms[i];
    if (!ri.visited) continue;
    int rx = left + SW + ri.col * (RW + SW), ry = top + ri.row * (RH + GAP);
    fillRect(rx + 2, ry, rx + RW - 3, ry + RH - 1, ink);
    int yl = floorY(ri.row * 2 + (ri.topL ? 0 : 1)), yr = floorY(ri.row * 2 + (ri.topR ? 0 : 1));
    fillRect(rx - SW / 2, yl - 1, rx + 2, yl + 1, ink);                // the corridors to the shafts
    fillRect(rx + RW - 3, yr - 1, rx + RW + SW / 2, yr + 1, ink);
  }
  if (fmodf(clockT, 0.6f) < 0.4f) {           // him
    int x = left + curShaft * (RW + SW) + SW / 2;
    float f = carWorldY / LEVEL_GAP;
    int f0 = (int)f;
    float k = f - f0;
    float y = floorY(f0) + (f0 + 1 < FLOORS ? (floorY(f0 + 1) - floorY(f0)) * k : 0);
    fillRect(x - 3, (int)y - 3, x + 3, (int)y + 3, rgb565(250, 250, 240));
    drawRectOutline(x - 4, (int)y - 4, x + 4, (int)y + 4, ink, 1);
  }
}

static void drawPanel() {
  gfxClipAll();
  uint16_t face = rgb565(120, 164, 196);
  fillRect(0, PANEL_Y, SCREEN_W - 1, SCREEN_H - 1, rgb565(30, 34, 48));
  bevelBox(4, PANEL_Y + 4, SCREEN_W - 5, SCREEN_H - 5, face);
  const int mx0 = 390, mx1 = 890;              // the mini map: under the lift, in the middle
  drawMiniMap(mx0, PANEL_Y + 16, mx1, SCREEN_H - 18);
  // on the left: where you are, and the car's arrows
  bevelBox(20, PANEL_Y + 16, 372, SCREEN_H - 18, rgb565(70, 78, 120));
  char b[40];
  snprintf(b, sizeof(b), "LIFT %c", shaftLetter(curShaft));
  drawVectorText(b, 150, PANEL_Y + 62, 4.2f, rgb565(255, 220, 80), true);
  snprintf(b, sizeof(b), "FLOOR %d", carFloor + 1);
  drawVectorText(b, 150, PANEL_Y + 112, 3.4f, rgb565(230, 236, 245), true);
  bool movingUp = carMoving && carFloor * LEVEL_GAP < carWorldY, movingDown = carMoving && !movingUp;
  drawArrowKey(310, PANEL_Y + 64, true, movingUp);
  drawArrowKey(310, PANEL_Y + 140, false, movingDown);
  drawVectorText(carMoving ? "MOVING" : "STOPPED", 150, PANEL_Y + 160, 2.4f, carMoving ? rgb565(255, 190, 70) : rgb565(120, 230, 140), false);
  // on the right: the details
  bevelBox(908, PANEL_Y + 16, SCREEN_W - 20, SCREEN_H - 18, rgb565(70, 78, 120));
  int seen = 0, kitsLeft = 0;
  for (int i = 0; i < ROOM_COUNT; i++) {
    seen += rooms[i].visited;
    kitsLeft += rooms[i].kit && !rooms[i].kitUsed;
  }
  int lx = 932;
  auto line = [&](const char *label, const char *value, int y, uint16_t col) {
    drawVectorText(label, lx + vectorTextWidth(label, 2.4f) / 2, y, 2.4f, rgb565(190, 200, 220), false);
    drawVectorText(value, SCREEN_W - 44 - vectorTextWidth(value, 2.8f) / 2, y, 2.8f, col, true);
  };
  snprintf(b, sizeof(b), "%d/%d", seen, ROOM_COUNT);
  line("ROOMS", b, PANEL_Y + 46, rgb565(240, 240, 245));
  snprintf(b, sizeof(b), "%d/%d", itemsFound, ITEMS_HIDDEN);
  line("FOUND", b, PANEL_Y + 86, rgb565(255, 220, 80));
  snprintf(b, sizeof(b), "%d", kitsLeft);
  line("FIRST AID LEFT", b, PANEL_Y + 126, rgb565(120, 230, 140));
  snprintf(b, sizeof(b), "V%s  FPS %d", GAME_VERSION, (int)lroundf(fpsShown));
  drawVectorText(b, (lx + SCREEN_W - 44) / 2, PANEL_Y + 170, 1.8f, rgb565(150, 160, 186), false);
}

// GAME OVER: why, and the score
static void drawGameOver() {
  float k = gameOverT / 1.0f;
  if (k > 1) k = 1;
  blendRect(0, HUD_H, SCREEN_W - 1, SCREEN_H - 1, 0, 0, 0, 0.65f * k);
  if (gameOverT < 0.4f) return;
  drawVectorTextW("GAME OVER", SCREEN_W / 2, 250, 10.0f, rgb565(120, 20, 14), 8.0f);
  drawVectorTextW("GAME OVER", SCREEN_W / 2, 250, 10.0f, rgb565(255, 70, 50), 3.0f);
  drawVectorText(gameOver == END_TIME ? "THE TIME RAN OUT" : "OUT OF HEALTH", SCREEN_W / 2, 350, 4.0f, rgb565(240, 240, 245), true);
  char b[48];
  snprintf(b, sizeof(b), "SCORE %ld", score);
  drawVectorText(b, SCREEN_W / 2, 420, 4.0f, rgb565(255, 190, 70), true);
  int seen = 0;
  for (int i = 0; i < ROOM_COUNT; i++) seen += rooms[i].visited;
  snprintf(b, sizeof(b), "%d OF %d FOUND   %d OF %d ROOMS", itemsFound, ITEMS_HIDDEN, seen, ROOM_COUNT);
  drawVectorText(b, SCREEN_W / 2, 470, 2.6f, rgb565(170, 178, 196), false);
  if (gameOverT > 1.5f && fmodf(clockT, 1.0f) < 0.65f) drawVectorText("PRESS X", SCREEN_W / 2, 560, 3.6f, rgb565(240, 240, 245), true);
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
    drawVectorText(b, x0 + RW / 2, y0 + RH / 2 - (ri.visited ? 6 : 0), 2.4f, ri.visited ? rgb565(230, 236, 245) : rgb565(110, 118, 136), true);
    if (ri.visited)   // its equipment: grey not searched, green nothing, gold something found
      for (int k = 0; k < ri.decorN; k++) {
        int dx = x0 + RW / 2 - (ri.decorN - 1) * 7 + k * 14;
        uint16_t c = !ri.searched[k] ? rgb565(80, 88, 104) : (ri.holds[k] ? rgb565(255, 210, 70) : rgb565(80, 230, 120));
        fillRect(dx - 4, y0 + RH - 15, dx + 4, y0 + RH - 8, c);
      }
    if (ri.visited && ri.kit && !ri.kitUsed) {   // a first aid kit still here: a red cross in the corner
      fillRect(x0 + RW - 20, y0 + 6, x0 + RW - 6, y0 + 20, rgb565(236, 236, 240));
      fillRect(x0 + RW - 15, y0 + 8, x0 + RW - 11, y0 + 18, rgb565(210, 36, 30));
      fillRect(x0 + RW - 18, y0 + 11, x0 + RW - 8, y0 + 15, rgb565(210, 36, 30));
    }
    if (ri.visited && ri.hazard == HZ_ELECTRIC) {   // known hazards: a lightning bolt, or an eye for a dark room
      int bx = x0 + 12, by = y0 + 13;
      int xs[6] = { bx + 3, bx - 5, bx, bx - 3, bx + 6, bx + 1 }, ys[6] = { by - 9, by + 2, by + 2, by + 10, by - 2, by - 2 };
      fillPolygon(xs, ys, 6, rgb565(255, 220, 60));
    } else if (ri.visited && (ri.hazard == HZ_CAMERA || ri.hazard == HZ_CAMERA2)) {   // a camera (two)
      for (int k = 0; k < (ri.hazard == HZ_CAMERA2 ? 2 : 1); k++) {
        int cx = x0 + 8 + k * 16, cy = y0 + 9;
        fillRect(cx, cy, cx + 11, cy + 7, rgb565(190, 196, 206));
        fillRect(cx + 11, cy + 2, cx + 13, cy + 5, rgb565(190, 196, 206));
        fillRect(cx + 2, cy + 2, cx + 3, cy + 3, rgb565(255, 40, 30));
      }
    } else if (ri.visited && ri.hazard == HZ_DRONE) {   // a drone
      int cx = x0 + 15, cy = y0 + 13;
      fillRect(cx - 10, cy - 6, cx - 3, cy - 5, rgb565(170, 176, 186));
      fillRect(cx + 3, cy - 6, cx + 10, cy - 5, rgb565(170, 176, 186));
      fillRect(cx - 6, cy - 3, cx + 6, cy + 3, rgb565(110, 116, 128));
      fillCircle(cx, cy, 1, rgb565(255, 40, 30));
    } else if (ri.visited && ri.hazard == HZ_DARK) {
      int ex = x0 + 14, ey = y0 + 13;
      fillCircle(ex, ey, 7, rgb565(60, 200, 100));
      fillCircle(ex, ey, 3, rgb565(10, 12, 16));
    }
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
// ============================================================ the title
static bool onTitle = true;

static inline float pulseOf(float t) { return 0.5f + 0.5f * sinf(t); }

// The story on the title screen: a classified transmission, typed out line by
// line, now and then glitching as reality wavers; the last line in red
static float titleT = 0;                 // seconds on the title screen
static const char *const titleStory[] = {
  "A TOP-SECRET EXPERIMENT DESIGNED TO MANIPULATE",
  "TIME AND MATTER HAS GONE CATASTROPHICALLY WRONG.",
  "THE FACILITY'S DIRECTOR, PROFESSOR ADRIAN VOSS,",
  "HAS TAKEN CONTROL OF THE INSTALLATION AND",
  "ACTIVATED A DEVICE KNOWN AS THE QUANTUM CORE.",
  "",
  "THE MACHINE IS NOW DESTABILISING REALITY ITSELF.",
};
#define STORY_LINES (int)(sizeof(titleStory) / sizeof(titleStory[0]))
#define STORY_CHARS_PER_SECOND 38.0f

static void drawTitleStory() {
  const int x0 = 196, x1 = 1084, y0 = 250, y1 = 572;
  blendRect(x0, y0, x1, y1, 4, 10, 16, 0.78f);
  drawRectOutline(x0, y0, x1, y1, rgb565(60, 170, 190), 2);
  fillRect(x0 + 2, y0 + 2, x1 - 2, y0 + 30, rgb565(70, 16, 14));
  bool blinkHead = fmodf(titleT, 1.2f) < 0.8f;
  drawVectorText("CLASSIFIED TRANSMISSION", x0 + 22 + vectorTextWidth("CLASSIFIED TRANSMISSION", 2.2f) / 2, y0 + 16, 2.2f,
                 blinkHead ? rgb565(255, 90, 70) : rgb565(150, 50, 40), true);
  drawVectorText("FACILITY Q-7  //  PRIORITY ONE", x1 - 22 - vectorTextWidth("FACILITY Q-7  //  PRIORITY ONE", 1.8f) / 2, y0 + 16, 1.8f,
                 rgb565(220, 150, 140), false);
  // the text, typed out
  float typed = (titleT - 0.6f) * STORY_CHARS_PER_SECOND;
  int lx = x0 + 34, curLine = -1, curCol = 0;
  // now and then a glitch: one line jumps sideways in two colours
  float g = fmodf(titleT, 2.9f);
  int glitchLine = g < 0.14f ? ((int)(titleT / 2.9f) * 5 + 3) % STORY_LINES : -1;
  for (int l = 0; l < STORY_LINES; l++) {
    int len = (int)strlen(titleStory[l]);
    int show = typed <= 0 ? 0 : (typed >= len ? len : (int)typed);
    typed -= len + 6;                          // (a little pause at each line's end)
    if (show <= 0) continue;
    char part[64];
    snprintf(part, sizeof(part), "%.*s", show, titleStory[l]);
    float y = y0 + 64 + l * 38;
    float w = vectorTextWidth(part, 2.5f);
    bool last = (l == STORY_LINES - 1);
    uint16_t col = last ? rgb565(255, 70 + (int)(40 * pulseOf(titleT * 3.0f)), 50) : rgb565(170, 236, 246);
    if (l == glitchLine) {
      float jx = 8.0f * sinf(titleT * 90.0f);
      drawVectorText(part, lx + w / 2 + jx - 3, y, 2.5f, rgb565(255, 40, 60), true);
      drawVectorText(part, lx + w / 2 - jx + 3, y, 2.5f, rgb565(40, 220, 255), true);
    } else {
      drawVectorText(part, lx + w / 2, y, 2.5f, col, true);
    }
    if (show < len) {
      curLine = l;
      curCol = show;
    }
  }
  // the cursor, after the last letter typed (or under the text once it is all out)
  if (fmodf(titleT, 0.6f) < 0.35f) {
    float cx, cy;
    if (curLine >= 0) {
      char part[64];
      snprintf(part, sizeof(part), "%.*s", curCol, titleStory[curLine]);
      cx = lx + vectorTextWidth(part, 2.5f) + 8;
      cy = y0 + 64 + curLine * 38;
    } else if (typed > 0) {             // all out: it waits at the end of the last line
      cx = lx + vectorTextWidth(titleStory[STORY_LINES - 1], 2.5f) + 8;
      cy = y0 + 64 + (STORY_LINES - 1) * 38;
    } else {
      cx = -100;
      cy = 0;
    }
    if (cx > 0) fillRect((int)cx, (int)cy - 8, (int)cx + 10, (int)cy + 8, rgb565(170, 236, 246));
  }
  // scan lines over it all
  for (int y = y0 + 34; y < y1 - 2; y += 4) blendRect(x0 + 2, y, x1 - 2, y, 0, 0, 0, 0.18f);
}

static void drawTitle() {
  gfxTarget = g_frame;
  gfxClipAll();
  // the dark of the shaft, lighter in the middle, girders standing in it
  for (int y = 0; y < SCREEN_H; y += 4) {
    float k = 1.0f - fabsf((float)(y - 300)) / 420.0f;
    if (k < 0) k = 0;
    fillRect(0, y, SCREEN_W - 1, y + 3, rgb565(8 + (int)(16 * k), 10 + (int)(20 * k), 16 + (int)(28 * k)));
  }
  float off = fmodf(clockT * 40.0f, 60.0f);    // rungs drifting down: a lift on its way
  static const int girders[4] = { 150, 380, 900, 1130 };
  for (int x : girders) {
    fillRect(x, 0, x + 15, SCREEN_H - 1, rgb565(26, 32, 44));
    fillRect(x, 0, x + 2, SCREEN_H - 1, rgb565(44, 52, 66));
  }
  for (int k = -1; k < 13; k++) {
    int y = (int)(k * 60 + off);
    drawLine(560, y, 720, y, rgb565(24, 28, 38), false);
  }
  // the name, in big vector letters with a glow behind
  const char *w1 = "QUANTUM", *w2 = "LOCKDOWN";
  float pulse = 0.5f + 0.5f * sinf(clockT * 2.0f);
  uint16_t glow = rgb565(60 + (int)(40 * pulse), 34, 12), main = rgb565(255, 190, 70);
  drawVectorTextW(w1, SCREEN_W / 2, 84, 11.0f, glow, 9.0f);
  drawVectorTextW(w2, SCREEN_W / 2, 186, 11.0f, glow, 9.0f);
  drawVectorTextW(w1, SCREEN_W / 2, 84, 11.0f, main, 3.4f);
  drawVectorTextW(w2, SCREEN_W / 2, 186, 11.0f, main, 3.4f);
  drawTitleStory();
  if (fmodf(clockT, 1.0f) < 0.65f) drawVectorText("PRESS X TO START", SCREEN_W / 2, 620, 4.0f, rgb565(240, 240, 245), true);
  if (tuneProblem[0][0]) drawVectorText(tuneProblem[0], SCREEN_W / 2, 656, 1.8f, rgb565(220, 120, 90), false);
  if (tuneProblem[1][0]) drawVectorText(tuneProblem[1], SCREEN_W / 2, 674, 1.8f, rgb565(220, 120, 90), false);
  char b[24];
  snprintf(b, sizeof(b), "VERSION %s", GAME_VERSION);
  drawVectorText(b, SCREEN_W / 2, 698, 1.8f, rgb565(100, 110, 130), false);
}

// A new game: a new complex, him in the car of shaft A at its top stop
static void newGame() {
  layOut(layoutSeed * 7919u + 13u + (uint32_t)(clockT * 1000.0f));
  enterShaft(START_SHAFT, topStop(START_SHAFT), true);
  // the opening: the duct is over the catwalk between the lift and the door to the first room
  ventShaft = START_SHAFT;
  ventFloor = carFloor;
  bool right = roomOff(START_SHAFT, carFloor, true) >= 0;
  ventX = right ? (SHAFT_X1 + SCREEN_W - 40) / 2 : (40 + SHAFT_X0) / 2;
  ventOpen = false;
  introT = 0;
  placeAgent((float)ventX, CAR_FLOOR_Y, right ? -1 : 1);
  mapOn = false;
  onTitle = false;
  score = 0;
  itemsFound = 0;
  health = HEALTH_MAX;
  hurtFlashT = 0;
  timeLeft = GAME_SECONDS;
  gameOver = END_NONE;
  itemShowT = nothingShowT = 0;
  for (int i = 0; i < MAX_POPS; i++) popT[i] = 0;
  msgT = 0;
}

void gameInit(uint32_t seed) {
  gfxInit();
  layoutSeed = seed;
  paintShaftPicture();
  onTitle = true;
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

  if (onTitle) {
    titleT += dt;
    if (pressed & BTN_JUMP) {
      newGame();
      pressed = 0;                     // (that X starts the game; it isn't a somersault)
    } else {
      drawTitle();
      return;
    }
  }

  if (gameOver != END_NONE) {           // the game is over: X (after a moment) back to the title
    gameOverT += dt;
    if (gameOverT > 1.5f && (pressed & BTN_JUMP)) {
      onTitle = true;
      titleT = 0;
      tuneWanted.store(0, std::memory_order_relaxed);
      drawTitle();
      return;
    }
    held = pressed = 0;
    mapOn = false;
  }

  if (introT >= 0) {                     // the opening: no map, no clock, no control
    updateIntro(dt);
    pressed = 0;
  }

  if (pressed & BTN_MAP) mapOn = !mapOn;   // (the map pauses everything, the clock too)
  if (mapOn) {
    if (pressed & BTN_NEW) newGame();
    pressed = 0;
  } else {
    if (gameOver == END_NONE && introT < 0) {
      timeLeft -= dt;
      if (timeLeft <= 0) {
        timeLeft = 0;
        endGame(END_TIME);
      }
    }
    if (hurtFlashT > 0) hurtFlashT -= dt;
    if (msgT > 0) msgT -= dt;
    if (nothingShowT > 0) nothingShowT -= dt;
    if (itemShowT > 0) itemShowT -= dt;
    for (int i = 0; i < MAX_POPS; i++)
      if (popT[i] > 0) popT[i] -= dt;
    updateLifts(dt);
    if (introT < 0) updateAgent(dt);
    if (introT < 0) updateHazards(dt);
  }

  // the picture
  gfxTarget = g_frame;
  if (mode == MODE_ROOM) {
    memcpy(g_frame, roomBg, sizeof(roomBg));
    gfxClip(HUD_H, PLAY_Y1);
    drawOrnamentsAnim();
    for (int i = 0; i < liftCount; i++) drawLiftPad(lifts[i]);
    drawDecorAnim();
    if (curHazard == HZ_ELECTRIC && elecState == 1 && flickerDim(elecT)) dimRoom();
    if (curHazard == HZ_DARK && !lightsOut && ((darkT > 0.25f && darkT < 0.35f) || (darkT > 0.5f && darkT < 0.56f))) dimRoom();
    if (curHazard == HZ_ELECTRIC && elecState != 0) drawFloorArcs(elecState == 2);
    if (!lightsOut) drawSearchMarks();
  } else {
    memcpy(g_frame, shaftBg, sizeof(shaftBg));
    gfxClip(HUD_H, PANEL_Y - 1);
    drawShaftScene();
  }
  gfxClip(introT >= 0 && introT < 1.9f ? CAR_FLOOR_Y - CORR_H + 1 : HUD_H, mode == MODE_SHAFT ? PANEL_Y - 1 : PLAY_Y1);   // (in the duct: hidden)
  drawAgentU(shownPose, agX, agY, agFacing, myScale);
  gfxClip(HUD_H, PLAY_Y1);
  if (mode == MODE_ROOM && lightsOut) {   // dark: the goggles' view, then what glows, the search bars over it
    nightVision();
    drawDarkGlows();
    drawSearchMarks();
  }
  if (mode == MODE_ROOM && agState == ST_SHOCK) drawShockSparks();
  if (mode == MODE_ROOM && camCount > 0) drawCameras();
  if (mode == MODE_ROOM && curHazard == HZ_DRONE) drawDrone();
  if (mode == MODE_SHAFT) drawPanel();
  if (mode == MODE_ROOM) drawScorePops();
  if (itemShowT > 0 && mode == MODE_ROOM && !mapOn) drawItemPanel();
  if (mapOn) drawMap();
  else if (itemShowT <= 0) drawMessage();
  if (gameOver != END_NONE) drawGameOver();
  // the music: the lift's tune in the car, stage 1's everywhere else; the lift's motor
  tuneWanted.store(gameOver != END_NONE ? 0 : (mode == MODE_SHAFT && inCar(agX) && agState != ST_JUMP ? 1 : 2), std::memory_order_relaxed);
  int hum = 0;
  if (!mapOn && gameOver == END_NONE) {
    if (mode == MODE_SHAFT && carMoving) hum = carFloor * LEVEL_GAP < carWorldY ? 1 : 2;
    for (int i = 0; i < liftCount && mode == MODE_ROOM; i++)
      if (lifts[i].moving) hum = (lifts[i].stops[lifts[i].at] < lifts[i].y ? 1 : 2) + 2;   // (3, 4: a pad - quieter)
  }
  droneHum.store(mode == MODE_ROOM && curHazard == HZ_DRONE && !mapOn && gameOver == END_NONE ? 1 : 0, std::memory_order_relaxed);
  int was = liftHum.exchange(hum, std::memory_order_relaxed);
  if (was != 0 && hum == 0 && !mapOn) sfxPlay(SFX_LIFT_STOP);
  drawHud();
}

// For the PC test build: where he is and what he is doing
const char *gameDebug() {
  static char b[160];
  static const char *st[] = { "IDLE", "RUN", "JUMP", "USE", "FALL", "LAND", "LIFT", "KO", "SHOCK", "CAUGHT" };
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

// For the PC test build: straight into room 1, made with layout tpl and the doors given
void gameDebugRoom(int tpl, bool topL, bool topR, int hazard) {
  if (onTitle) layOut(layoutSeed);
  onTitle = false;
  rooms[0].tpl = (uint8_t)(tpl % WIDE_ROOM_COUNT);
  rooms[0].topL = topL;
  rooms[0].topR = topR;
  rooms[0].hazard = (uint8_t)hazard;
  enterRoom(0, true);
}
