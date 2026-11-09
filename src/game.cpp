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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "people.h"            // (the kinds of people: stage 2's chef and maintenance man)
#pragma GCC diagnostic pop
#include "scream_sound.h"
#include <sys/stat.h>

#include "micromod.h"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================ settings
#define GAME_VERSION      "032"
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
#define ITEMS_HIDDEN      12       // pieces of equipment with a QUANTUM DRIVE in them (the drive bay needs them all)
#define DOCUMENTS         5        // TOP SECRET DOCUMENTS in other pieces (bonus points: DOC_VALUES, in the order found)
#define DOC_SHOW          2.8f     // seconds their panel stays up
#define POWER_OUT_SECONDS 0.6f     // the countdown at zero: the lights flicker this long, then out (night vision)...
#define POWER_FAIL_PANEL  1.6f     // ...and MISSION FAILED comes up after this
#define WARN_EVERY        10       // the last minute: every this many seconds the lights flicker and an alert sounds
#define GENERATORS        3        // power generators (each in a different room): all on to power the main computer
#define GENERATOR_BONUS   (5 * 60) // turning on a generator adds this to the countdown (each one)
#define HEALTH_DRAIN_SECONDS 60.0f // every minute of play costs 1 health
#define HI_COUNT          5        // the high score table
#define SHOCK_TIME_LOST   30       // seconds off the clock when the electrified floor gets him...
#define PIT_TIME_LOST     30       // ...or he falls down a pit in the floor
#define BONUS_PER_MINUTE  500      // end of game: each whole minute left on the clock
#define BONUS_CLEAN       1000     // end of game: each of no electrocution, no drone hits, no pit falls,
                                   // never caught on camera, no first aid used
#define CONTROL_COL       4        // the QUANTUM CONTROL room is in this column (the middle one), on a random row
#define CONTROL_ROOM      ROOM_COUNT        // (its index in rooms[], after the 30)
#define SUB_ROOM          (ROOM_COUNT + 1)  // the sub basement's room: the QUANTUMLOOP station (under a lift shaft, once unlocked)
#define ARRIVAL_ROOM      (ROOM_COUNT + 2)  // stage 2: where the QUANTUMLOOP arrives
#define ALL_ROOMS         (ROOM_COUNT + 3)
#define S2_LIFTS          6        // stage 2, the QUANTUM LAB: lifts A..F...
#define S2_LEVELS         6        // ...lift A has a room on its right at each of the 6 levels, B 5, C 4, D 3, E 2, F 1
#define S2_ROOMS          21       // (6 + 5 + 4 + 3 + 2 + 1)
#define S2_ARRIVAL        S2_LEVELS  // lift A's floor under level 1: the QUANTUMLOOP's arrival station, on its right
#define BODY_FOUND_SECONDS 60      // the maintenance man knocked out: the countdown to his body being found...
                                   // ...in the bin, it goes back to the mission's countdown
#define WK_HOME_X         470      // the maintenance man waits for lift A here (on its left walkway)...
#define WK_CORNER_X       66       // ...and runs to the end of it, and cowers there
#define WK_RUN_SPEED      125.0f   // (he runs at about 230)
#define KNOCK_RANGE       85       // X this close to him: knocked out
#define BIN_X             330      // lift A's left walkways: the bin (big enough for a man)...
#define BIN_H             104
#define AID_X             170      // ...and every walkway on the left: the first aid cabinet (once each)
#define KITCHEN_DECK_Y    360      // the kitchen: its storage deck (the floor is the cooking)
#define CLEAN_MID_Y       440      // the cleaning room: three levels - the floor (the front counter), the office...
#define CLEAN_TOP_Y       200      // ...and the stores at the top
#define MED_DECK_Y        380      // the medical bay: its ward upstairs (the nurse station and one bed downstairs)
#define GUARD_DESK_X      760      // the security room: the guard behind the desk
#define NPC_SPEED         62.0f    // the lab's people walking their paths
#define NPC_SIGHT         330      // facing him, on his level, this close: they notice him...
#define SUSP_STEP         0.7f     // ...a suspicion point every this many seconds he looks at them (twice as fast searching)
#define SUSP_STEP_GUARD   0.35f    // ...the security guards: twice as quick to get suspicious...
#define GUARD_AIM_SECONDS 0.5f     // ...and at one point short of recognising him, they draw and shoot
#define SHOT_HURT         10
#define SHOT_TIME_LOST    30
#define KNIFE_RANGE       80       // with the kitchen's knife: X this close to someone (on his level)
#define SEC_ALERT_SECONDS 7.0f     // the security room's door guard knifed: the other guards shoot this long after
#define STORE_MID_Y       440      // the store room: three levels
#define STORE_TOP_Y       200
#define ADMIN_DECK_Y      400      // the admin room: a small deck at the back
#define ADMIN_DESK_X      650      // (where the one at the desk sits)
#define SUSP_MAX          5        // ...this many: recognised - back out to the lift area, the clock down
#define RECOGNISED_TIME_LOST 30
#define CARD_SHOW         3.2f     // ACCESS CARD LEVEL n FOUND: seconds on screen
#define S2_ITEMS          16       // searchable things in a lab room, at most
#define RIDE_SECONDS      20.0f    // the QUANTUMLOOP journey
#define LOOP_POWER_X      500      // the station: the QUANTUMLOOP's power switch...
#define LOOP_DOOR_X       800      // ...and its doors (the middle of the portal)
#define SUB_FLOOR         FLOORS   // the sub basement: a floor under the bottom row of the complex
#define MAX_ORNAMENTS_SHOWN 2      // decorations on a room's walls (not counting the HIGH VOLTAGE sign)
#define SEARCH_SCORE      100      // searching a piece of equipment (the first time)
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
#define CAM_PERIOD        6.0f     // seconds for a sweep across and back...
#define CAM2_PERIOD       7.1f     // ...the second camera in a room with two: a little slower, so the beams drift apart
#define CAM_MID_DEG       38.0f    // the sweep: its middle (degrees from straight down, out into the room)...
#define CAM_SWING_DEG     34.0f    // ...and how far each way
#define CAM_HALF_DEG      7.0f     // half the width of the beam (it goes through everything)
#define CAM_REACT         0.12f    // seconds of moving in the beam before it notices
#define CAM_ENTRY_GRACE   0.8f     // seconds after coming in before it can notice
#define CAUGHT_SECONDS    1.6f     // hands up, then back at the door he came in by
#define DRONE_ROOMS       3        // 10%: a drone hunting him (and 30%, 9 rooms with room 1, have nothing)
#define DRONE_SPEED       115.0f   // its top speed (he runs at about 230)
#define DRONE_WARN        440.0f   // closer than this: its light goes yellow...
#define DRONE_RANGE       270.0f   // ...closer than this, red: it fires
#define DRONE_FIRE_EVERY  0.8f     // seconds between shots (the first a little after it goes red)
#define BULLET_SPEED      650.0f
#define BULLET_HURT       5        // health a bullet costs
#define DRONE_GRACE       1.5f     // seconds it waits after he comes in (or back in)
#define DRONE_SCAN_DEG_S  80.0f    // its red scanning beam turns right round it, this many degrees a second...
#define DRONE_SCAN_HALF   3.0f     // ...half its width (it sees him through anything, moving or not); then it flies
                                   // to where it saw him and scans again. In range it fires, whatever it is doing.
#define NVG_RANGE         400      // night vision: how far the goggles light up ahead...
#define NVG_HALF_DEG      28.0f    // ...half the width of their view...
#define NVG_AROUND        56       // ...and the circle round him
#define MAX_ORNAMENTS     14       // things on a room's walls that are only to look at (lights, fans...)
#ifndef BIG_FAN_MIN
#define BIG_FAN_MIN       88       // a big fan goes where there is a space this big (its radius): rooms with big gaps
#endif
#define ELEVATOR_MUSIC_VOLUME 0.40f  // elevator.mod, while he stands in the lift car: 0.0 = off ... 1.0 = full
#define STAGE1_MUSIC_VOLUME   0.25f  // stage1.mod, the rest of the time in stage 1
#define TITLE_MUSIC_VOLUME    0.35f  // title.mod, on the title screen (and while initials are entered)
#define STAGE2_MUSIC_VOLUME   0.25f  // stage2.mod, stage 2 (from its briefing on)
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
  bool holds[MAX_DECOR];           // ...a quantum drive in it
  bool docs[MAX_DECOR];            // ...TOP SECRET DOCUMENTS in it
  bool kit, kitUsed;               // a first aid kit here, and whether it has been used
  float progress[MAX_DECOR + 2];   // how far each piece has been searched (seconds; then the kit, the generator)
  uint8_t hazard;                  // Hazard
  bool gen, genOn;                 // a power generator here, and whether it has been turned on
  bool term;                       // the facility terminal (one, not in room 1): DOWNLOAD FACILITY BLUEPRINT
  bool noRightDoor;                // (the sub basement's room: a wall on the right)
};
enum Hazard : uint8_t { HZ_NONE, HZ_ELECTRIC, HZ_DARK, HZ_CAMERA, HZ_CAMERA2, HZ_DRONE };
static RoomInfo rooms[ALL_ROOMS];          // numbered 1..30 across the rows, top to bottom; then QUANTUM CONTROL, the sub basement's
static int8_t roomAt[ROWS][COLS];          // which room is in each place (-1: none)
static bool shaftSeen[SHAFTS][FLOORS + 1];
// The way down: the main computer unlocks the sub basement under one lift shaft
static bool subUnlocked = false;
// ---- the nuclear launch sites the facility is hacking (the whole game, both stages). One after
// another they're HACKED (10 minutes for the first, then 6, 5, 4, 3 more); once all five are, or
// QUANTUM LOCKDOWN is on (the hacked ones are then final), the hacked ones are LAUNCHED in turn
// (5 minutes, then 4, 3, 2, 1 more), and each LANDS 10 to 15 minutes after its launch. All the
// launched ones landed: the game is over.
#define NUKE_SITES 5
static const char *const siteName[NUKE_SITES] = { "NORTH ATLANTIC", "EASTERN EUROPE", "PACIFIC", "CENTRAL ASIA", "SOUTHERN REGION" };
static const float HACK_MINUTES[NUKE_SITES] = { 10, 6, 5, 4, 3 };
static const float LAUNCH_MINUTES[NUKE_SITES] = { 5, 4, 3, 2, 1 };
#define LAND_MIN_MINUTES 10
#define LAND_MAX_MINUTES 15
static int sitesHacked = 0, sitesLaunched = 0;
static float hackT = 0, launchT = 0;        // seconds into the hack / launch under way
static float landLeft[NUKE_SITES];          // a launched one: seconds till it lands (<= 0: landed)
static bool lockdown = false;               // QUANTUM LOCKDOWN (the main computer): no more hacking
#define FLYERS 4                            // (their news, flying out at him: see drawFlyers)
#define FLYER_SECONDS 2.6f
struct Flyer { char text[40]; float t; bool on; };
static Flyer flyers[FLYERS];
static bool blueprint = false;             // the facility blueprint downloaded: the generators' rooms shown on the maps
static bool bpMapShown = false;            // (logging off after the download: the map, once)
static bool driveScan = false;             // the QUANTUM DRIVE SCAN run: rooms with drives still to find shown on the maps
static bool scanMapShown = false;
static int subShaft = -1;
// The stages, and the scenes between them
static int stage = 1;
enum Scene : uint8_t { SC_NONE, SC_STAGE_CLEAR, SC_STAGE2_BRIEF, SC_BOARD_WALK, SC_FADE_OUT, SC_RIDE, SC_ARRIVE };
static uint8_t scene = SC_NONE;
static float sceneT = 0;
static bool loopPowered = false;       // the QUANTUMLOOP's power switched on
static float loopPowerT = 9;           // seconds since (its lights coming on)
static float loopDoorT = 0;            // its doors: 0 shut ... 1 open
static int loopDoorDir = 0;            // +1 opening, -1 closing
static std::atomic<int> rideHum{0};    // the journey's rumble

// ---- stage 2: the QUANTUM LAB. Lifts A..F side by side; a room on the right of each lift at its
// levels (from the top level down: A 6, B 5 ... F 1). A room's left door is on its lift's right
// walkway, its right door (where the next lift comes down that far) on the next lift's left one;
// each door has a card reader (access level 1..5: once in, always open). In rooms[0..20].
enum LabKind : uint8_t { LK_LAB, LK_KITCHEN, LK_MEDICAL, LK_SECURITY, LK_ADMIN, LK_CLEANING, LK_STORE };
static const char *const labKindName[] = { "LAB", "KITCHEN", "MEDICAL BAY", "SECURITY", "ADMIN", "CLEANING", "STORE ROOM" };
static int8_t s2Room[S2_LIFTS][S2_LEVELS];   // the room on the right of each lift at each level (-1 none)
static uint8_t s2Kind[S2_ROOMS], s2Lvl[S2_ROOMS];   // what it is, the access level its doors need
static bool s2Aid[S2_LIFTS][S2_LEVELS];      // each left walkway's first aid cabinet: used
static int s2Card = 0;                       // his access card's level (0: none)
static bool s2Disguised = false;             // in the maintenance man's overalls
static bool s2AllAccess = false;             // (the settings, for testing: every door opens)
static float s2BumpT = 0;                    // (ACCESS LEVEL n REQUIRED: not over and over)
enum WorkerState : uint8_t { WK_WAIT, WK_SPOT, WK_RUN, WK_COWER, WK_FALL, WK_DOWN, WK_STRIPPED, WK_BINNED };
static uint8_t wkState = WK_WAIT;            // the maintenance man, waiting for lift A at one of its levels
static int wkFloor = 0, wkFace = 1;
static float wkX = WK_HOME_X, wkT = 0;
static float bodyT = -1;                     // >= 0: seconds till his body is found
static uint8_t s2Act = 0;                    // what he is doing instead of moving: 1 knocking him out, 2 searching him, 3 into the bin
static float s2ActT = 0, binPT = 0;
static int binPhase = 0;
static bool bodyCarried = false;
struct Npc {                                 // the lab's people (the chef, the cleaners): back and forth along a path
  uint8_t kind;                              // PersonKind
  float x, y, a, b;                          // where; the level they're on; the ends of the path
  int dir;
  float t, pause;                            // (walking / working at an end)
  bool watching;                             // suspicious of him: stopped, staring
  bool dead;                                 // (the knife)
  float dieT, aimT;                          // falling; a guard drawing his pistol (> 0) and firing
  float sight;                               // how far he sees
  bool turns;                                // (standing at a post: looking one way, then the other)
  bool knifeOnly;                            // (the security room's door guard: a bullet won't stop him)
  bool seated;                               // (at a desk)
};
#define MAX_NPCS 3
static Npc npcs[MAX_NPCS];
static bool s2NpcDead[S2_ROOMS][MAX_NPCS];  // who has been killed, where they lie
static float s2NpcDX[S2_ROOMS][MAX_NPCS], s2NpcDY[S2_ROOMS][MAX_NPCS];
static int8_t s2NpcDDir[S2_ROOMS][MAX_NPCS];
static bool hasKnife = false;                // the kitchen's knife
static bool hasGun = false;                  // the security room's pistol (R1 / R2 to fire)
static int s2GunItem = 0;                    // (which of the two gun racks it's in)
static float secAlertT = -1;                 // the security room: seconds till the guards open fire (< 0: no alert)
static float gunFlashT = 0;
static int s2KnifeItem = 0, s2CardItem = 2;  // which of the kitchen's things the knife is in; which medical bay patient has the level 2 card
static bool panelKnife = false;              // (the FOUND panel: the knife, not a card)
static float shotFlashT = 0;
static int npcCount = 0;
static int susp = 0;                         // suspicion points (SUSP_MAX: recognised)
static float suspT = 0;
static bool s2Recog = false;                 // recognised: hands up, then out to the lift area
static float cardShowT = 0;                  // ACCESS CARD LEVEL n FOUND
static int cardShown = 0;
static bool s2Searched[S2_ROOMS][S2_ITEMS];  // the lab rooms' equipment: searched...
static float s2Prog[S2_ROOMS][S2_ITEMS];     // ...how far

// Does lift s stop at floor f? Is there a walkway on this side (0 left)?
static bool s2Stops(int s, int f) { return s >= 0 && s < S2_LIFTS && ((f >= 0 && f <= S2_LEVELS - 1 - s) || (s == 0 && f == S2_ARRIVAL)); }
static bool s2Foyer(int s, int f, int side) { return s2Stops(s, f) && (f != S2_ARRIVAL || side == 1); }
static int s2CardLevel() { return s2AllAccess ? 5 : s2Card; }
static bool s2Locked(int idx) { return idx >= 0 && idx < S2_ROOMS && !rooms[idx].visited && s2CardLevel() < s2Lvl[idx]; }
static void s2LevelName(int f, char *b, int n) {
  if (f == S2_ARRIVAL) snprintf(b, n, "Q");
  else snprintf(b, n, "%d", S2_LEVELS - f);
}
static void s2RoomName(int idx, char *b, int n) {
  if (s2Kind[idx] != LK_LAB) snprintf(b, n, "%s", labKindName[s2Kind[idx]]);
  else snprintf(b, n, "LAB %c%d", 'A' + rooms[idx].col, S2_LEVELS - rooms[idx].row);
}
// Quantum drives: found and carried, and loaded into the drive bay in QUANTUM CONTROL
static int drivesCarried = 0, drivesLoaded = 0;
static float leverT = -1;                  // >= 0: the lever of this room's generator going up (seconds)
static bool bayBusy = false;               // drives going into the bay, one by one
static float powerPanelT = 0;              // NO POWER: the panel with a generator's picture
static bool termOn = false;                // the main computer's screen fills the view
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
  if (stage == 2) {                    // the QUANTUM LAB
    if (!s2Stops(s, f)) return -1;
    if (f == S2_ARRIVAL) return right ? ARRIVAL_ROOM : -1;
    if (right) return s2Room[s][f];
    return s > 0 ? s2Room[s - 1][f] : -1;
  }
  if (f == SUB_FLOOR) return (right && subUnlocked && s == subShaft) ? SUB_ROOM : -1;
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
  for (int f = SUB_FLOOR; f >= 0; f--)
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
    if (count[c] >= (c == CONTROL_COL ? ROWS - 1 : MAX_PER_COL)) continue;   // (room left in the middle column for QUANTUM CONTROL)
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
  // QUANTUM CONTROL: in the middle column, on a row its rooms leave free
  int controlRow = rnd(ROWS);
  while (used[controlRow][CONTROL_COL]) controlRow = (controlRow + 1) % ROWS;
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
      for (int i = 0; i < MAX_DECOR; i++) ri.searched[i] = ri.holds[i] = ri.docs[i] = false;
      ri.kit = ri.kitUsed = false;
      for (int i = 0; i < MAX_DECOR + 2; i++) ri.progress[i] = 0;
      ri.hazard = HZ_NONE;
      ri.gen = ri.genOn = ri.noRightDoor = ri.term = false;
    }
  // the two rooms that aren't in the numbering: QUANTUM CONTROL (one level, a door at the bottom
  // on each side) and the sub basement's (a door on the left only; it isn't anywhere until unlocked)
  for (int k = CONTROL_ROOM; k <= ARRIVAL_ROOM; k++) {
    RoomInfo &ri = rooms[k];
    memset(&ri, 0, sizeof(ri));
    ri.row = (int8_t)(k == CONTROL_ROOM ? controlRow : ROWS);
    ri.col = (int8_t)(k == CONTROL_ROOM ? CONTROL_COL : 0);
    ri.topL = ri.topR = false;
    ri.seed = (uint16_t)rnd32();
    ri.noRightDoor = (k != CONTROL_ROOM);
  }
  roomAt[controlRow][CONTROL_COL] = (int8_t)CONTROL_ROOM;
  subUnlocked = false;
  subShaft = -1;
  blueprint = bpMapShown = driveScan = scanMapShown = false;
  sitesHacked = sitesLaunched = 0;
  hackT = launchT = 0;
  for (int i = 0; i < FLYERS; i++) flyers[i].on = false;
  lockdown = false;
  // the rooms with one more piece of equipment, to make EQUIPMENT_TOTAL
  for (int extra = EQUIPMENT_TOTAL - ROOM_COUNT * (EQUIPMENT_TOTAL / ROOM_COUNT); extra > 0;) {
    RoomInfo &ri = rooms[rnd(ROOM_COUNT)];
    if (ri.decorN >= MAX_DECOR || ri.decorN >= wideRooms[ri.tpl].spotCount) continue;
    ri.decorN++;
    extra--;
  }
  // and the pieces with a quantum drive in them, ITEMS_HIDDEN of them, one at most in a room
  for (int k = 0; k < ITEMS_HIDDEN;) {
    RoomInfo &ri = rooms[rnd(ROOM_COUNT)];
    bool has = false;
    for (int m = 0; m < MAX_DECOR; m++) has |= ri.holds[m];
    if (has) continue;
    ri.holds[rnd(ri.decorN)] = true;
    k++;
  }
  // the TOP SECRET DOCUMENTS: in pieces without a drive, each in a different room
  for (int k = 0; k < DOCUMENTS;) {
    RoomInfo &ri = rooms[rnd(ROOM_COUNT)];
    int i = rnd(ri.decorN);
    bool here = false;
    for (int m = 0; m < MAX_DECOR; m++) here |= ri.docs[m];
    if (here || ri.holds[i]) continue;
    ri.docs[i] = true;
    k++;
  }

  // the first aid kits, in rooms with a spare spot for one
  for (int k = 0; k < FIRST_AID_KITS;) {
    RoomInfo &ri = rooms[rnd(ROOM_COUNT)];
    if (ri.kit || ri.decorN >= wideRooms[ri.tpl].spotCount) continue;
    ri.kit = true;
    k++;
  }
  // the power generators, each in a different room with a spare spot for it
  for (int k = 0; k < GENERATORS;) {
    RoomInfo &ri = rooms[rnd(ROOM_COUNT)];
    if (ri.gen || ri.decorN + (ri.kit ? 1 : 0) >= wideRooms[ri.tpl].spotCount) continue;
    ri.gen = true;
    k++;
  }
  // the facility terminal: one, in a room with a spare spot for it (not room 1), and no quantum
  // drive or power generator in that room
  for (int tries = 0; tries < 1000; tries++) {
    RoomInfo &ri = rooms[1 + rnd(ROOM_COUNT - 1)];
    if (ri.decorN + (ri.kit ? 1 : 0) + (ri.gen ? 1 : 0) >= wideRooms[ri.tpl].spotCount) continue;
    bool drive = false;
    for (int m = 0; m < MAX_DECOR; m++) drive |= ri.holds[m];
    if (drive || ri.gen) continue;
    ri.term = true;
    break;
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

// Stage 2's lab: its rooms (in rooms[0..20]), what they are and their access levels; the
// maintenance man's level
static void layOutStage2() {
  rngState = layoutSeed * 2246822519u + 991u;
  int n = 0;
  for (int s = 0; s < S2_LIFTS; s++)
    for (int f = 0; f < S2_LEVELS; f++) {
      s2Room[s][f] = -1;
      s2Aid[s][f] = false;
      if (f > S2_LEVELS - 1 - s) continue;
      RoomInfo &ri = rooms[n];
      memset(&ri, 0, sizeof(ri));
      ri.row = (int8_t)f;
      ri.col = (int8_t)s;
      ri.seed = (uint16_t)rnd32();
      ri.noRightDoor = f > S2_LEVELS - 2 - s;   // (the next lift doesn't come down this far)
      s2Kind[n] = LK_LAB;
      s2Lvl[n] = (uint8_t)(s + 1 > 5 ? 5 : s + 1);   // a column's rooms all the same: lift A's level 1, B's 2 ... E's and F's 5
      for (int k = 0; k < S2_ITEMS; k++) { s2Searched[n][k] = false; s2Prog[n][k] = 0; }
      for (int k = 0; k < MAX_NPCS; k++) s2NpcDead[n][k] = false;
      s2Room[s][f] = (int8_t)n++;
    }
  // lift A's six rooms, in a random order: the kitchen (level 1: the maintenance man's card), the rest 2..5
  uint8_t kinds[S2_LEVELS] = { LK_KITCHEN, LK_MEDICAL, LK_SECURITY, LK_ADMIN, LK_CLEANING, LK_STORE };
  for (int i = S2_LEVELS - 1; i > 0; i--) {
    int j = rnd(i + 1);
    uint8_t k = kinds[i];
    kinds[i] = kinds[j];
    kinds[j] = k;
  }
  for (int f = 0; f < S2_LEVELS; f++) {
    int idx = s2Room[0][f];
    s2Kind[idx] = kinds[f];
  }
  memset(roomAt, -1, sizeof(roomAt));
  wkFloor = rnd(S2_LEVELS);
  wkState = WK_WAIT;
  wkX = WK_HOME_X;
  wkFace = 1;
  wkT = 0;
  bodyT = -1;
  s2Card = 0;
  s2Disguised = false;
  s2Act = 0;
  bodyCarried = false;
  susp = 0;
  cardShowT = 0;
  hasKnife = false;
  hasGun = false;
  secAlertT = -1;
  s2KnifeItem = rnd(8);                      // (one of the kitchen's 8 things)
  s2GunItem = rnd(2);                        // (one of the security room's two gun racks)
  static const int beds[5] = { 2, 4, 5, 6, 7 };
  s2CardItem = beds[rnd(5)];                 // (one of the medical bay's 5 patients)
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
  DEC_KINDS, DEC_FIRSTAID = DEC_KINDS, DEC_GENERATOR, DEC_TERMINAL };
static const int16_t decorW[DEC_KINDS + 3] = { 52, 62, 44, 84, 96, 104, 34, 70, 56, 104, 72, 58, 48, 96, 96 };
static const int16_t decorH[DEC_KINDS + 3] = { 96, 68, 96, 92, 82, 98, 96, 110, 132, 108, 118, 120, 100, 112, 150 };
struct Decor {
  uint8_t kind;
  int16_t x, y;                    // middle, and the top of what it stands on
  uint16_t seed;                   // its blinking lights
  int16_t w, h;                    // (DEC_LAB: its size - the lab rooms' equipment is painted with the room)
};
#define DEC_LAB (DEC_TERMINAL + 1)
#define DECOR_SLOTS 16
static Decor decor[DECOR_SLOTS];         // (the equipment, then a first aid kit and a power generator if the room has them; the lab: its things)
static int dW(int i) { return decor[i].kind == DEC_LAB ? decor[i].w : decorW[decor[i].kind]; }
static int dH(int i) { return decor[i].kind == DEC_LAB ? decor[i].h : decorH[decor[i].kind]; }
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
  float period;                        // seconds for its sweep across and back
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
static bool drScan = true;             // scanning with its beam (else flying to where the beam last found him)
static float drScanAng = 0, drScanDir = 1;   // the beam (degrees from straight down, + to the right), turning right round
static float drTX = 0, drTY = 0;       // where it is flying to
static bool drAlerted = false;         // (it has found him once since he came in)
static void endGame(int why);
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
enum EndReason : uint8_t { END_NONE, END_TIME, END_HEALTH, END_WIN, END_BODY, END_NUKES };
static EndReason gameOver = END_NONE;
static float gameOverT = 0;
static float playT = 0;                // time played (seconds of play: not the map, not the opening)
static float drainT = 0;               // towards the next minute's health
static int warnSlot = 99;              // the last minute: which ten seconds the last alert was in
static float warnT = 9;                // seconds since that alert (the lights flicker for the first moment)
// What happened this game (for the end of game bonuses), and the bonuses
static int nShocks = 0, nDroneHits = 0, nPitFalls = 0, nCaught = 0, nKitsUsed = 0;
#define BONUS_LINES 6
static long bonusPts[BONUS_LINES];
static long bonusTotal = 0;
// The high scores
struct HiEntry { char name[4]; int32_t score; int32_t secs; };
static HiEntry hiTable[HI_COUNT] = { { "ACE", 25000, 850 }, { "VOS", 20000, 785 }, { "MAX", 15000, 700 },
                                     { "ZED", 10000, 560 }, { "KAT", 5000, 405 } };
static bool entryOn = false;           // entering initials for the table
static int entryPos = 0, entrySlot = -1;
static char entryName[4] = "AAA";
static float entryT = 0;
static char floatText[24] = "";        // a word floating up from the equipment (NOTHING, HEALTH RESTORED...)
static uint16_t floatColour = 0;

// The score
static long score = 0;
static int itemsFound = 0;
static float itemShowT = 0;            // FOUND OBJECT on screen
static float docShowT = 0;             // TOP SECRET DOCUMENTS on screen
static int docsFound = 0;
static long docValue = 0;              // (the last one's value)
static const long DOC_VALUES[DOCUMENTS] = { 1000, 1500, 2000, 2500, 5000 };
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
static bool settingsOn = false;        // the settings menu (Triangle): the game paused
static int settingsSel = 0;
static bool quitConfirm = false;       // QUIT TO TITLE pressed once: X again to quit
static bool briefingOn = false;        // STAGE 1's briefing, after the opening: X to start
static float briefingT = 0;
static std::atomic<bool> musicOn[2] = { true, true };   // 0 ELEVATOR MUSIC, 1 STAGE MUSIC (the settings)
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
long micromod3_calculate_mod_file_len(signed char *module_header);
long micromod3_initialise(signed char *data, long sampling_rate);
void micromod3_set_gain(long value);
void micromod3_get_audio(short *output_buffer, long count);
void micromod3_set_position(long pos);
long micromod4_calculate_mod_file_len(signed char *module_header);
long micromod4_initialise(signed char *data, long sampling_rate);
void micromod4_set_gain(long value);
void micromod4_get_audio(short *output_buffer, long count);
}

enum Sfx : uint8_t { SFX_NONE, SFX_STEP, SFX_LIFT_STOP, SFX_BEEP, SFX_CLUNK, SFX_FOUND, SFX_FANFARE, SFX_LAND,
                     SFX_BUZZ, SFX_ZAP, SFX_POWERDOWN, SFX_SCREAM, SFX_ALARM, SFX_SHOT, SFX_HIT,
                     SFX_POWERUP, SFX_CLICK, SFX_DENIED, SFX_KEY, SFX_SHUTDOWN, SFX_WARN, SFX_LOOPPOWER, SFX_DOORS, SFX_TICK24 };
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

static std::atomic<int> tuneWanted{3};    // 0 none, 1 the lift's (elevator.mod), 2 stage 1's (stage1.mod), 3 the title's (title.mod), 4 stage 2's (stage2.mod)
static std::atomic<int> liftHum{0};       // 0 still, 1 the car going up, 2 down, 3 / 4 a room's lift pad
static std::atomic<int> droneHum{0};      // a drone in the room: its rotors' whine (0 none)
#define TUNES 4                            // 0 elevator.mod, 1 stage1.mod, 2 title.mod, 3 stage2.mod: a MOD player each
static bool tuneReady[TUNES] = { false, false, false, false };
static char tuneProblem[TUNES][48] = { "NO ELEVATOR.MOD IN THE ASSETS", "NO STAGE1.MOD IN THE ASSETS", "NO TITLE.MOD IN THE ASSETS",
                                       "NO STAGE2.MOD IN THE ASSETS" };
static float tuneGain[TUNES] = { 0, 0, 0, 0 };   // (the sound thread's)
static std::atomic<bool> titleFromStart{false};   // (back at the title: its tune starts again from the top)
static long (*const modLen[TUNES])(signed char *) = { micromod_calculate_mod_file_len, micromod2_calculate_mod_file_len,
                                                     micromod3_calculate_mod_file_len, micromod4_calculate_mod_file_len };
static long (*const modInit[TUNES])(signed char *, long) = { micromod_initialise, micromod2_initialise, micromod3_initialise, micromod4_initialise };
static void (*const modGain[TUNES])(long) = { micromod_set_gain, micromod2_set_gain, micromod3_set_gain, micromod4_set_gain };
static void (*const modAudio[TUNES])(short *, long) = { micromod_get_audio, micromod2_get_audio, micromod3_get_audio, micromod4_get_audio };
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
    case SFX_POWERUP:                          // a generator starting: the lever's clunk, a rising whine, the engine
      startVoice(WAVE_NOISE, 900, 150, 0, 90, 0.30f);
      startVoice(WAVE_TRIANGLE, 80, 520, 100, 900, 0.30f);
      startVoice(WAVE_SQUARE, 40, 90, 100, 900, 0.08f);
      startVoice(WAVE_TRIANGLE, 520, 520, 1000, 300, 0.18f);
      break;
    case SFX_SHUTDOWN:                         // the countdown at zero: breakers clunk, the generators wind down to nothing
      startVoice(WAVE_NOISE, 700, 90, 0, 260, 0.30f);
      startVoice(WAVE_SQUARE, 90, 40, 0, 220, 0.14f);
      startVoice(WAVE_TRIANGLE, 420, 30, 120, 2600, 0.30f);
      startVoice(WAVE_SQUARE, 110, 16, 120, 2600, 0.09f);
      startVoice(WAVE_TRIANGLE, 300, 24, 420, 2300, 0.20f);
      startVoice(WAVE_NOISE, 400, 50, 2500, 300, 0.25f);
      break;
    case SFX_WARN:                             // the last minute: a two-tone alert, three times
      for (int k = 0; k < 3; k++) {
        startVoice(WAVE_SQUARE, 700, 700, k * 300, 140, 0.13f);
        startVoice(WAVE_SQUARE, 520, 520, k * 300 + 150, 140, 0.13f);
      }
      break;
    case SFX_LOOPPOWER:                        // the QUANTUMLOOP powering up: a rising whine, then a chime
      startVoice(WAVE_NOISE, 900, 150, 0, 90, 0.30f);
      startVoice(WAVE_TRIANGLE, 60, 900, 80, 2000, 0.25f);
      startVoice(WAVE_SQUARE, 30, 120, 80, 2000, 0.06f);
      startVoice(WAVE_TRIANGLE, 1047, 1047, 2100, 300, 0.30f);
      startVoice(WAVE_TRIANGLE, 1568, 1568, 2250, 500, 0.25f);
      break;
    case SFX_TICK24:                           // a site hacked: a heavy digital clock ticking (the TV show 24's), dying away
      for (int k = 0; k < 6; k++) {
        float lv = 1.0f - k * 0.13f;
        startVoice(WAVE_SQUARE, 150, 95, k * 760, 90, 0.20f * lv);          // the thud
        startVoice(WAVE_TRIANGLE, 1180, 880, k * 760, 45, 0.24f * lv);      // the click on top
        startVoice(WAVE_NOISE, 2600, 600, k * 760, 35, 0.12f * lv);
        startVoice(WAVE_SQUARE, 150, 95, k * 760 + 380, 60, 0.10f * lv);    // ...and its softer echo
      }
      break;
    case SFX_DOORS:                            // its doors sliding: a hiss and a hum
      startVoice(WAVE_NOISE, 3000, 600, 0, 900, 0.20f);
      startVoice(WAVE_SQUARE, 200, 140, 0, 900, 0.05f);
      startVoice(WAVE_NOISE, 500, 100, 950, 150, 0.22f);
      break;
    case SFX_CLICK:                            // a drive sliding into the bay
      startVoice(WAVE_SQUARE, 1200, 1200, 0, 15, 0.12f);
      startVoice(WAVE_NOISE, 3000, 1000, 0, 30, 0.10f);
      break;
    case SFX_DENIED:                           // no power / no boot: two low buzzes
      startVoice(WAVE_SQUARE, 140, 130, 0, 180, 0.14f);
      startVoice(WAVE_SQUARE, 140, 130, 240, 260, 0.14f);
      break;
    case SFX_KEY:                              // the terminal's menu: a short tick
      startVoice(WAVE_SQUARE, 1800, 1800, 0, 12, 0.08f);
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

// The QUANTUMLOOP's journey: a deep rumble, rushing past in waves
static float rdLP = 0, rdPhase = 0, rdWave = 0, rdLevel = 0;
static uint32_t rdNoise = 777;
static inline float rideSample(int on) {
  rdLevel += ((on ? 0.22f : 0.0f) - rdLevel) * 0.0004f;
  if (rdLevel < 0.0005f && !on) return 0;
  rdNoise = rdNoise * 1664525u + 1013904223u;
  rdLP += ((float)(rdNoise >> 16) / 32768.0f - 1.0f - rdLP) * 0.03f;
  rdPhase += 41.0f / AUDIO_RATE;
  if (rdPhase >= 1) rdPhase -= 1;
  rdWave += 2.6f / AUDIO_RATE;
  if (rdWave >= 1) rdWave -= 1;
  float tri = 4.0f * fabsf(rdPhase - 0.5f) - 1.0f;
  return (rdLP * 2.2f + tri * 0.35f) * (0.6f + 0.4f * sinf(rdWave * TWO_PI)) * rdLevel;
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
  if (which < 0 || which >= TUNES || !data || len < 1084) return;
  static const char *const names[TUNES] = { "ELEVATOR.MOD", "STAGE1.MOD", "TITLE.MOD", "STAGE2.MOD" };
  const char *name = names[which];
  long need = modLen[which](data);
  if (need < 0) { snprintf(tuneProblem[which], 48, "%s: NOT A MOD FILE", name); return; }
  if (need > len) { snprintf(tuneProblem[which], 48, "%s: FILE CUT SHORT", name); return; }
  long ok = modInit[which](data, AUDIO_RATE);
  if (ok != 0) { snprintf(tuneProblem[which], 48, "%s: WON'T PLAY", name); return; }
  modGain[which](64);
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
  int ride = rideHum.load(std::memory_order_relaxed);
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
    fx[n] = mix * SFX_VOLUME + liftHumSample(hum) + droneSample(drone) + rideSample(ride);
  }
  // the three tunes, each fading towards its level (a tune at nothing waits where it was)
  static int32_t acc[2048 * 2];
  memset(acc, 0, (size_t)frames * 2 * sizeof(int32_t));
  int want = tuneWanted.load(std::memory_order_relaxed);
  if (tuneReady[2] && tuneGain[2] <= 0 && titleFromStart.exchange(false)) micromod3_set_position(0);
  for (int k = 0; k < TUNES; k++) {
    static const float vols[TUNES] = { ELEVATOR_MUSIC_VOLUME, STAGE1_MUSIC_VOLUME, TITLE_MUSIC_VOLUME, STAGE2_MUSIC_VOLUME };
    float vol = vols[k];
    bool allowed = k == 2 || musicOn[k == 3 ? 1 : k].load(std::memory_order_relaxed);   // (the settings: stage music, both stages; elevator music)
    float target = (tuneReady[k] && want == k + 1 && allowed) ? vol : 0.0f;
    if (!tuneReady[k] || (tuneGain[k] <= 0 && target <= 0)) continue;
    memset(tuneBuf, 0, (size_t)frames * 4);
    modAudio[k](tuneBuf, frames);
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
static void paintRoomSide(bool right, int doorY, bool door = true) {
  uint16_t frame = rgb565(96, 104, 118), frameHi = rgb565(150, 158, 172), dark = rgb565(8, 10, 14);
  // the bulkhead
  int bx0 = right ? SCREEN_W - 16 : 0, bx1 = right ? SCREEN_W - 1 : 15;
  fillRect(bx0, HUD_H, bx1, ROOM_FLOOR_Y - 1, frame);
  fillRect(right ? bx0 : bx1 - 2, HUD_H, right ? bx0 + 2 : bx1, ROOM_FLOOR_Y - 1, frameHi);
  for (int y = 70; y < ROOM_FLOOR_Y; y += 40) fillCircle((bx0 + bx1) / 2, y, 2, rgb565(60, 66, 78));
  if (!door) return;                       // (a plain wall)
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

// A POWER GENERATOR: a yellow housing on a skid, an engine grille, an exhaust,
// a control box on its side with the lever and the lamp (those are drawn each
// frame: drawDecorAnim)
// The facility terminal: a tall grey console on its own, a big green screen, a keyboard shelf, a
// beacon on top (nothing else looks like it)
static void paintFacilityTerminal(int x, int fy) {
  uint16_t body = rgb565(92, 100, 116), dark = rgb565(54, 60, 72), lite = rgb565(140, 150, 168);
  int xs[4] = { x - 40, x + 40, x + 34, x - 34 }, ys[4] = { fy - 150, fy - 150, fy - 1, fy - 1 };
  fillPolygon(xs, ys, 4, body);
  fillRect(x - 40, fy - 150, x + 40, fy - 146, lite);
  fillRect(x - 46, fy - 8, x + 46, fy - 1, dark);                 // its foot
  fillRect(x - 32, fy - 140, x + 32, fy - 88, rgb565(30, 32, 36)); // the screen's bezel (the screen is drawn live)
  fillRect(x - 48, fy - 80, x + 48, fy - 70, lite);               // the keyboard shelf
  for (int k = 0; k < 8; k++) fillRect(x - 42 + k * 11, fy - 79, x - 34 + k * 11, fy - 75, dark);
  for (int k = 0; k < 4; k++) fillRect(x - 24, fy - 58 + k * 8, x + 24, fy - 56 + k * 8, dark);   // vents
  fillRect(x - 6, fy - 160, x + 6, fy - 151, dark);               // the beacon's base
  fillRect(x - 30, fy - 20, x - 12, fy - 14, rgb565(240, 196, 40));
  drawVectorText("FT-7", x + 16, fy - 17, 1.1f, rgb565(200, 206, 220), false);
}

static void paintGenerator(int x, int fy) {
  int x0 = x - 48, x1 = x + 47;
  uint16_t yel = rgb565(214, 168, 40), yelHi = rgb565(246, 206, 90), yelLo = rgb565(150, 112, 24);
  uint16_t dark = rgb565(34, 36, 40), steel = rgb565(96, 102, 114);
  fillRect(x0, fy - 10, x1, fy - 1, dark);                                // the skid
  fillRect(x0 + 6, fy - 12, x1 - 6, fy - 10, steel);
  fillRect(x0 + 4, fy - 82, x1 - 22, fy - 12, yel);                       // the housing
  fillRect(x0 + 4, fy - 82, x1 - 22, fy - 79, yelHi);
  fillRect(x1 - 25, fy - 79, x1 - 22, fy - 12, yelLo);
  fillRect(x0 + 10, fy - 72, x0 + 46, fy - 22, dark);                     // the engine grille (its fan turns, once on)
  for (int y = fy - 68; y < fy - 24; y += 6) fillRect(x0 + 12, y, x0 + 44, y + 1, rgb565(60, 64, 72));
  fillCircle(x0 + 28, fy - 47, 18, rgb565(24, 26, 30));
  fillRect(x0 + 52, fy - 70, x0 + 66, fy - 64, yelLo);                    // vents
  fillRect(x0 + 52, fy - 58, x0 + 66, fy - 52, yelLo);
  hazardBand(x0 + 50, x1 - 26, fy - 30, fy - 22);
  fillRect(x0 + 14, fy - 98, x0 + 22, fy - 82, steel);                    // the exhaust
  fillRect(x0 + 12, fy - 100, x0 + 24, fy - 97, rgb565(60, 64, 72));
  fillRect(x1 - 20, fy - 92, x1, fy - 30, rgb565(70, 76, 88));            // the control box
  fillRect(x1 - 20, fy - 92, x1, fy - 90, rgb565(130, 136, 148));
  fillRect(x1 - 14, fy - 72, x1 - 6, fy - 40, rgb565(20, 22, 26));       // the lever's slot
  drawVectorText("GEN", x0 + 64, fy - 40, 1.4f, rgb565(60, 44, 10), true);
}

// The generator's lever (k: 0 down, off ... 1 up, on) and its lamp
static void drawGeneratorLive(int x, int fy, float k, bool on) {
  int x1 = x + 47;
  float px = x1 - 10.0f, py = fy - 56.0f;
  float a = (60.0f - 120.0f * k) * 0.017453293f;                         // 60 degrees down ... 60 up
  float hx = px + cosf(a) * 4.0f, hy = py + sinf(a) * 24.0f;
  drawLineF(px, py, hx + 6, hy, rgb565(170, 176, 186), 3.0f);
  fillCircle((int)(hx + 6), (int)hy, 4, rgb565(200, 40, 30));             // the knob
  fillCircle((int)px, (int)py, 3, rgb565(60, 64, 72));
  int lx = x1 - 10, ly = fy - 84;
  uint16_t lamp = on ? rgb565(80, 255, 120) : rgb565(220, 40, 30);
  fillCircle(lx, ly, 5, lamp);
  fillCircle(lx - 1, ly - 2, 1, rgb565(255, 255, 255));
  if (on) glowDisc(lx + 0.5f, ly + 0.5f, 34, 60, 255, 110, 0.5f);
  if (on) {                                                               // the engine's fan turning
    int cx = x - 48 + 28, cy = fy - 47;
    for (int b = 0; b < 3; b++) {
      float ang = clockT * 14.0f + b * TWO_PI / 3;
      drawLineF(cx + 0.5f, cy + 0.5f, cx + 0.5f + cosf(ang) * 15, cy + 0.5f + sinf(ang) * 15, rgb565(110, 116, 128), 4.0f);
    }
    fillCircle(cx, cy, 4, rgb565(150, 156, 168));
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
    case DEC_GENERATOR: paintGenerator(d.x, d.y); break;
    case DEC_TERMINAL: paintFacilityTerminal(d.x, d.y); break;
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
    } else if (d.kind == DEC_TERMINAL) {         // its screen: LOGIN with a cursor; the beacon turning
      int x = d.x, fy = d.y;
      fillRect(x - 28, fy - 136, x + 28, fy - 92, rgb565(4, 30, 14));
      for (int y = fy - 136; y < fy - 92; y += 3) fillRect(x - 28, y, x + 28, y, rgb565(2, 18, 8));
      drawVectorText("FACILITY", x, fy - 126, 1.2f, rgb565(80, 220, 120), false);
      drawVectorText(blueprint ? "BLUEPRINT" : "LOGIN", x, fy - 112, 1.3f, rgb565(120, 255, 150), false);
      if (fmodf(clockT, 0.8f) < 0.45f) fillRect(x - 6, fy - 102, x + 6, fy - 98, rgb565(120, 255, 150));
      bool on = fmodf(clockT, 1.0f) < 0.5f;
      fillCircle(x, fy - 164, 6, on ? rgb565(80, 200, 255) : rgb565(30, 70, 110));
      if (on) glowDisc((float)x, (float)(fy - 164), 16, 80, 200, 255, 0.35f);
    } else if (d.kind == DEC_GENERATOR) {
      bool on = rooms[curRoom].genOn;
      float k = on ? (leverT >= 0 ? fminf(1.0f, leverT / 0.5f) : 1.0f) : 0.0f;
      drawGeneratorLive(d.x, d.y, k, on && k >= 1.0f);
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

// The lab rooms' things to search (painted with the room): x middle, y the level, w, h
struct LabItem { int16_t x, y, w, h; };
static const LabItem kitchenItems[] = {
  { 325, ROOM_FLOOR_Y, 150, 236 }, { 570, ROOM_FLOOR_Y, 260, 92 }, { 865, ROOM_FLOOR_Y, 250, 82 }, { 1110, ROOM_FLOOR_Y, 160, 90 },   // fridge, range, prep table, sink
  { 395, KITCHEN_DECK_Y, 250, 186 }, { 680, KITCHEN_DECK_Y, 240, 186 }, { 890, KITCHEN_DECK_Y, 140, 100 }, { 1115, KITCHEN_DECK_Y, 250, 186 } };   // the racks, boxes and water
static const LabItem cleaningItems[] = {
  { 325, ROOM_FLOOR_Y, 150, 190 }, { 470, ROOM_FLOOR_Y, 100, 56 }, { 720, ROOM_FLOOR_Y, 320, 100 }, { 990, ROOM_FLOOR_Y, 140, 190 }, { 1140, ROOM_FLOOR_Y, 100, 110 },   // shelves, buckets, the counter, shelves, the trolley
  { 325, CLEAN_MID_Y, 90, 170 }, { 430, CLEAN_MID_Y, 90, 170 }, { 650, CLEAN_MID_Y, 180, 110 }, { 825, CLEAN_MID_Y, 70, 130 }, { 990, CLEAN_MID_Y, 140, 80 }, { 1160, CLEAN_MID_Y, 120, 60 },   // cupboards, desk, filing cabinet, boxes
  { 350, CLEAN_TOP_Y, 140, 80 }, { 545, CLEAN_TOP_Y, 150, 70 }, { 730, CLEAN_TOP_Y, 100, 120 }, { 910, CLEAN_TOP_Y, 140, 80 }, { 1120, CLEAN_TOP_Y, 160, 66 } };   // crates, boxes, the mops, crates, boxes
static const LabItem medicalItems[] = {
  { 320, ROOM_FLOOR_Y, 140, 60 }, { 640, ROOM_FLOOR_Y, 280, 100 }, { 960, ROOM_FLOOR_Y, 200, 70 }, { 1140, ROOM_FLOOR_Y, 90, 160 },   // seats, the nurse station, a patient, his machines
  { 340, MED_DECK_Y, 190, 70 }, { 560, MED_DECK_Y, 190, 70 }, { 780, MED_DECK_Y, 190, 70 }, { 1000, MED_DECK_Y, 190, 70 },   // the ward's four patients
  { 1180, MED_DECK_Y, 90, 170 } };                                                                                              // the medicine cabinet
static const LabItem securityItems[] = {
  { 250, ROOM_FLOOR_Y, 100, 160 }, { 370, ROOM_FLOOR_Y, 100, 160 }, { 510, ROOM_FLOOR_Y, 84, 150 },   // two gun racks, lockers
  { GUARD_DESK_X, ROOM_FLOOR_Y, 260, 100 }, { 990, ROOM_FLOOR_Y, 60, 120 }, { 1130, ROOM_FLOOR_Y, 150, 60 } };   // the desk, the filing cabinet, seats
static const LabItem storeItems[] = {
  { 120, ROOM_FLOOR_Y, 90, 170 }, { 250, ROOM_FLOOR_Y, 110, 120 }, { 400, ROOM_FLOOR_Y, 150, 186 }, { 580, ROOM_FLOOR_Y, 100, 150 },
  { 740, ROOM_FLOOR_Y, 140, 100 }, { 900, ROOM_FLOOR_Y, 90, 170 },                                        // the floor
  { 110, STORE_MID_Y, 130, 90 }, { 260, STORE_MID_Y, 84, 150 }, { 420, STORE_MID_Y, 150, 186 }, { 600, STORE_MID_Y, 140, 80 },
  { 760, STORE_MID_Y, 90, 170 }, { 920, STORE_MID_Y, 140, 110 },                                          // the middle
  { 130, STORE_TOP_Y, 150, 70 }, { 360, STORE_TOP_Y, 140, 80 }, { 580, STORE_TOP_Y, 90, 110 }, { 820, STORE_TOP_Y, 160, 66 } };   // the top
static const LabItem adminItems[] = {
  { 310, ROOM_FLOOR_Y, 260, 100 }, { ADMIN_DESK_X, ROOM_FLOOR_Y, 180, 72 }, { 890, ROOM_FLOOR_Y, 200, 70 }, { 1030, ROOM_FLOOR_Y, 60, 120 },
  { 1210, ROOM_FLOOR_Y, 40, 124 },                                                                       // counter, desk, table, filing, cooler
  { 790, ADMIN_DECK_Y, 120, 190 }, { 930, ADMIN_DECK_Y, 90, 170 }, { 1040, ADMIN_DECK_Y, 80, 60 } };       // the archives, a cupboard, boxes
static void placeLabItems(int idx) {
  decorCount = 0;
  auto add = [](const LabItem &it) {
    if (decorCount >= DECOR_SLOTS) return;
    Decor &d = decor[decorCount++];
    d.kind = DEC_LAB;
    d.x = it.x; d.y = it.y; d.w = it.w; d.h = it.h;
    d.seed = 0;
  };
  if (s2Kind[idx] == LK_KITCHEN) for (const LabItem &it : kitchenItems) add(it);
  if (s2Kind[idx] == LK_CLEANING) for (const LabItem &it : cleaningItems) add(it);
  if (s2Kind[idx] == LK_MEDICAL) for (const LabItem &it : medicalItems) add(it);
  if (s2Kind[idx] == LK_SECURITY) for (const LabItem &it : securityItems) add(it);
  if (s2Kind[idx] == LK_STORE) for (const LabItem &it : storeItems) add(it);
  if (s2Kind[idx] == LK_ADMIN) for (const LabItem &it : adminItems) add(it);
}
static void placeDecor(const RoomInfo &ri, const WideRoom &t) {
  decorCount = 0;
  if (stage == 2 && &ri - rooms < S2_ROOMS) {   // the lab: its rooms' own things
    placeLabItems((int)(&ri - rooms));
    return;
  }
  if (&ri - rooms >= ROOM_COUNT) return;   // (no equipment in QUANTUM CONTROL or the sub basement)
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
    d.w = d.h = 0;
    d.kind = (uint8_t)kind;
    d.x = s.x;
    d.y = s.y;
    d.seed = (uint16_t)rnd32();
  }
  // the first aid kit, on the next spot, then the power generator
  int next = ri.decorN;
  if (ri.kit && next < t.spotCount) {
    const Spot &s = t.spots[order[next++]];
    Decor &d = decor[decorCount++];
    d.kind = DEC_FIRSTAID;
    d.x = s.x;
    d.y = s.y;
    d.seed = 0;
  }
  if (ri.gen && next < t.spotCount) {
    const Spot &s = t.spots[order[next++]];
    Decor &d = decor[decorCount++];
    d.kind = DEC_GENERATOR;
    d.x = s.x;
    d.y = s.y;
    d.seed = 0;
  }
  if (ri.term && next < t.spotCount) {
    // (the terminal is tall: a spot with nothing low over it, if there is one)
    for (int k = next; k < t.spotCount; k++) {
      const Spot &c = t.spots[order[k]];
      int room = 1000;
      for (int p = 0; p < t.platCount; p++) {
        const Plat &pl = t.plats[p];
        if (pl.y < c.y && pl.x1 > c.x - 48 && pl.x0 < c.x + 48 && c.y - pl.y - PLAT_T < room) room = c.y - pl.y - PLAT_T;
      }
      if (room >= decorH[DEC_TERMINAL] + 20) {
        int tmp = order[next];
        order[next] = order[k];
        order[k] = tmp;
        break;
      }
    }
    const Spot &s = t.spots[order[next++]];
    Decor &d = decor[decorCount++];
    d.kind = DEC_TERMINAL;
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
  ornCount = keepClearN = 0;
  if (&ri - rooms >= ROOM_COUNT) return;   // (QUANTUM CONTROL and the sub basement have their own)
  uint32_t save = rngState;
  rngState = ri.seed * 2246822519u + 777u;
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
  // at most MAX_ORNAMENTS_SHOWN of them: the big fan (half the time, where it fits) and the rest, different kinds
  int shown = 0;
  if (bestR && rnd(2) && addOrnament(ORN_BIGFAN, bx, by, bestR)) shown++;
  static const uint8_t small[5] = { ORN_REDLIGHT, ORN_FAN, ORN_TUBE, ORN_GAUGE, ORN_NOTICE };
  int first = rnd(5);
  for (int k = 0; k < 5 && shown < MAX_ORNAMENTS_SHOWN; k++) {
    int kind = small[(first + k * 2) % 5];   // (every other one round the list: a mix)
    int before = ornCount;
    tryOrnament(kind, kind == ORN_FAN ? 24 : (kind == ORN_TUBE ? 52 + rnd(3) * 8 : 0));
    if (ornCount > before) shown++;
  }
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

// ---- QUANTUM CONTROL: one level, under a false ceiling full of ducts. Its
// equipment is NUCLEAR MISSION's LCC (computer racks, reel to reel tape units,
// the fan, the status display over the main computer, the printer) and the
// QUANTUM DRIVE BAY. What moves is drawn each frame (drawControlRoomAnim).
#define CC_CEIL_Y     250          // its false ceiling
#define CONSOLE_X     640          // the main computer
#define BAY_X         160          // the QUANTUM DRIVE BAY (200 wide), on the left
#define BAY_TOP       (ROOM_FLOOR_Y - 262)
static const int ccRackX[3] = { 1000, 1066, 1160 };   // (the racks and tape units: on the right)
static const int ccTapeX[2] = { 804, 872 };
#define CC_FAN_X 862
#define CC_FAN_Y 330

static void drawTextLeft(const char *t, float x, float y, float size, uint16_t c, bool thick) {
  drawVectorText(t, x + vectorTextWidth(t, size) / 2, y, size, c, thick);
}

static void paintControlRoom() {
  int fy = ROOM_FLOOR_Y;
  // the walls: dark steel panels
  fillRect(0, HUD_H, SCREEN_W - 1, PLAY_Y1, rgb565(30, 36, 48));
  for (int x = 0; x < SCREEN_W; x += 160) {
    fillRect(x + 5, CC_CEIL_Y + 8, x + 154, fy - 8, rgb565(40, 48, 62));
    fillRect(x, CC_CEIL_Y, x + 2, fy - 1, rgb565(20, 24, 32));
  }
  fillRect(0, fy - 96, SCREEN_W - 1, fy - 90, rgb565(26, 30, 40));          // a rail along the wall
  // the false ceiling: ducts and pipes, a fascia with the room's name, lamps under it
  fillRect(0, HUD_H, SCREEN_W - 1, CC_CEIL_Y - 1, rgb565(22, 26, 34));
  for (int x = 40; x < SCREEN_W; x += 210) {                                // ducts going up
    fillRect(x, HUD_H, x + 70, CC_CEIL_Y - 40, rgb565(70, 78, 92));
    fillRect(x, HUD_H, x + 4, CC_CEIL_Y - 40, rgb565(110, 118, 132));
    for (int y = HUD_H + 20; y < CC_CEIL_Y - 50; y += 30) fillRect(x - 3, y, x + 73, y + 4, rgb565(54, 60, 72));
  }
  paintPipe(96, 22);
  paintPipe(150, 14);
  fillRect(0, CC_CEIL_Y - 44, SCREEN_W - 1, CC_CEIL_Y - 1, rgb565(56, 62, 76));   // the fascia
  fillRect(0, CC_CEIL_Y - 44, SCREEN_W - 1, CC_CEIL_Y - 42, rgb565(120, 128, 144));
  hazardBand(0, SCREEN_W - 1, CC_CEIL_Y - 8, CC_CEIL_Y - 1);
  drawVectorText("QUANTUM CONTROL", SCREEN_W / 2, CC_CEIL_Y - 26, 2.8f, rgb565(255, 190, 70), true);
  for (int x = 70; x < SCREEN_W; x += 180) fillRect(x, CC_CEIL_Y, x + 40, CC_CEIL_Y + 3, rgb565(255, 230, 170));
  // cable tray under the ceiling (NUCLEAR MISSION's)
  fillRect(0, CC_CEIL_Y + 12, SCREEN_W - 1, CC_CEIL_Y + 17, rgb565(60, 64, 72));
  for (int x = 30; x < SCREEN_W; x += 90) drawLine(x, CC_CEIL_Y + 18, x + 10, CC_CEIL_Y + 50, rgb565(20, 22, 26), false);
  // computer racks: three on the right
  for (int k = 0; k < 3; k++) {
    int x0 = ccRackX[k], x1 = x0 + 58;
    fillRect(x0, fy - 232, x1, fy - 1, rgb565(38, 42, 50));
    fillRect(x0, fy - 232, x1, fy - 229, rgb565(90, 96, 108));
    fillRect(x0 + 4, fy - 222, x1 - 4, fy - 90, rgb565(22, 24, 30));      // the light panel
    for (int r = 0; r < 4; r++) fillRect(x0 + 6, fy - 80 + r * 16, x1 - 6, fy - 72 + r * 16, rgb565(54, 58, 68));   // vents
    fillRect(x1 - 3, fy - 228, x1, fy - 1, rgb565(26, 28, 34));
  }
  // reel to reel tape units
  for (int k = 0; k < 2; k++) {
    int x0 = ccTapeX[k], x1 = x0 + 62;
    fillRect(x0, fy - 228, x1, fy - 1, rgb565(168, 166, 156));
    fillRect(x0, fy - 228, x1, fy - 225, rgb565(206, 204, 194));
    fillRect(x1 - 3, fy - 224, x1, fy - 1, rgb565(130, 128, 120));
    fillRect(x0 + 5, fy - 216, x1 - 5, fy - 104, rgb565(40, 44, 52));      // the window
    fillRect(x0 + 8, fy - 96, x1 - 8, fy - 84, rgb565(60, 62, 66));        // tape head
    for (int b = 0; b < 3; b++) fillRect(x0 + 8 + b * 16, fy - 72, x0 + 18 + b * 16, fy - 64, rgb565(90, 92, 100));
    fillRect(x0 + 6, fy - 50, x1 - 6, fy - 8, rgb565(150, 148, 140));      // lower door
    drawRectOutline(x0 + 6, fy - 50, x1 - 6, fy - 8, rgb565(120, 118, 112), 1);
  }
  // the fan on the wall: housing (the blades turn)
  fillCircle(CC_FAN_X, CC_FAN_Y, 46, rgb565(50, 54, 62));
  fillCircle(CC_FAN_X, CC_FAN_Y, 41, rgb565(18, 20, 24));
  fillRect(CC_FAN_X - 8, CC_FAN_Y + 46, CC_FAN_X + 8, CC_FAN_Y + 62, rgb565(50, 54, 62));
  // the status display over the main computer
  fillRect(CONSOLE_X - 140, 290, CONSOLE_X + 140, 372, rgb565(14, 10, 10));
  drawRectOutline(CONSOLE_X - 140, 290, CONSOLE_X + 140, 372, rgb565(110, 116, 128), 3);
  // the main computer (NUCLEAR MISSION's launch computer)
  int x = CONSOLE_X;
  fillRect(x - 90, fy - 64, x + 90, fy - 1, rgb565(78, 84, 96));
  fillRect(x - 94, fy - 70, x + 94, fy - 62, rgb565(120, 128, 142));
  for (int k = 0; k < 8; k++) fillRect(x - 70 + k * 18, fy - 52, x - 58 + k * 18, fy - 44, rgb565(50, 54, 62));   // keys
  fillRect(x - 74, fy - 196, x + 74, fy - 76, rgb565(40, 44, 52));       // monitor
  fillRect(x - 8, fy - 76, x + 8, fy - 70, rgb565(40, 44, 52));
  drawRectOutline(x - 74, fy - 196, x + 74, fy - 76, rgb565(110, 116, 128), 2);
  fillRect(x + 100, fy - 46, x + 104, fy - 1, rgb565(60, 64, 72));      // a printer by the desk, with paper
  fillRect(x + 96, fy - 70, x + 112, fy - 46, rgb565(150, 150, 144));
  fillRect(x + 99, fy - 78, x + 109, fy - 70, rgb565(236, 234, 224));
  // the QUANTUM DRIVE BAY: a tall cabinet, ITEMS_HIDDEN slots (filled each frame), a counter
  int b0 = BAY_X - 100, b1 = BAY_X + 99;
  fillRect(b0, BAY_TOP, b1, fy - 1, rgb565(64, 70, 84));
  fillRect(b0, BAY_TOP, b1, BAY_TOP + 3, rgb565(130, 138, 156));
  fillRect(b1 - 4, BAY_TOP + 4, b1, fy - 1, rgb565(40, 44, 54));
  fillRect(b0 + 8, BAY_TOP + 10, b1 - 8, BAY_TOP + 34, rgb565(20, 24, 32));
  drawVectorText("QUANTUM DRIVE BAY", BAY_X, BAY_TOP + 22, 1.6f, rgb565(90, 220, 255), true);
  fillRect(b0 + 8, BAY_TOP + 42, b1 - 8, BAY_TOP + 196, rgb565(24, 28, 36));   // the slots' panel
  fillRect(BAY_X - 50, BAY_TOP + 206, BAY_X + 50, BAY_TOP + 232, rgb565(10, 14, 12));   // the counter
  drawRectOutline(BAY_X - 50, BAY_TOP + 206, BAY_X + 50, BAY_TOP + 232, rgb565(110, 116, 128), 1);
  hazardBand(b0 + 4, b1 - 6, fy - 14, fy - 6);
}

// ---- the QUANTUMLOOP station (the sub basement's room) and, in stage 2, the
// arrival station (the same, its board saying where you are): a sign and the
// departure board on the left, benches, the power switch, the portal with
// its doors, and the transport's tube running off to the right. What lights
// up and moves is drawn each frame (drawLoopStation).
#define LOOP_RING_Y   520          // the portal's middle
static void paintLoopStation(bool arrival) {
  int fy = ROOM_FLOOR_Y;
  // the walls: concrete panels in steel frames
  fillRect(0, HUD_H, SCREEN_W - 1, PLAY_Y1, rgb565(74, 78, 84));
  for (int x = 0; x < SCREEN_W; x += 128) {
    fillRect(x + 4, 124, x + 123, fy - 8, rgb565(86, 90, 96));
    fillRect(x, 110, x + 3, fy - 1, rgb565(52, 56, 62));
  }
  for (int y = 250; y < fy; y += 140) fillRect(0, y, SCREEN_W - 1, y + 2, rgb565(60, 64, 70));
  // the ceiling: beams, pipes, light panels
  fillRect(0, HUD_H, SCREEN_W - 1, 110, rgb565(34, 38, 46));
  for (int x = 60; x < SCREEN_W; x += 240) fillRect(x, HUD_H, x + 30, 110, rgb565(50, 56, 66));
  paintPipe(64, 16);
  for (int k = 0; k < 3; k++) {
    int lx = 180 + k * 400;
    fillRect(lx, 96, lx + 150, 108, rgb565(240, 244, 250));
    fillRect(lx - 4, 92, lx + 154, 96, rgb565(70, 76, 88));
    glowDisc(lx + 75.0f, 110.0f, 150, 200, 220, 255, 0.12f);
  }
  paintPipe(118, 12);
  // the sign: QUANTUMLOOP TRANSPORT SYSTEM, a swoosh over it
  fillRect(70, 150, 440, 252, rgb565(28, 32, 40));
  drawRectOutline(70, 150, 440, 252, rgb565(110, 116, 128), 2);
  drawVectorTextW("QUANTUMLOOP", 255, 186, 3.9f, rgb565(230, 234, 240), 2.4f);
  drawVectorText("TRANSPORT SYSTEM", 255, 228, 2.0f, rgb565(170, 176, 186), false);
  for (int k = 0; k < 24; k++) {                       // (the swoosh)
    float a0 = 3.4f + k * 0.07f, a1 = a0 + 0.07f;
    drawLineF(300 + cosf(a0) * 130, 214 + sinf(a0) * 46, 300 + cosf(a1) * 130, 214 + sinf(a1) * 46, rgb565(200, 206, 216), 2.0f);
  }
  // the departure board's frame (what it says: drawLoopStation)
  fillRect(110, 290, 380, 452, rgb565(24, 26, 32));
  drawRectOutline(110, 290, 380, 452, rgb565(120, 126, 138), 3);
  // benches, a bin
  for (int b = 0; b < 3; b++) {
    int x0 = 118 + b * 66;
    fillRect(x0, 560, x0 + 60, 604, rgb565(150, 156, 166));
    for (int yy = 566; yy < 600; yy += 7)
      for (int xx = x0 + 5; xx < x0 + 56; xx += 7) fillRect(xx, yy, xx + 2, yy + 2, rgb565(100, 104, 114));
    fillRect(x0 - 2, 604, x0 + 62, 616, rgb565(176, 182, 192));
  }
  fillRect(124, 616, 132, fy - 1, rgb565(80, 84, 92));
  fillRect(304, 616, 312, fy - 1, rgb565(80, 84, 92));
  fillRect(350, 600, 384, fy - 1, rgb565(150, 156, 166));
  fillRect(346, 594, 388, 602, rgb565(176, 182, 192));
  fillRect(356, 610, 378, 616, rgb565(40, 44, 50));
  // the power switch: a pedestal under a sign
  int px = LOOP_POWER_X;
  fillRect(px - 84, 470, px + 84, 500, rgb565(28, 32, 40));
  drawRectOutline(px - 84, 470, px + 84, 500, rgb565(255, 190, 70), 1);
  drawVectorText("QUANTUMLOOP POWER", px, 485, 1.4f, rgb565(255, 190, 70), true);
  fillRect(px - 26, 580, px + 26, fy - 1, rgb565(70, 76, 88));
  fillRect(px - 26, 580, px + 26, 584, rgb565(130, 136, 148));
  int tx[4] = { px - 36, px + 36, px + 30, px - 30 }, ty[4] = { 548, 534, 584, 584 };   // its tilted top
  fillPolygon(tx, ty, 4, rgb565(96, 102, 116));
  hazardBand(px - 26, px + 25, fy - 22, fy - 12);
  // the tube, off to the right
  for (int y = 330; y <= 650; y++) {
    float k = (y - 330) / 320.0f;
    int c = (int)(150 - 90 * fabsf(k - 0.32f) * 1.6f);
    fillRect(980, y, SCREEN_W - 17, y, rgb565(c, c + 4, c + 10));
  }
  for (int x = 1060; x < SCREEN_W - 20; x += 120) {
    fillRect(x, 330, x + 4, 650, rgb565(70, 74, 82));
    fillRect(x + 5, 330, x + 6, 650, rgb565(190, 196, 204));
  }
  drawVectorTextW("QUANTUMLOOP", 1140, 470, 2.6f, rgb565(210, 216, 226), 2.0f);
  // railings in front of it
  fillRect(1010, 596, SCREEN_W - 20, 602, rgb565(230, 180, 40));
  fillRect(1010, 630, SCREEN_W - 20, 634, rgb565(170, 176, 186));
  for (int x = 1016; x < SCREEN_W - 20; x += 80) fillRect(x, 596, x + 5, fy - 1, rgb565(170, 176, 186));
  // the portal: a heavy ring round the doorway
  int cx = LOOP_DOOR_X, cy = LOOP_RING_Y;
  fillCircle(cx, cy, 236, rgb565(46, 50, 58));
  fillCircle(cx, cy, 228, rgb565(150, 156, 166));
  fillCircle(cx - 4, cy - 4, 214, rgb565(176, 182, 192));
  fillCircle(cx, cy, 212, rgb565(120, 126, 136));
  fillCircle(cx, cy, 176, rgb565(60, 64, 72));
  for (int k = 0; k < 16; k++) {
    float a = k * TWO_PI / 16;
    fillCircle(cx + (int)(cosf(a) * 220), cy + (int)(sinf(a) * 220), 3, rgb565(90, 96, 106));
  }
  fillRect(cx - 74, 470, cx + 74, fy - 1, rgb565(40, 44, 52));      // the door frame
  fillRect(cx - 74, 470, cx + 74, 474, rgb565(150, 156, 166));
  fillRect(cx - 100, 384, cx + 100, 462, rgb565(20, 24, 32));        // the sign over it (lit: drawLoopStation)
  drawRectOutline(cx - 100, 384, cx + 100, 462, rgb565(90, 96, 108), 2);
  (void)arrival;
}

// The station, each frame: the portal's lights, the tube's strip, the sign
// and the board, the switch's lamps, the doors (and what is behind them)
static bool atLoopPower();
static bool atLoopDoor();
static void drawLoopStation(bool arrival) {
  int fy = ROOM_FLOOR_Y, cx = LOOP_DOOR_X, cy = LOOP_RING_Y;
  bool blink = fmodf(clockT, 1.0f) < 0.6f;
  float k = loopPowered ? clampv(loopPowerT / 2.0f, 0.0f, 1.0f) : 0.0f;   // (the lights coming on, in turn)
  bool flick = loopPowered && k < 1.0f && fxRnd(0, 4) == 0;
  // the portal's light arcs
  for (int a = 0; a < 4; a++) {
    float on = clampv(k * 4.0f - a, 0.0f, 1.0f);
    float mid = (a == 0 ? 200 : a == 1 ? 140 : a == 2 ? 340 : 40) * 0.017453293f;
    uint16_t c = on > 0.5f && !flick ? rgb565(110, 200, 255) : rgb565(36, 56, 80);
    for (int s2 = -6; s2 < 6; s2++) {
      float a0 = mid + s2 * 0.06f, a1 = a0 + 0.06f;
      drawLineF(cx + cosf(a0) * 194, cy + sinf(a0) * 194, cx + cosf(a1) * 194, cy + sinf(a1) * 194, c, 9.0f);
    }
    if (on > 0.5f && !flick) glowDisc(cx + cosf(mid) * 194, cy + sinf(mid) * 194, 90, 60, 160, 255, 0.35f);
  }
  // the tube's strip
  if (k > 0) {
    int x1 = 980 + (int)((SCREEN_W - 17 - 980) * k);
    fillRect(980, 556, x1, 562, rgb565(90, 190, 255));
    glowDisc((980 + x1) / 2.0f, 559.0f, (x1 - 980) / 2.0f + 20, 40, 140, 255, 0.18f);
  } else {
    fillRect(980, 556, SCREEN_W - 17, 562, rgb565(40, 60, 80));
  }
  // the sign over the doors
  bool lit = k >= 0.5f && !flick;
  drawVectorText("QUANTUMLOOP", cx, 408, 2.3f, lit ? rgb565(230, 240, 255) : rgb565(60, 66, 78), true);
  if (arrival) drawVectorText("ARRIVALS", cx, 440, 2.4f, lit ? rgb565(255, 220, 80) : rgb565(70, 64, 40), true);
  else {
    drawVectorText("STAGE 2", cx - 16, 440, 2.4f, lit ? rgb565(255, 220, 80) : rgb565(70, 64, 40), true);
    for (int c2 = 0; c2 < 2; c2++) {
      int ax = cx + 44 + c2 * 14;
      int xs[3] = { ax, ax + 10, ax }, ys[3] = { 430, 440, 450 };
      fillPolygon(xs, ys, 3, lit ? rgb565(255, 220, 80) : rgb565(70, 64, 40));
    }
  }
  if (lit) glowDisc(cx, 423, 120, 80, 150, 255, 0.15f);
  // the departure board
  fillRect(122, 302, 368, 440, rgb565(8, 14, 30));
  if (arrival) {
    drawVectorText("ARRIVED", 245, 330, 2.6f, rgb565(120, 210, 255), true);
    drawVectorText("STAGE 2", 245, 372, 3.4f, rgb565(255, 220, 80), true);
    if (blink) drawVectorText("QUANTUM LAB", 260, 414, 2.2f, rgb565(120, 255, 160), true);
    int xs[3] = { 156, 172, 172 }, ys[3] = { 414, 404, 424 };
    fillPolygon(xs, ys, 3, rgb565(120, 255, 160));
  } else if (!loopPowered || k < 0.75f) {
    drawVectorText("QUANTUMLOOP", 245, 340, 2.4f, rgb565(120, 210, 255), true);
    if (blink && !loopPowered) drawVectorText("POWER OFFLINE", 245, 392, 2.4f, rgb565(255, 80, 60), true);
  } else {
    drawVectorText("NEXT DEPARTURE", 245, 326, 2.2f, rgb565(120, 210, 255), true);
    drawVectorText("STAGE 2", 245, 366, 3.4f, rgb565(255, 220, 80), true);
    if (blink) drawVectorText("NOW BOARDING", 245, 412, 2.4f, rgb565(120, 255, 160), true);
  }
  for (int y = 302; y < 440; y += 3) blendRect(122, y, 368, y, 0, 0, 0, 0.2f);
  // the power switch: its hand plate and lamps
  int px = LOOP_POWER_X;
  bool on = loopPowered || arrival;
  fillCircle(px, 546, 14, on ? rgb565(60, 220, 110) : rgb565(200, 40, 30));
  fillCircle(px - 4, 542, 4, rgb565(255, 255, 255));
  if (on) glowDisc(px, 546, 50, 60, 255, 120, 0.4f);
  else if (blink) glowDisc(px, 546, 40, 255, 40, 30, 0.3f);
  for (int l = 0; l < 3; l++) fillRect(px - 18 + l * 14, 562, px - 10 + l * 14, 568, on && k * 3 > l ? rgb565(80, 255, 120) : rgb565(60, 30, 26));
  // the doors: behind them, the transport's lit cabin; the two halves slide apart
  int dx0 = cx - 70, dx1 = cx + 70, dy0 = 478;
  fillRect(dx0, dy0, dx1, fy - 1, rgb565(150, 160, 176));
  fillRect(dx0, dy0 + 40, dx1, dy0 + 100, rgb565(20, 40, 70));          // (its window: the tunnel)
  for (int s2 = 0; s2 < 3; s2++) fillRect(dx0 + 10 + s2 * 46, dy0 + 120, dx0 + 40 + s2 * 46, dy0 + 176, rgb565(60, 80, 120));   // (seats)
  int open = (int)(70 * loopDoorT);
  for (int h = 0; h < 2; h++) {
    int x0 = h ? cx + open : dx0, x1 = h ? dx1 : cx - open;
    if (x1 - x0 < 2) continue;
    fillRect(x0, dy0, x1, fy - 1, rgb565(176, 182, 192));
    fillRect(x0, dy0, x1, dy0 + 3, rgb565(220, 226, 234));
    int wx0 = h ? x0 + 12 : x1 - 56, wx1 = h ? x0 + 56 : x1 - 12;   // the windows
    if (wx1 > x1 - 4) wx1 = x1 - 4;
    if (wx0 < x0 + 4) wx0 = x0 + 4;
    if (wx1 > wx0) fillRect(wx0, dy0 + 20, wx1, dy0 + 110, rgb565(40, 60, 80));
    hazardBand(x0, x1, fy - 56, fy - 42);
  }
  if (blink && scene == SC_NONE) {
    if (atLoopPower()) drawVectorText("USE", LOOP_POWER_X, 450, 3.0f, rgb565(255, 220, 120), true);
    if (atLoopDoor() && loopDoorT <= 0) drawVectorText("USE", cx, 362, 3.0f, rgb565(255, 220, 120), true);
  }
  fillCircle(cx - 30, 474, 3, on && loopDoorT >= 0 ? rgb565(80, 255, 140) : rgb565(220, 40, 30));
  fillCircle(cx + 30, 474, 3, on ? rgb565(80, 255, 140) : rgb565(220, 40, 30));
}

// ============================================================ painting: stage 2's lab (NUCLEAR MISSION's LAB look)
// The air-conditioning duct along the top
static void labDuctBand(int x0, int x1, int yd0, int yd1) {
  uint16_t body = rgb565(156, 162, 170), hi = rgb565(196, 202, 210), lo = rgb565(98, 104, 112), flange = rgb565(124, 130, 138);
  fillRect(x0, yd0, x1, yd1, body);
  fillRect(x0, yd0, x1, yd0 + 2, hi);
  fillRect(x0, yd1 - 3, x1, yd1, lo);
  for (int x = x0 + 50; x < x1; x += 110) {
    fillRect(x, yd0 - 2, x + 6, yd1 + 2, flange);
    fillRect(x + 1, yd0 - 2, x + 2, yd1 + 2, hi);
    for (int y = yd0 + 8; y < yd1 - 4; y += 12) {
      fillCircle(x - 5, y, 1, lo);
      fillCircle(x + 11, y, 1, lo);
    }
  }
  for (int x = x0 + 20; x < x1; x += 110) fillRect(x, (yd0 + yd1) / 2, x + 18, (yd0 + yd1) / 2 + 1, lo);
}

// The red and black stripe under it
static void labStripe(int x0, int x1, int y0, int y1) {
  fillRect(x0, y0, x1, y1, rgb565(20, 20, 22));
  int h = y1 - y0 + 1;
  for (int x = x0 - 40; x < x1 + 40; x += 36) {
    int xs[4] = { x, x + 18, x + 18 + h, x + h };
    int ys[4] = { y0, y0, y1 + 1, y1 + 1 };
    for (int k = 0; k < 4; k++) xs[k] = clampv(xs[k], x0, x1 + 1);
    fillPolygon(xs, ys, 4, rgb565(200, 30, 30));
  }
}

// A glass rail: steel posts, glass panels, a chrome top rail (fy: the floor it stands on)
static void labRail(int x0, int x1, int fy) {
  uint16_t glass = rgb565(204, 226, 234), sheen = rgb565(236, 246, 250), post = rgb565(150, 156, 166), chrome = rgb565(222, 226, 232);
  fillRect(x0, fy - 46, x1, fy - 4, glass);
  for (int x = x0 + 10; x < x1 - 10; x += 75) {
    drawLine(x + 8, fy - 8, x + 30, fy - 44, sheen, true);
    drawLine(x + 20, fy - 8, x + 34, fy - 30, sheen, false);
  }
  for (int x = x0; x <= x1; x += 75) fillRect(x, fy - 50, x + 3, fy - 1, post);
  fillRect(x0, fy - 52, x1, fy - 48, chrome);
  fillRect(x0, fy - 52, x1, fy - 51, rgb565(250, 250, 252));
  fillRect(x0, fy - 5, x1, fy - 3, post);
}

// The green first aid cabinet on the wall (its middle at x, y): shut, or open and empty
static void labFirstAid(int x, int y, bool used) {
  fillRect(x - 24, y - 20, x + 24, y + 20, rgb565(20, 130, 70));
  fillRect(x - 24, y - 20, x + 24, y - 18, rgb565(80, 190, 120));
  fillRect(x - 24, y + 17, x + 24, y + 20, rgb565(12, 90, 48));
  if (used) {                                // the door hanging open, nothing inside
    fillRect(x - 20, y - 16, x + 20, y + 16, rgb565(40, 46, 44));
    fillRect(x - 20, y + 2, x + 20, y + 3, rgb565(90, 96, 94));
    int dx[4] = { x - 24, x - 44, x - 44, x - 24 }, dy[4] = { y - 20, y - 26, y + 26, y + 20 };
    fillPolygon(dx, dy, 4, rgb565(30, 150, 84));
    fillRect(x - 38, y - 4, x - 30, y + 4, rgb565(220, 224, 226));
  } else {
    fillRect(x - 4, y - 13, x + 4, y + 13, rgb565(250, 250, 250));
    fillRect(x - 13, y - 4, x + 13, y + 4, rgb565(250, 250, 250));
    fillRect(x + 18, y - 4, x + 21, y + 4, rgb565(200, 206, 210));
  }
  drawVectorText("FIRST AID", x, y + 32, 1.3f, rgb565(20, 110, 60), false);
}

// White wall panels from y0 to y1, x0..x1 (seams, a dado line, the skirting at the bottom)
static void labWall(int x0, int x1, int y0, int y1) {
  uint16_t wall = rgb565(226, 230, 234), seam = rgb565(198, 204, 212), skirt = rgb565(186, 192, 200);
  fillRect(x0, y0, x1, y1, wall);
  for (int x = (x0 / 100 + 1) * 100; x < x1; x += 100) fillRect(x, y0, x + 1, y1, seam);
  if (y1 - y0 > 120) fillRect(x0, y1 - 63, x1, y1 - 62, seam);
  fillRect(x0, y1 - 11, x1, y1, skirt);
}

// The white floor slab at fy (to y1)
static void labFloor(int x0, int x1, int fy, int y1) {
  fillRect(x0, fy, x1, y1, rgb565(196, 200, 208));
  fillRect(x0, fy, x1, fy + 2, rgb565(244, 246, 250));
  fillRect(x0, fy + 11, x1, fy + 13, rgb565(150, 156, 164));
  if (y1 > fy + 14) fillRect(x0, fy + 14, x1, y1, rgb565(90, 96, 106));
}

// The lift's call buttons beside the shaft (x: the plate's middle, fy: the floor)
static void labCallButtons(int x, int fy) {
  fillRect(x - 9, fy - 112, x + 9, fy - 72, rgb565(150, 156, 166));
  fillRect(x - 7, fy - 110, x + 7, fy - 74, rgb565(60, 66, 76));
  fillCircle(x, fy - 101, 4, rgb565(200, 206, 214));
  fillCircle(x, fy - 84, 4, rgb565(200, 206, 214));
  fillCircle(x, fy - 101, 2, rgb565(90, 200, 255));
}

// A room's side (stage 2): a white frame, the doorway (dark) and a green EXIT lamp, or a plain wall
static void paintLabSide(bool right, int doorY, bool door) {
  int bx0 = right ? SCREEN_W - 16 : 0, bx1 = right ? SCREEN_W - 1 : 15;
  fillRect(bx0, HUD_H, bx1, ROOM_FLOOR_Y - 1, rgb565(178, 184, 194));
  fillRect(right ? bx0 : bx1 - 2, HUD_H, right ? bx0 + 2 : bx1, ROOM_FLOOR_Y - 1, rgb565(236, 238, 242));
  if (!door) return;
  int x0 = right ? SCREEN_W - 30 : 0, x1 = right ? SCREEN_W - 1 : 29, top = doorY - DOOR_H;
  fillRect(x0, top, x1, doorY - 1, rgb565(40, 46, 56));
  int fx0 = right ? x0 - 10 : x1 + 1, fx1 = fx0 + 9;
  fillRect(fx0, top - 12, fx1, doorY - 1, rgb565(180, 186, 194));
  fillRect(right ? fx0 : fx1 - 2, top - 12, right ? fx0 + 2 : fx1, doorY - 1, rgb565(236, 238, 242));
  fillRect(x0, top - 12, x1, top - 1, rgb565(180, 186, 194));
  int lx = right ? SCREEN_W - 36 : 6, ly = top - 34;
  fillRect(lx, ly, lx + 30, ly + 16, rgb565(20, 60, 30));
  fillRect(lx + 3, ly + 3, lx + 27, ly + 13, rgb565(60, 220, 110));
}

// ---- the kitchen: cooking on the floor, its stores on the deck above
static void paintFridge(int x0, int x1, int fy) {
  int top = fy - 236;
  fillRect(x0, top, x1, fy - 1, rgb565(196, 202, 210));
  fillRect(x0, top, x1, top + 3, rgb565(232, 236, 240));
  fillRect(x0, top, x0 + 3, fy - 1, rgb565(226, 230, 236));
  fillRect(x1 - 3, top, x1, fy - 1, rgb565(150, 156, 166));
  int mid = (x0 + x1) / 2;
  fillRect(mid - 1, top + 24, mid + 1, fy - 12, rgb565(130, 136, 146));
  fillRect(x0, top + 18, x1, top + 22, rgb565(150, 156, 166));
  fillRect(mid - 12, top + 70, mid - 7, top + 150, rgb565(110, 116, 126));   // the handles
  fillRect(mid + 7, top + 70, mid + 12, top + 150, rgb565(110, 116, 126));
  fillRect(x0 + 14, top + 5, x0 + 50, top + 15, rgb565(20, 30, 40));          // its readout
  drawVectorText("3C", x0 + 32, top + 10, 1.2f, rgb565(90, 220, 255), false);
  fillRect(x0 + 6, fy - 12, x1 - 6, fy - 1, rgb565(60, 64, 72));
}

static void paintRange(int x0, int x1, int fy) {
  int top = fy - 92;
  // the extractor hood above it, its duct up to the deck
  int hx[4] = { x0 - 10, x1 + 10, x1 - 40, x0 + 40 }, hy[4] = { top - 128, top - 128, top - 178, top - 178 };
  fillPolygon(hx, hy, 4, rgb565(178, 184, 194));
  fillRect(x0 - 10, top - 128, x1 + 10, top - 120, rgb565(150, 156, 166));
  fillRect((x0 + x1) / 2 - 40, KITCHEN_DECK_Y + PLAT_T, (x0 + x1) / 2 + 40, top - 178, rgb565(196, 202, 210));
  for (int x = x0 + 4; x < x1 - 4; x += 10) fillRect(x, top - 126, x + 4, top - 122, rgb565(90, 96, 106));
  // the steel splashback, the range
  fillRect(x0 + 6, top - 110, x1 - 6, top - 1, rgb565(206, 212, 220));
  for (int y = top - 100; y < top; y += 22) fillRect(x0 + 6, y, x1 - 6, y, rgb565(176, 182, 192));
  fillRect(x0, top, x1, fy - 1, rgb565(150, 156, 166));
  fillRect(x0, top, x1, top + 8, rgb565(40, 42, 48));
  for (int x = x0 + 20; x < x1 - 10; x += 32) fillCircle(x, top + 20, 5, rgb565(40, 42, 48));   // the knobs
  int ow = (x1 - x0 - 30) / 2;
  for (int k = 0; k < 2; k++) {                // two ovens
    int ox0 = x0 + 10 + k * (ow + 10), ox1 = ox0 + ow;
    fillRect(ox0, top + 34, ox1, fy - 10, rgb565(120, 126, 136));
    fillRect(ox0 + 10, top + 46, ox1 - 10, fy - 22, rgb565(30, 26, 24));
    fillRect(ox0 + 12, top + 48, ox1 - 12, top + 52, rgb565(250, 140, 50));   // the glow inside
    fillRect(ox0 + 6, top + 38, ox1 - 6, top + 41, rgb565(220, 224, 230));    // the handle
  }
  // pans on the burners
  int sx = x0 + 40;
  fillRect(sx - 26, top - 54, sx + 26, top - 1, rgb565(176, 182, 192));        // the stock pot
  fillRect(sx - 30, top - 58, sx + 30, top - 52, rgb565(150, 156, 166));
  fillRect(sx - 34, top - 40, sx - 26, top - 36, rgb565(60, 64, 72));
  fillRect(sx + 26, top - 40, sx + 34, top - 36, rgb565(60, 64, 72));
  int fx = (x0 + x1) / 2 + 10;
  fillRect(fx - 30, top - 12, fx + 30, top - 1, rgb565(40, 42, 48));          // the frying pan
  fillRect(fx + 30, top - 10, fx + 74, top - 6, rgb565(30, 30, 34));
  int px = x1 - 46;
  fillRect(px - 20, top - 30, px + 20, top - 1, rgb565(196, 120, 70));         // a copper saucepan
  fillRect(px - 20, top - 32, px + 20, top - 28, rgb565(226, 150, 96));
  fillRect(px - 52, top - 26, px - 20, top - 22, rgb565(150, 90, 50));
}

static void paintPrepCounter(int x0, int x1, int fy) {
  int top = fy - 82;
  // the rail of utensils on the wall above it
  fillRect(x0 + 10, top - 138, x1 - 10, top - 134, rgb565(160, 166, 176));
  for (int k = 0, x = x0 + 30; x < x1 - 20; x += 36, k++) {
    fillRect(x - 1, top - 134, x + 1, top - 110, rgb565(120, 126, 136));
    if (k % 3 == 0) fillCircle(x, top - 96, 14, rgb565(60, 62, 68));                  // a pan
    else if (k % 3 == 1) { fillRect(x - 2, top - 110, x + 2, top - 70, rgb565(170, 176, 186)); fillCircle(x, top - 70, 7, rgb565(170, 176, 186)); }   // a ladle
    else { fillRect(x - 6, top - 110, x + 6, top - 80, rgb565(196, 202, 210)); fillRect(x - 2, top - 80, x + 2, top - 66, rgb565(80, 56, 36)); }     // a cleaver
  }
  // the steel table: legs, a shelf of pots underneath
  fillRect(x0, top, x1, top + 8, rgb565(214, 220, 228));
  fillRect(x0, top + 6, x1, top + 9, rgb565(150, 156, 166));
  fillRect(x0 + 6, top + 9, x0 + 12, fy - 1, rgb565(150, 156, 166));
  fillRect(x1 - 12, top + 9, x1 - 6, fy - 1, rgb565(150, 156, 166));
  fillRect(x0 + 6, fy - 26, x1 - 6, fy - 22, rgb565(176, 182, 192));
  for (int x = x0 + 30; x < x1 - 30; x += 56) fillRect(x - 18, fy - 52, x + 18, fy - 27, rgb565(186, 192, 202));
  // on top: a chopping board, a knife, vegetables, a bowl
  fillRect(x0 + 30, top - 6, x0 + 110, top - 1, rgb565(176, 132, 84));
  fillRect(x0 + 60, top - 9, x0 + 100, top - 7, rgb565(220, 224, 230));
  fillCircle(x0 + 140, top - 9, 9, rgb565(210, 40, 36));
  fillCircle(x0 + 160, top - 8, 8, rgb565(220, 50, 40));
  fillCircle(x0 + 186, top - 12, 12, rgb565(110, 190, 70));
  int bx = x1 - 50;
  int bxs[4] = { bx - 30, bx + 30, bx + 20, bx - 20 }, bys[4] = { top - 26, top - 26, top - 1, top - 1 };
  fillPolygon(bxs, bys, 4, rgb565(236, 236, 230));
  fillCircle(bx - 8, top - 28, 6, rgb565(240, 190, 60));
  fillCircle(bx + 8, top - 29, 6, rgb565(250, 150, 40));
}

static void paintSink(int x0, int x1, int fy) {
  int top = fy - 90;
  fillRect(x0, top - 150, x1, top - 146, rgb565(176, 182, 192));             // a shelf of plates
  for (int x = x0 + 12; x < x1 - 8; x += 14) fillRect(x, top - 186, x + 6, top - 150, rgb565(246, 246, 242));
  fillRect(x0, top, x1, fy - 1, rgb565(176, 182, 192));                      // the unit, its doors
  fillRect(x0, top, x1, top + 6, rgb565(220, 226, 232));
  int mid = (x0 + x1) / 2;
  fillRect(mid - 1, top + 10, mid + 1, fy - 6, rgb565(140, 146, 156));
  fillRect(mid - 14, top + 30, mid - 9, top + 50, rgb565(110, 116, 126));
  fillRect(mid + 9, top + 30, mid + 14, top + 50, rgb565(110, 116, 126));
  fillRect(x0 + 16, top + 1, x0 + 96, top + 4, rgb565(70, 76, 86));          // the basin
  int tx = x0 + 56;                                                         // the tap
  fillRect(tx - 3, top - 44, tx + 3, top - 1, rgb565(210, 216, 224));
  fillRect(tx - 3, top - 44, tx + 26, top - 38, rgb565(210, 216, 224));
  fillRect(tx + 20, top - 38, tx + 26, top - 30, rgb565(210, 216, 224));
  fillRect(x1 - 70, top - 40, x1 - 10, top - 1, rgb565(150, 156, 166));      // the dish rack
  for (int x = x1 - 64; x < x1 - 14; x += 10) fillRect(x, top - 52, x + 5, top - 4, rgb565(246, 246, 242));
}

static void paintBox(int x0, int y1, int w, int h, int tone) {                 // a cardboard box standing on y1
  int r = 176 - tone * 14, g = 128 - tone * 10, b = 74 - tone * 6;
  fillRect(x0, y1 - h, x0 + w, y1 - 1, rgb565(r, g, b));
  fillRect(x0, y1 - h, x0 + w, y1 - h + 2, rgb565(r + 30, g + 26, b + 20));
  fillRect(x0 + w / 2 - 4, y1 - h, x0 + w / 2 + 4, y1 - h / 2, rgb565(r + 40, g + 36, b + 34));   // the tape
  if (w > 40) fillRect(x0 + 6, y1 - h / 2 + 4, x0 + 26, y1 - h / 2 + 14, rgb565(240, 240, 232));    // a label
}

static void paintRack(int x0, int x1, int fy, int seed) {                     // shelving on the deck, full of stores
  uint16_t post = rgb565(150, 156, 166);
  int top = fy - 186;
  fillRect(x0, top, x0 + 5, fy - 1, post);
  fillRect(x1 - 5, top, x1, fy - 1, post);
  for (int k = 0; k < 3; k++) {
    int sy = fy - 6 - k * 62;
    fillRect(x0, sy, x1, sy + 4, rgb565(176, 182, 192));
    int x = x0 + 10, n = 0;
    while (x < x1 - 30) {
      int what = (seed + k * 3 + n * 7) % 5, w;
      if (what <= 1) { w = 44 + (n % 2) * 14; paintBox(x, sy, w, 40 + (n % 3) * 6, (n + k) % 3); }
      else if (what == 2) {                                                 // tins
        w = 40;
        for (int c = 0; c < 3; c++) {
          fillRect(x + c * 13, sy - 22, x + c * 13 + 11, sy - 1, rgb565(170, 176, 186));
          fillRect(x + c * 13, sy - 16, x + c * 13 + 11, sy - 8, (c + k) % 2 ? rgb565(200, 50, 40) : rgb565(240, 190, 50));
        }
      } else if (what == 3) {                                               // a sack
        w = 46;
        int sx[6] = { x + 4, x + 42, x + 46, x + 40, x + 6, x }, syy[6] = { sy - 50, sy - 50, sy - 20, sy - 1, sy - 1, sy - 20 };
        fillPolygon(sx, syy, 6, rgb565(222, 210, 176));
        drawVectorText(n % 2 ? "RICE" : "FLOUR", x + 23, sy - 26, 1.1f, rgb565(120, 96, 60), false);
      } else {                                                              // jars
        w = 36;
        for (int c = 0; c < 3; c++) {
          fillRect(x + c * 12, sy - 26, x + c * 12 + 10, sy - 1, rgb565(200, 220, 226));
          fillRect(x + c * 12 + 1, sy - 18, x + c * 12 + 9, sy - 2, (c % 2) ? rgb565(200, 90, 40) : rgb565(120, 160, 60));
          fillRect(x + c * 12, sy - 30, x + c * 12 + 10, sy - 26, rgb565(140, 100, 60));
        }
      }
      x += w + 10;
      n++;
    }
  }
}

static void paintKitchen() {
  int dy = KITCHEN_DECK_Y;
  // white tiles under the deck (behind the cooking)
  fillRect(224, dy + PLAT_T, SCREEN_W - 1, ROOM_FLOOR_Y - 1, rgb565(238, 240, 242));
  for (int y = dy + PLAT_T + 30; y < ROOM_FLOOR_Y; y += 30) fillRect(224, y, SCREEN_W - 1, y, rgb565(206, 212, 218));
  for (int x = 240; x < SCREEN_W; x += 30) fillRect(x, dy + PLAT_T, x, ROOM_FLOOR_Y - 1, rgb565(206, 212, 218));
  fillRect(224, ROOM_FLOOR_Y - 11, SCREEN_W - 1, ROOM_FLOOR_Y - 1, rgb565(186, 192, 200));
  for (int x = 300; x < SCREEN_W - 40; x += 230) fillRect(x, dy + PLAT_T + 1, x + 90, dy + PLAT_T + 4, rgb565(250, 252, 255));   // lights under the deck
  paintFridge(250, 400, ROOM_FLOOR_Y);
  paintRange(440, 700, ROOM_FLOOR_Y);
  paintPrepCounter(740, 990, ROOM_FLOOR_Y);
  paintSink(1030, 1190, ROOM_FLOOR_Y);
  // the deck: its stores, then the slab and its glass rail
  paintRack(270, 520, dy, 1);
  paintRack(560, 800, dy, 4);
  paintBox(830, dy, 60, 54, 0);                // boxes stacked on the deck
  paintBox(900, dy, 54, 46, 2);
  paintBox(846, dy - 54, 50, 42, 1);
  fillRect(820, dy - 8, 960, dy - 1, rgb565(150, 110, 70));   // a pallet of bottled water
  for (int x = 912; x < 956; x += 11) {
    fillRect(x, dy - 70, x + 9, dy - 9, rgb565(120, 180, 230));
    fillRect(x + 2, dy - 76, x + 7, dy - 70, rgb565(40, 90, 200));
  }
  paintRack(990, 1240, dy, 2);
  fillRect(224, dy, SCREEN_W - 1, dy + PLAT_T - 1, rgb565(196, 200, 208));
  fillRect(224, dy, SCREEN_W - 1, dy + 2, rgb565(244, 246, 250));
  fillRect(224, dy + PLAT_T - 3, SCREEN_W - 1, dy + PLAT_T - 1, rgb565(150, 156, 166));
  drawVectorText("KITCHEN", 1020, 120, 5.0f, rgb565(60, 72, 96), true);
  drawVectorText("STORES", 1020, 160, 2.2f, rgb565(110, 120, 140), false);
}

// ---- the cleaning room: the front counter on the floor, the office in the middle, the stores at the top
static void paintDeckSlab(int y) {
  fillRect(224, y, SCREEN_W - 1, y + PLAT_T - 1, rgb565(196, 200, 208));
  fillRect(224, y, SCREEN_W - 1, y + 2, rgb565(244, 246, 250));
  fillRect(224, y + PLAT_T - 3, SCREEN_W - 1, y + PLAT_T - 1, rgb565(150, 156, 166));
  for (int x = 300; x < SCREEN_W - 40; x += 230) fillRect(x, y + PLAT_T + 1, x + 90, y + PLAT_T + 4, rgb565(250, 252, 255));   // lights under it
}

static void paintCrates(int x0, int w, int fy, int seed) {                    // wooden crates, stacked
  int cw = w / 2 - 2;
  for (int k = 0; k < 3; k++) {
    int bx = x0 + (k == 2 ? cw / 2 : k * (cw + 4)), by = fy - (k == 2 ? 40 : 0);
    if (k == 2 && seed % 2) continue;
    fillRect(bx, by - 40, bx + cw, by - 1, rgb565(176, 136, 86));
    drawRectOutline(bx, by - 40, bx + cw, by - 1, rgb565(120, 88, 50), 2);
    drawLine(bx + 2, by - 38, bx + cw - 2, by - 3, rgb565(130, 96, 56), true);
    fillRect(bx, by - 22, bx + cw, by - 19, rgb565(130, 96, 56));
  }
}

static void paintShelfUnit(int x0, int x1, int fy, int seed) {                 // a steel shelf unit full of cleaning products
  uint16_t post = rgb565(150, 156, 166);
  int top = fy - 190;
  fillRect(x0, top, x0 + 4, fy - 1, post);
  fillRect(x1 - 4, top, x1, fy - 1, post);
  for (int k = 0; k < 3; k++) {
    int sy = fy - 8 - k * 60;
    fillRect(x0, sy, x1, sy + 4, rgb565(176, 182, 192));
    for (int n = 0, x = x0 + 8; x < x1 - 20; n++) {
      int what = (seed + k * 2 + n) % 4, w;
      if (what == 0) {                                                     // spray bottles
        w = 16;
        uint16_t c = (n + k) % 2 ? rgb565(60, 160, 220) : rgb565(230, 80, 120);
        fillRect(x, sy - 30, x + 12, sy - 1, c);
        fillRect(x + 3, sy - 38, x + 9, sy - 30, rgb565(240, 240, 240));
        fillRect(x + 9, sy - 38, x + 14, sy - 35, rgb565(240, 240, 240));
      } else if (what == 1) {                                              // a jug of bleach
        w = 26;
        fillRect(x, sy - 36, x + 22, sy - 1, rgb565(240, 240, 236));
        fillRect(x + 4, sy - 42, x + 12, sy - 36, rgb565(60, 120, 200));
        fillRect(x + 3, sy - 24, x + 19, sy - 14, rgb565(240, 200, 60));
      } else if (what == 2) {                                              // toilet rolls
        w = 30;
        for (int r = 0; r < 2; r++)
          for (int c2 = 0; c2 < 2; c2++) fillRect(x + c2 * 14, sy - 14 - r * 14, x + c2 * 14 + 12, sy - 3 - r * 14, rgb565(250, 250, 250));
      } else {                                                             // a box of detergent
        w = 28;
        fillRect(x, sy - 34, x + 24, sy - 1, rgb565(240, 130, 40));
        fillRect(x + 3, sy - 26, x + 21, sy - 18, rgb565(250, 250, 250));
      }
      x += w + 6;
    }
  }
}

static void paintBuckets(int x0, int fy) {
  int bx[4] = { x0 + 2, x0 + 26, x0 + 52, 0 }, h[3] = { 34, 30, 40 };
  uint16_t c[3] = { rgb565(220, 50, 40), rgb565(50, 120, 220), rgb565(240, 200, 40) };
  for (int k = 0; k < 3; k++) {
    int xs[4] = { bx[k], bx[k] + 24, bx[k] + 20, bx[k] + 4 }, ys[4] = { fy - h[k], fy - h[k], fy - 1, fy - 1 };
    fillPolygon(xs, ys, 4, c[k]);
    fillRect(bx[k], fy - h[k] - 2, bx[k] + 24, fy - h[k], rgb565(60, 60, 64));
  }
  fillRect(x0 + 58, fy - 56, x0 + 70, fy - 40, rgb565(120, 120, 126));   // the wringer on the yellow one
}

static void paintCounter(int x0, int x1, int fy) {                             // the front counter
  int top = fy - 100;
  fillRect(x0 + 40, top - 104, x1 - 40, top - 30, rgb565(196, 200, 208));      // a notice board behind it
  fillRect(x0 + 44, top - 100, x1 - 44, top - 34, rgb565(176, 136, 96));
  for (int k = 0; k < 6; k++) fillRect(x0 + 60 + k * 36, top - 92 + (k % 2) * 8, x0 + 88 + k * 36, top - 62 + (k % 2) * 8, rgb565(250, 250, 240));
  fillRect(x0, top, x1, top + 10, rgb565(70, 76, 90));                         // the top
  fillRect(x0 + 6, top + 10, x1 - 6, fy - 1, rgb565(60, 140, 160));            // the front
  fillRect(x0 + 6, top + 10, x1 - 6, top + 14, rgb565(90, 180, 196));
  fillRect(x0 + 6, fy - 10, x1 - 6, fy - 1, rgb565(40, 100, 116));
  drawVectorText("CLEANING SERVICES", (x0 + x1) / 2.0f, top + 50, 2.4f, rgb565(250, 252, 255), true);
  fillCircle(x1 - 40, top - 6, 7, rgb565(220, 200, 80));                       // the bell
  fillRect(x1 - 48, top - 2, x1 - 32, top, rgb565(60, 60, 66));
  fillRect(x0 + 30, top - 30, x0 + 70, top - 1, rgb565(240, 240, 236));        // a sign-in book
  fillRect(x0 + 50, top - 30, x0 + 51, top - 1, rgb565(150, 150, 150));
}

static void paintTrolley(int x0, int fy) {                                     // the cleaning trolley
  fillRect(x0 + 6, fy - 90, x0 + 94, fy - 86, rgb565(150, 156, 166));
  fillRect(x0 + 6, fy - 40, x0 + 94, fy - 36, rgb565(150, 156, 166));
  fillRect(x0 + 6, fy - 90, x0 + 10, fy - 12, rgb565(150, 156, 166));
  fillRect(x0 + 90, fy - 90, x0 + 94, fy - 12, rgb565(150, 156, 166));
  fillRect(x0 + 50, fy - 86, x0 + 88, fy - 40, rgb565(40, 40, 44));            // the bin bag
  fillRect(x0 + 14, fy - 110, x0 + 30, fy - 90, rgb565(60, 160, 220));         // bottles on top
  fillRect(x0 + 34, fy - 104, x0 + 46, fy - 90, rgb565(230, 80, 120));
  drawLine(x0 + 4, fy - 12, x0 + 30, fy - 150, rgb565(170, 130, 80), true);    // a broom leaning in it
  fillCircle(x0 + 16, fy - 6, 6, rgb565(30, 30, 34));
  fillCircle(x0 + 84, fy - 6, 6, rgb565(30, 30, 34));
}

static void paintCupboardTall(int cx, int fy) {
  int x0 = cx - 44, x1 = cx + 44, top = fy - 170;
  fillRect(x0, top, x1, fy - 1, rgb565(170, 178, 190));
  fillRect(x0, top, x1, top + 3, rgb565(210, 216, 226));
  fillRect(cx - 1, top + 6, cx + 1, fy - 6, rgb565(120, 128, 140));
  fillRect(cx - 10, top + 70, cx - 6, top + 100, rgb565(90, 96, 106));
  fillRect(cx + 6, top + 70, cx + 10, top + 100, rgb565(90, 96, 106));
  for (int y = top + 14; y < top + 34; y += 5) {                              // vents
    fillRect(x0 + 10, y, cx - 10, y + 1, rgb565(130, 138, 150));
    fillRect(cx + 10, y, x1 - 10, y + 1, rgb565(130, 138, 150));
  }
}

static void paintOfficeDesk(int x0, int x1, int fy) {
  int top = fy - 72;
  fillRect(x0, top, x1, top + 8, rgb565(150, 110, 70));
  fillRect(x0 + 6, top + 8, x0 + 12, fy - 1, rgb565(110, 80, 50));
  fillRect(x1 - 60, top + 8, x1 - 6, fy - 1, rgb565(130, 94, 60));            // its drawers
  fillRect(x1 - 60, top + 30, x1 - 6, top + 31, rgb565(100, 72, 46));
  int mx = (x0 + x1) / 2 - 10;                                               // the computer
  fillRect(mx - 34, top - 52, mx + 34, top - 10, rgb565(40, 44, 52));
  fillRect(mx - 30, top - 48, mx + 30, top - 14, rgb565(30, 90, 120));
  for (int k = 0; k < 4; k++) fillRect(mx - 26, top - 44 + k * 7, mx - 26 + 20 + (k * 13) % 30, top - 42 + k * 7, rgb565(120, 220, 255));
  fillRect(mx - 4, top - 10, mx + 4, top - 1, rgb565(40, 44, 52));
  fillRect(mx - 30, top - 5, mx + 20, top - 1, rgb565(60, 64, 72));            // keyboard
  fillRect(x0 + 16, top - 18, x0 + 40, top - 1, rgb565(240, 240, 236));        // papers, a mug
  fillRect(x1 - 30, top - 14, x1 - 18, top - 1, rgb565(200, 60, 50));
  fillRect(x0 - 30, fy - 40, x0 + 6, fy - 34, rgb565(50, 54, 62));            // the chair
  fillRect(x0 - 30, fy - 80, x0 - 24, fy - 34, rgb565(50, 54, 62));
  fillRect(x0 - 14, fy - 34, x0 - 10, fy - 6, rgb565(90, 96, 106));
  fillRect(x0 - 28, fy - 6, x0 + 4, fy - 3, rgb565(90, 96, 106));
}

static void paintFilingCabinet(int cx, int fy) {
  int x0 = cx - 34, x1 = cx + 34, top = fy - 130;
  fillRect(x0, top, x1, fy - 1, rgb565(150, 160, 172));
  for (int k = 0; k < 4; k++) {
    int y = top + 4 + k * 31;
    fillRect(x0 + 4, y, x1 - 4, y + 27, rgb565(176, 184, 196));
    fillRect(cx - 10, y + 6, cx + 10, y + 9, rgb565(90, 96, 106));
    fillRect(cx - 7, y + 14, cx + 7, y + 21, rgb565(240, 240, 232));
  }
}

static void paintMopRack(int cx, int fy) {                                      // mops and brooms hanging on the wall
  fillRect(cx - 50, fy - 128, cx + 50, fy - 122, rgb565(150, 156, 166));
  for (int k = 0; k < 4; k++) {
    int x = cx - 36 + k * 24;
    fillRect(x - 1, fy - 124, x + 1, fy - 30, rgb565(170, 130, 80));
    if (k % 2 == 0) {                                                        // a mop head
      int xs[4] = { x - 10, x + 10, x + 14, x - 14 }, ys[4] = { fy - 34, fy - 34, fy - 6, fy - 6 };
      fillPolygon(xs, ys, 4, rgb565(232, 232, 222));
      for (int s = -10; s <= 10; s += 5) drawLine(x + s, fy - 30, x + s * 13 / 10, fy - 6, rgb565(200, 200, 190), false);
    } else {                                                                 // a broom head
      fillRect(x - 14, fy - 34, x + 14, fy - 26, rgb565(60, 120, 200));
      for (int s = -12; s <= 12; s += 4) fillRect(x + s, fy - 26, x + s + 2, fy - 12, rgb565(240, 210, 120));
    }
  }
}

static void paintCleaning() {
  paintShelfUnit(250, 400, ROOM_FLOOR_Y, 0);
  paintBuckets(420, ROOM_FLOOR_Y);
  paintCounter(560, 880, ROOM_FLOOR_Y);
  paintShelfUnit(920, 1060, ROOM_FLOOR_Y, 2);
  paintTrolley(1090, ROOM_FLOOR_Y);
  paintDeckSlab(CLEAN_MID_Y);
  paintCupboardTall(325, CLEAN_MID_Y);
  paintCupboardTall(430, CLEAN_MID_Y);
  paintOfficeDesk(560, 740, CLEAN_MID_Y);
  paintFilingCabinet(825, CLEAN_MID_Y);
  paintBox(920, CLEAN_MID_Y, 70, 56, 0);
  paintBox(994, CLEAN_MID_Y, 60, 46, 2);
  paintBox(940, CLEAN_MID_Y - 56, 54, 26, 1);
  paintBox(1100, CLEAN_MID_Y, 56, 42, 1);
  paintBox(1160, CLEAN_MID_Y, 60, 60, 0);
  paintDeckSlab(CLEAN_TOP_Y);
  paintCrates(280, 140, CLEAN_TOP_Y, 0);
  paintBox(470, CLEAN_TOP_Y, 70, 50, 2);
  paintBox(544, CLEAN_TOP_Y, 72, 64, 0);
  paintBox(488, CLEAN_TOP_Y - 50, 46, 18, 1);
  paintMopRack(730, CLEAN_TOP_Y);
  paintCrates(840, 140, CLEAN_TOP_Y, 1);
  paintBox(1040, CLEAN_TOP_Y, 76, 56, 1);
  paintBox(1120, CLEAN_TOP_Y, 70, 44, 2);
  paintBox(1130, CLEAN_TOP_Y - 44, 50, 22, 0);
  drawVectorText("CLEANING", 1040, CLEAN_MID_Y - 170, 4.0f, rgb565(60, 72, 96), true);
}

// ---- the medical bay: the nurse station and one patient downstairs, the ward upstairs
static void paintBed(int cx, int fy) {                                         // a hospital bed (the patient and blanket are drawn over it)
  int x0 = cx - 95, x1 = cx + 95;
  fillRect(x0, fy - 92, x0 + 8, fy - 26, rgb565(170, 178, 190));               // the head end
  fillRect(x0 - 2, fy - 96, x0 + 10, fy - 90, rgb565(200, 206, 216));
  fillRect(x1 - 8, fy - 74, x1, fy - 26, rgb565(170, 178, 190));               // the foot end
  fillRect(x0 + 8, fy - 38, x1 - 8, fy - 28, rgb565(140, 148, 160));           // the frame
  fillRect(x0 + 8, fy - 50, x1 - 8, fy - 38, rgb565(244, 246, 250));           // the mattress
  fillRect(x0 + 12, fy - 60, x0 + 54, fy - 50, rgb565(250, 250, 252));         // the pillow
  for (int k = 0; k < 2; k++) {
    int lx = k ? x1 - 20 : x0 + 14;
    fillRect(lx, fy - 28, lx + 4, fy - 8, rgb565(130, 136, 146));
    fillCircle(lx + 2, fy - 6, 5, rgb565(40, 42, 48));
  }
}

static void paintMonitorStand(int cx, int fy) {                                // a heart monitor on its stand, a drip
  fillRect(cx - 2, fy - 150, cx + 2, fy - 8, rgb565(150, 156, 166));
  fillRect(cx - 18, fy - 8, cx + 18, fy - 4, rgb565(150, 156, 166));
  fillRect(cx - 34, fy - 190, cx + 34, fy - 144, rgb565(50, 54, 62));
  fillRect(cx - 30, fy - 186, cx + 30, fy - 148, rgb565(10, 20, 16));          // (its trace is drawn live)
  int dx = cx + 50;                                                          // the drip
  fillRect(dx - 1, fy - 170, dx + 1, fy - 6, rgb565(170, 176, 186));
  fillRect(dx - 12, fy - 172, dx + 12, fy - 168, rgb565(170, 176, 186));
  fillRect(dx + 4, fy - 166, dx + 18, fy - 140, rgb565(200, 230, 240));
  drawLine(dx + 11, fy - 140, dx - 30, fy - 60, rgb565(200, 220, 230), false);
}

static void paintMachines(int cx, int fy) {                                    // the patient downstairs: a ventilator, a monitor
  fillRect(cx - 40, fy - 96, cx + 40, fy - 1, rgb565(214, 218, 226));
  fillRect(cx - 40, fy - 96, cx + 40, fy - 92, rgb565(240, 242, 246));
  for (int k = 0; k < 3; k++) {
    fillCircle(cx - 22 + k * 22, fy - 70, 8, rgb565(60, 64, 72));
    fillCircle(cx - 22 + k * 22, fy - 70, 5, rgb565(220, 224, 230));
  }
  fillRect(cx - 30, fy - 46, cx + 30, fy - 30, rgb565(40, 120, 200));
  fillCircle(cx - 30, fy - 4, 5, rgb565(40, 42, 48));
  fillCircle(cx + 30, fy - 4, 5, rgb565(40, 42, 48));
  fillRect(cx - 34, fy - 160, cx + 34, fy - 104, rgb565(50, 54, 62));            // the monitor on top
  fillRect(cx - 30, fy - 156, cx + 30, fy - 108, rgb565(10, 20, 16));
  fillRect(cx - 4, fy - 104, cx + 4, fy - 96, rgb565(90, 96, 106));
}

static void paintMedCabinet(int cx, int fy) {                                  // the medicine cabinet: glass doors, bottles
  int x0 = cx - 45, x1 = cx + 45, top = fy - 170;
  fillRect(x0, top, x1, fy - 1, rgb565(236, 238, 242));
  fillRect(x0 + 6, top + 26, x1 - 6, fy - 40, rgb565(196, 222, 232));
  fillRect(cx - 1, top + 26, cx + 1, fy - 40, rgb565(170, 176, 186));
  for (int r = 0; r < 3; r++) {
    int sy = top + 60 + r * 34;
    fillRect(x0 + 6, sy, x1 - 6, sy + 2, rgb565(170, 176, 186));
    for (int k = 0; k < 6; k++) {
      uint16_t c = (k + r) % 3 == 0 ? rgb565(200, 120, 40) : (k + r) % 3 == 1 ? rgb565(240, 240, 240) : rgb565(60, 120, 200);
      fillRect(x0 + 10 + k * 12, sy - 18 + (k % 2) * 4, x0 + 18 + k * 12, sy - 1, c);
    }
  }
  fillRect(x0 + 6, fy - 34, x1 - 6, fy - 6, rgb565(214, 218, 226));             // a drawer
  fillRect(cx - 10, fy - 22, cx + 10, fy - 18, rgb565(120, 126, 136));
  fillRect(cx - 4, top + 5, cx + 4, top + 21, rgb565(220, 40, 40));             // a red cross on top
  fillRect(cx - 8, top + 9, cx + 8, top + 17, rgb565(220, 40, 40));
}

static void paintSeats(int cx, int fy) {                                       // visitors' seats (NUCLEAR MISSION's)
  for (int k = 0; k < 3; k++) {
    int x = cx - 46 + k * 46;
    fillRect(x - 20, fy - 34, x + 20, fy - 26, rgb565(60, 120, 200));
    fillRect(x - 20, fy - 70, x - 16, fy - 26, rgb565(50, 100, 170));
    fillRect(x - 2, fy - 26, x + 2, fy - 1, rgb565(150, 156, 166));
  }
}

static void paintMedical() {
  // downstairs: the seats, the back of the nurse station (its front is drawn over the nurse), a patient and his machines
  paintSeats(320, ROOM_FLOOR_Y);
  int sx0 = 500, sx1 = 780, top = ROOM_FLOOR_Y - 100;
  fillRect(sx0 + 30, top - 120, sx1 - 30, top - 40, rgb565(196, 200, 208));      // a board of rotas behind it
  fillRect(sx0 + 34, top - 116, sx1 - 34, top - 44, rgb565(250, 250, 252));
  for (int r = 0; r < 4; r++) fillRect(sx0 + 44, top - 106 + r * 16, sx1 - 60 - (r % 2) * 40, top - 102 + r * 16, rgb565(100, 140, 200));
  paintBed(960, ROOM_FLOOR_Y);
  paintMachines(1140, ROOM_FLOOR_Y);
  // upstairs: the ward
  paintDeckSlab(MED_DECK_Y);
  for (int k = 0; k < 4; k++) {
    paintMonitorStand(340 + k * 220 - 120, MED_DECK_Y);
    paintBed(340 + k * 220, MED_DECK_Y);
  }
  paintMedCabinet(1180, MED_DECK_Y);
  drawVectorText("MEDICAL BAY", 760, 140, 5.0f, rgb565(60, 72, 96), true);
  fillCircle(560, 140, 26, rgb565(250, 250, 252));
  fillRect(553, 124, 567, 156, rgb565(220, 40, 40));
  fillRect(544, 133, 576, 147, rgb565(220, 40, 40));
}

// ---- the security room: a wall of CCTV screens, the desk (its front over the guard), gun racks, lockers
static void paintGunRack(int cx, int fy) {
  int x0 = cx - 48, x1 = cx + 48, top = fy - 160;
  fillRect(x0, top, x1, fy - 1, rgb565(60, 64, 72));
  fillRect(x0 + 4, top + 4, x1 - 4, fy - 5, rgb565(40, 44, 50));
  fillRect(x0 + 4, top + 24, x1 - 4, top + 30, rgb565(100, 106, 116));          // the rail the guns stand in
  fillRect(x0 + 4, fy - 20, x1 - 4, fy - 14, rgb565(100, 106, 116));
  for (int k = 0; k < 4; k++) {                                              // rifles, upright
    int gx = x0 + 16 + k * 22;
    fillRect(gx - 2, top + 8, gx + 2, fy - 60, rgb565(26, 28, 30));            // barrel
    fillRect(gx - 4, fy - 90, gx + 4, fy - 60, rgb565(36, 38, 40));
    fillRect(gx + 4, fy - 84, gx + 10, fy - 70, rgb565(36, 38, 40));           // magazine
    fillRect(gx - 5, fy - 60, gx + 5, fy - 22, rgb565(96, 70, 44));            // stock
  }
  fillRect(x0 + 30, top - 14, x1 - 30, top - 2, rgb565(240, 196, 40));
}

static void paintSecurityLockers(int x, int fy) {                               // NUCLEAR MISSION's lockers
  for (int k = 0; k < 2; k++) {
    int x0 = x + k * 42;
    fillRect(x0, fy - 150, x0 + 40, fy - 1, rgb565(120, 128, 140));
    fillRect(x0, fy - 150, x0 + 40, fy - 147, rgb565(160, 168, 180));
    for (int v = 0; v < 4; v++) fillRect(x0 + 8, fy - 136 + v * 6, x0 + 32, fy - 134 + v * 6, rgb565(70, 76, 88));
    fillRect(x0 + 32, fy - 90, x0 + 35, fy - 70, rgb565(200, 204, 210));
    fillRect(x0 + 6, fy - 60, x0 + 18, fy - 54, rgb565(240, 240, 236));
  }
}

static void paintSecurityRoom() {
  // the CCTV wall (the pictures are drawn live)
  fillRect(190, 96, 1190, 326, rgb565(40, 44, 52));
  fillRect(190, 96, 1190, 100, rgb565(90, 96, 106));
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 8; c++) fillRect(200 + c * 124, 106 + r * 72, 316 + c * 124, 170 + r * 72, rgb565(16, 18, 22));
  drawVectorText("SECURITY", 690, 350, 4.0f, rgb565(40, 70, 140), true);
  paintGunRack(250, ROOM_FLOOR_Y);
  paintGunRack(370, ROOM_FLOOR_Y);
  paintSecurityLockers(468, ROOM_FLOOR_Y);
  int dx0 = GUARD_DESK_X - 130, dx1 = GUARD_DESK_X + 130, top = ROOM_FLOOR_Y - 100;   // the desk's top: monitors, a radio
  fillRect(dx0 + 20, top - 46, dx0 + 84, top - 6, rgb565(40, 44, 52));
  fillRect(dx0 + 24, top - 42, dx0 + 80, top - 10, rgb565(30, 70, 60));
  fillRect(dx1 - 84, top - 46, dx1 - 20, top - 6, rgb565(40, 44, 52));
  fillRect(dx1 - 80, top - 42, dx1 - 24, top - 10, rgb565(30, 60, 90));
  fillRect(dx0 + 110, top - 22, dx0 + 140, top - 1, rgb565(30, 32, 36));          // the radio
  fillRect(dx0 + 134, top - 40, dx0 + 136, top - 22, rgb565(30, 32, 36));
  // the filing cabinet (NUCLEAR MISSION's), the seats
  int fx = 960;
  fillRect(fx, ROOM_FLOOR_Y - 120, fx + 60, ROOM_FLOOR_Y - 1, rgb565(110, 118, 110));
  for (int k = 0; k < 4; k++) {
    int y = ROOM_FLOOR_Y - 116 + k * 29;
    fillRect(fx + 4, y, fx + 56, y + 25, rgb565(128, 136, 128));
    fillRect(fx + 20, y + 10, fx + 40, y + 13, rgb565(200, 204, 210));
    fillRect(fx + 24, y + 3, fx + 36, y + 7, rgb565(240, 240, 236));
  }
  paintSeats(1130, ROOM_FLOOR_Y);
}

// ---- the store room: three levels of boxes, cupboards, cabinets, racks (nobody in it)
static void paintMetalCabinet(int cx, int fy, int h) {                         // a grey double-door cabinet
  int x0 = cx - 50, x1 = cx + 50, top = fy - h;
  fillRect(x0, top, x1, fy - 1, rgb565(130, 140, 150));
  fillRect(x0, top, x1, top + 3, rgb565(170, 180, 190));
  fillRect(cx - 1, top + 6, cx + 1, fy - 8, rgb565(90, 98, 108));
  fillRect(cx - 9, top + h / 2 - 12, cx - 5, top + h / 2 + 12, rgb565(210, 214, 220));
  fillRect(cx + 5, top + h / 2 - 12, cx + 9, top + h / 2 + 12, rgb565(210, 214, 220));
  fillRect(x0 + 10, top + 12, x0 + 40, top + 22, rgb565(240, 240, 232));      // a label
  fillRect(x0, fy - 8, x1, fy - 1, rgb565(80, 86, 96));
}

static void paintSmallCupboard(int cx, int fy, uint16_t c) {                   // a squat wooden cupboard
  int x0 = cx - 45, x1 = cx + 45, top = fy - 110;
  fillRect(x0, top, x1, fy - 1, c);
  fillRect(x0, top, x1, top + 6, rgb565(200, 160, 110));
  fillRect(cx - 1, top + 10, cx + 1, fy - 6, rgb565(90, 60, 36));
  fillCircle(cx - 8, top + 50, 3, rgb565(220, 200, 120));
  fillCircle(cx + 8, top + 50, 3, rgb565(220, 200, 120));
}

static void paintPallet(int x0, int w, int fy) {                               // a pallet stacked with boxes, shrink-wrapped
  fillRect(x0, fy - 10, x0 + w, fy - 1, rgb565(160, 120, 76));
  for (int x = x0 + 6; x < x0 + w - 8; x += 26) fillRect(x, fy - 10, x + 10, fy - 1, rgb565(110, 80, 50));
  paintBox(x0 + 4, fy - 10, w / 2 - 6, 46, 0);
  paintBox(x0 + w / 2, fy - 10, w / 2 - 4, 46, 1);
  paintBox(x0 + 14, fy - 56, w - 28, 40, 2);
  blendRect(x0 + 2, fy - 98, x0 + w - 2, fy - 10, 220, 230, 240, 0.18f);
}

static void paintStore() {
  // the floor
  paintCupboardTall(120, ROOM_FLOOR_Y);
  paintBox(195, ROOM_FLOOR_Y, 60, 60, 0);
  paintBox(255, ROOM_FLOOR_Y, 50, 70, 2);
  paintBox(205, ROOM_FLOOR_Y - 60, 70, 50, 1);
  paintRack(325, 475, ROOM_FLOOR_Y, 3);
  paintMetalCabinet(580, ROOM_FLOOR_Y, 150);
  paintPallet(670, 140, ROOM_FLOOR_Y);
  paintCupboardTall(900, ROOM_FLOOR_Y);
  // the middle
  paintDeckSlab(STORE_MID_Y);
  paintBox(45, STORE_MID_Y, 64, 56, 1);
  paintBox(110, STORE_MID_Y, 64, 90, 0);
  paintSecurityLockers(218, STORE_MID_Y);
  paintRack(345, 495, STORE_MID_Y, 0);
  paintCrates(530, 140, STORE_MID_Y, 0);
  paintCupboardTall(760, STORE_MID_Y);
  paintBox(850, STORE_MID_Y, 70, 70, 2);
  paintBox(922, STORE_MID_Y, 66, 110, 1);
  // the top
  paintDeckSlab(STORE_TOP_Y);
  paintBox(55, STORE_TOP_Y, 70, 60, 0);
  paintBox(127, STORE_TOP_Y, 76, 70, 2);
  paintCrates(290, 140, STORE_TOP_Y, 1);
  paintSmallCupboard(580, STORE_TOP_Y, rgb565(150, 104, 64));
  paintBox(740, STORE_TOP_Y, 80, 50, 1);
  paintBox(822, STORE_TOP_Y, 78, 66, 0);
  paintBox(760, STORE_TOP_Y - 50, 50, 24, 2);
  drawVectorText("STORE", 1210, 300, 2.6f, rgb565(60, 72, 96), true);
  drawVectorText("ROOM", 1210, 336, 2.6f, rgb565(60, 72, 96), true);
}

// ---- the admin room: the front counter by the door, a desk, a meeting table; a deck of archives at the back
static void paintAdminChair(int cx, int fy, int dir) {
  fillRect(cx - 20, fy - 40, cx + 20, fy - 32, rgb565(50, 60, 90));
  int bx = cx - dir * 18;
  fillRect(bx - 3, fy - 84, bx + 3, fy - 32, rgb565(50, 60, 90));
  fillRect(cx - 2, fy - 32, cx + 2, fy - 8, rgb565(120, 126, 136));
  fillRect(cx - 16, fy - 8, cx + 16, fy - 5, rgb565(120, 126, 136));
}

static void paintAdmin() {
  // the walls: a clock, a notice board, pictures
  fillCircle(560, 150, 26, rgb565(250, 250, 250));
  drawRectOutline(534, 124, 586, 176, rgb565(60, 64, 72), 2);
  drawLine(560, 150, 560, 132, rgb565(30, 30, 34), true);
  drawLine(560, 150, 574, 156, rgb565(30, 30, 34), true);
  fillRect(200, 430, 420, 520, rgb565(176, 136, 96));                           // the notice board behind the counter
  for (int k = 0; k < 5; k++) fillRect(214 + k * 40, 440 + (k % 2) * 12, 244 + k * 40, 480 + (k % 2) * 12, rgb565(250, 250, 240));
  drawVectorText("ENQUIRIES", 310, 410, 2.4f, rgb565(60, 72, 96), true);
  for (int k = 0; k < 2; k++) {                                              // pictures over the meeting table
    int px = 830 + k * 120;
    fillRect(px, 470, px + 80, 530, rgb565(90, 70, 50));
    fillRect(px + 5, 475, px + 75, 525, k ? rgb565(90, 150, 200) : rgb565(110, 170, 100));
  }
  // the front counter, by the door
  int cx0 = 180, cx1 = 440, top = ROOM_FLOOR_Y - 100;
  fillRect(cx0, top, cx1, top + 8, rgb565(170, 126, 80));
  fillRect(cx0 + 6, top + 8, cx1 - 6, ROOM_FLOOR_Y - 1, rgb565(60, 90, 140));
  fillRect(cx0 + 6, top + 8, cx1 - 6, top + 12, rgb565(90, 120, 170));
  drawVectorText("ADMINISTRATION", (cx0 + cx1) / 2.0f, top + 50, 2.2f, rgb565(240, 244, 250), true);
  fillRect(cx0 + 20, top - 14, cx0 + 70, top - 1, rgb565(196, 160, 90));       // an in-tray of papers
  fillRect(cx0 + 24, top - 18, cx0 + 66, top - 14, rgb565(250, 250, 244));
  fillCircle(cx1 - 30, top - 6, 7, rgb565(220, 200, 80));                       // a bell
  fillRect(cx1 - 38, top - 2, cx1 - 22, top, rgb565(60, 60, 66));
  // the desk (its front is drawn over the one sitting at it), his chair
  paintAdminChair(ADMIN_DESK_X + 18, ROOM_FLOOR_Y, -1);
  int dx0 = ADMIN_DESK_X - 90, dtop = ROOM_FLOOR_Y - 72;
  fillRect(dx0 + 20, dtop - 52, dx0 + 88, dtop - 8, rgb565(40, 44, 52));        // the computer, turned to him
  fillRect(dx0 + 24, dtop - 48, dx0 + 84, dtop - 12, rgb565(40, 110, 160));
  fillRect(dx0 + 50, dtop - 8, dx0 + 58, dtop, rgb565(40, 44, 52));
  fillRect(dx0 + 120, dtop - 20, dx0 + 160, dtop, rgb565(240, 240, 236));       // papers, a mug
  fillRect(dx0 + 166, dtop - 14, dx0 + 176, dtop, rgb565(200, 60, 50));
  // the meeting table and its chairs
  int tx0 = 790, tx1 = 990, ttop = ROOM_FLOOR_Y - 70;
  paintAdminChair(tx0 + 10, ROOM_FLOOR_Y, 1);
  paintAdminChair(tx1 - 10, ROOM_FLOOR_Y, -1);
  fillRect(tx0 + 20, ttop, tx1 - 20, ttop + 8, rgb565(170, 126, 80));
  fillRect(tx0 + 30, ttop + 8, tx0 + 38, ROOM_FLOOR_Y - 1, rgb565(130, 94, 60));
  fillRect(tx1 - 38, ttop + 8, tx1 - 30, ROOM_FLOOR_Y - 1, rgb565(130, 94, 60));
  fillRect(tx0 + 60, ttop - 6, tx0 + 100, ttop, rgb565(250, 250, 244));
  fillRect(tx0 + 130, ttop - 16, tx0 + 140, ttop, rgb565(120, 200, 230));       // a jug of water
  paintFilingCabinet(1030, ROOM_FLOOR_Y);
  // the water cooler, a plant
  int wx = 1210;
  fillRect(wx - 18, ROOM_FLOOR_Y - 80, wx + 18, ROOM_FLOOR_Y - 1, rgb565(230, 232, 236));
  fillRect(wx - 14, ROOM_FLOOR_Y - 124, wx + 14, ROOM_FLOOR_Y - 80, rgb565(150, 200, 240));
  fillRect(wx - 6, ROOM_FLOOR_Y - 60, wx + 6, ROOM_FLOOR_Y - 54, rgb565(60, 120, 220));
  fillRect(480, ROOM_FLOOR_Y - 34, 520, ROOM_FLOOR_Y - 1, rgb565(170, 100, 60));
  for (int k = 0; k < 5; k++) fillCircle(488 + k * 6, ROOM_FLOOR_Y - 46 - (k % 2) * 16, 12, rgb565(60, 150, 70));
  // the deck at the back: archive shelves of binders, a cupboard, boxes
  fillRect(700, ADMIN_DECK_Y, 1089, ADMIN_DECK_Y + PLAT_T - 1, rgb565(196, 200, 208));
  fillRect(700, ADMIN_DECK_Y, 1089, ADMIN_DECK_Y + 2, rgb565(244, 246, 250));
  fillRect(700, ADMIN_DECK_Y + PLAT_T - 3, 1089, ADMIN_DECK_Y + PLAT_T - 1, rgb565(150, 156, 166));
  int sx0 = 730, sx1 = 850;
  fillRect(sx0, ADMIN_DECK_Y - 190, sx0 + 4, ADMIN_DECK_Y - 1, rgb565(150, 156, 166));
  fillRect(sx1 - 4, ADMIN_DECK_Y - 190, sx1, ADMIN_DECK_Y - 1, rgb565(150, 156, 166));
  for (int r = 0; r < 3; r++) {
    int sy = ADMIN_DECK_Y - 8 - r * 62;
    fillRect(sx0, sy, sx1, sy + 4, rgb565(176, 182, 192));
    for (int k = 0; k < 9; k++) {
      uint16_t c = (k + r) % 3 == 0 ? rgb565(200, 50, 40) : (k + r) % 3 == 1 ? rgb565(40, 90, 170) : rgb565(240, 200, 60);
      fillRect(sx0 + 8 + k * 12, sy - 48, sx0 + 18 + k * 12, sy - 1, c);
      fillRect(sx0 + 11 + k * 12, sy - 36, sx0 + 15 + k * 12, sy - 30, rgb565(250, 250, 250));
    }
  }
  paintCupboardTall(930, ADMIN_DECK_Y);
  paintBox(1002, ADMIN_DECK_Y, 76, 60, 1);
  drawVectorText("ADMIN", 420, 200, 5.0f, rgb565(60, 72, 96), true);
  drawVectorText("ARCHIVES", 790, ADMIN_DECK_Y - 214, 1.8f, rgb565(110, 120, 140), false);
}

// A stage 2 room: white walls, the duct and stripe along the top; then what is in it (the kitchen; the rest empty for now)
static void paintLabRoom() {
  int idx = curRoom;
  fillRect(0, HUD_H, SCREEN_W - 1, CEIL_Y - 1, rgb565(214, 218, 224));
  for (int x = 40; x < SCREEN_W - 40; x += 180) fillRect(x, CEIL_Y - 8, x + 70, CEIL_Y - 4, rgb565(250, 252, 255));
  labStripe(0, SCREEN_W - 1, CEIL_Y, CEIL_Y + 12);
  bool clean = s2Kind[idx] == LK_CLEANING || s2Kind[idx] == LK_STORE;   // (three levels: no duct)
  if (!clean) labDuctBand(0, SCREEN_W - 1, CEIL_Y + 16, CEIL_Y + 58);   // (the cleaning room: its top level up there instead)
  labWall(0, SCREEN_W - 1, clean ? CEIL_Y + 13 : CEIL_Y + 61, ROOM_FLOOR_Y - 1);
  if (s2Kind[idx] == LK_KITCHEN) {
    paintKitchen();
  } else if (s2Kind[idx] == LK_STORE) {
    paintStore();
  } else if (s2Kind[idx] == LK_ADMIN) {
    paintAdmin();
  } else if (clean) {
    paintCleaning();
  } else if (s2Kind[idx] == LK_MEDICAL) {
    paintMedical();
  } else if (s2Kind[idx] == LK_SECURITY) {
    paintSecurityRoom();
  } else {
    char b[24];
    s2RoomName(idx, b, sizeof(b));
    drawVectorText(b, SCREEN_W / 2, 300, 5.0f, rgb565(60, 72, 96), true);
    snprintf(b, sizeof(b), "ACCESS LEVEL %d", s2Lvl[idx]);
    drawVectorText(b, SCREEN_W / 2, 346, 2.2f, rgb565(110, 120, 140), false);
  }
  for (int i = 0; i < liftCount; i++) {        // a lift pad's shaft: a glass tube in a white frame
    const Lift &l = lifts[i];
    int top = shaftTop(l), bottom = l.stops[l.stopCount - 1];
    fillRect(l.x0, top, l.x1 - 1, bottom - 1, rgb565(184, 206, 218));
    for (int y = top + 20; y < bottom - 20; y += 90) drawLine(l.x0 + 12, y + 40, l.x1 - 14, y, rgb565(222, 238, 246), true);
    fillRect(l.x0, top, l.x0 + 4, bottom - 1, rgb565(236, 238, 242));
    fillRect(l.x1 - 5, top, l.x1 - 1, bottom - 1, rgb565(170, 176, 186));
    fillRect(l.x0 - 4, top - 14, l.x1 + 3, top, rgb565(214, 218, 224));
    fillRect(l.x0 - 4, top - 3, l.x1 + 3, top, rgb565(70, 170, 255));
    for (int k = 0; k < l.stopCount; k++) {
      fillRect(l.x0 - 6, l.stops[k] - 2, l.x0 - 1, l.stops[k] + 2, rgb565(200, 30, 30));
      fillRect(l.x1, l.stops[k] - 2, l.x1 + 5, l.stops[k] + 2, rgb565(200, 30, 30));
    }
  }
  labFloor(0, SCREEN_W - 1, ROOM_FLOOR_Y, PLAY_Y1);
  paintLabSide(false, doorYL, true);
  paintLabSide(true, doorYR, !rooms[idx].noRightDoor);
}

static void paintRoom() {
  gfxTarget = roomBg;
  gfxClipAll();
  if (stage == 2 && curRoom < S2_ROOMS) {    // the lab's rooms
    paintLabRoom();
    gfxTarget = g_frame;
    return;
  }
  if (curRoom >= ROOM_COUNT) {               // QUANTUM CONTROL, the sub basement: their own
    if (curRoom == CONTROL_ROOM) paintControlRoom();
    else paintLoopStation(curRoom == ARRIVAL_ROOM);
    for (int i = 0; i < platCount; i++) paintPlatform(plats[i]);
    paintRoomSide(false, doorYL);
    paintRoomSide(true, doorYR, !rooms[curRoom].noRightDoor);
    gfxTarget = g_frame;
    return;
  }
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
  if (room == CONTROL_ROOM) snprintf(b, sizeof(b), "QUANTUM CONTROL");
  else if (room == SUB_ROOM || room == ARRIVAL_ROOM) snprintf(b, sizeof(b), "QUANTUMLOOP");
  else snprintf(b, sizeof(b), "ROOM %d", room + 1);
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

// ---- stage 2's lift areas: NUCLEAR MISSION's LAB walkways
//                                torso head  armN elbN  armF elbF  legN  kneeN  legF  kneeF  footN footF spin plant lift
static const AgentPose PERSON_IDLE = {  2,   0,   38,  48,   30,  58,    3,   -3,   -3,   -2,     0,    0,   0,   1,   0 };
static const AgentPose PERSON_LIE  = {  0,   0,   20,  10,  -10,  15,    0,    0,    6,   -4,     0,    0, -90,   0, -53 };

// Walking, as NUCLEAR MISSION's people do (t: seconds)
static void personWalk(AgentPose &p, float t) {
  p = PERSON_IDLE;
  float s = sinf(t * 7.0f);
  p.legN = 18 * s; p.kneeN = -10 - 10 * fmaxf(0, s);
  p.legF = -18 * s; p.kneeF = -10 - 10 * fmaxf(0, -s);
  p.armN = -16 * s; p.elbowN = 20; p.armF = 16 * s; p.elbowF = 20;
  p.plant = 1;
}

// The live parts of the lab rooms behind the people: the monitors' traces, the CCTV pictures, the patients
static void drawLabRoomBack() {
  int k = s2Kind[curRoom];
  if (k == LK_MEDICAL) {
    auto ecg = [](int x0, int y0, int w, int h, float ph) {    // a heart trace sweeping across a screen
      float prevY = 0;
      for (int i = 0; i <= w; i += 2) {
        float u = fmodf(i / (float)w + ph, 1.0f), v = fmodf(u * 3.0f, 1.0f);
        float y = (v > 0.45f && v < 0.5f) ? -h * 0.45f : (v > 0.5f && v < 0.55f) ? h * 0.3f : 0.0f;
        if (i) drawLine(x0 + i - 2, y0 + (int)prevY, x0 + i, y0 + (int)y, rgb565(80, 240, 120), false);
        prevY = y;
      }
    };
    ecg(1112, ROOM_FLOOR_Y - 132, 56, 30, clockT * 0.5f);
    for (int b = 0; b < 4; b++) ecg(340 + b * 220 - 150, MED_DECK_Y - 167, 56, 26, clockT * 0.5f + b * 0.27f);
    // the patients, unconscious in their beds, under their blankets
    for (int b = 0; b < 5; b++) {
      int cx = b == 0 ? 960 : 340 + (b - 1) * 220, fy = b == 0 ? ROOM_FLOOR_Y : MED_DECK_Y;
      AgentPose p = PERSON_LIE;
      drawPersonU(p, cx - 6, fy - 48, 1, AGENT_SCALE, PK_PATIENT);
      fillRect(cx - 40, fy - 62, cx + 86, fy - 46, rgb565(150, 190, 220));
      fillRect(cx - 40, fy - 62, cx + 86, fy - 59, rgb565(190, 220, 240));
    }
  } else if (k == LK_SECURITY) {
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < 8; c++) {
        int x0 = 203 + c * 124, y0 = 109 + r * 72, ph = ((int)(clockT * 0.4f) + r * 3 + c) % 5;
        uint16_t bg = ph == 0 ? rgb565(60, 90, 80) : ph == 1 ? rgb565(70, 80, 100) : ph == 2 ? rgb565(90, 90, 80) : ph == 3 ? rgb565(50, 70, 90) : rgb565(80, 96, 110);
        fillRect(x0, y0, x0 + 110, y0 + 58, bg);
        fillRect(x0, y0 + 40, x0 + 110, y0 + 58, rgb565(40, 46, 52));          // a corridor floor
        int px = x0 + 20 + (int)(70 * (0.5f + 0.5f * sinf(clockT * 0.7f + r + c * 1.3f)));
        fillRect(px - 3, y0 + 18, px + 3, y0 + 42, rgb565(30, 34, 40));         // someone walking
        fillCircle(px, y0 + 14, 4, rgb565(30, 34, 40));
        int sl = (int)(fmodf(clockT * 30 + c * 7, 58));
        fillRect(x0, y0 + sl, x0 + 110, y0 + sl, rgb565(140, 150, 160));       // the scan line
        if (((int)(clockT * 2) + c) & 1) fillCircle(x0 + 102, y0 + 6, 2, rgb565(255, 50, 40));
      }
  }
}

// ...and in front of them: the nurse station's front, the security desk's
static void drawLabRoomFront() {
  int k = s2Kind[curRoom];
  if (k == LK_MEDICAL) {
    int x0 = 500, x1 = 780, top = ROOM_FLOOR_Y - 100;
    fillRect(x0, top, x1, top + 8, rgb565(214, 218, 226));
    fillRect(x0 + 6, top + 8, x1 - 6, ROOM_FLOOR_Y - 1, rgb565(244, 246, 250));
    fillRect(x0 + 6, top + 8, x1 - 6, top + 12, rgb565(70, 170, 200));
    fillRect(x0 + 6, ROOM_FLOOR_Y - 10, x1 - 6, ROOM_FLOOR_Y - 1, rgb565(176, 182, 192));
    fillRect(x0 + 30, top + 30, x0 + 44, top + 70, rgb565(220, 40, 40));
    fillRect(x0 + 17, top + 43, x0 + 57, top + 57, rgb565(220, 40, 40));
    drawVectorText("NURSE STATION", (x0 + x1) / 2 + 30, top + 50, 2.2f, rgb565(60, 72, 96), true);
    fillRect(x1 - 70, top - 40, x1 - 20, top - 4, rgb565(50, 54, 62));            // its computer
    fillRect(x1 - 66, top - 36, x1 - 24, top - 8, rgb565(40, 110, 160));
    fillRect(x1 - 48, top - 4, x1 - 42, top, rgb565(50, 54, 62));
  } else if (k == LK_ADMIN) {                  // the desk's front, over the one sitting at it
    int x0 = ADMIN_DESK_X - 90, x1 = ADMIN_DESK_X + 90, top = ROOM_FLOOR_Y - 72;
    fillRect(x0, top, x1, top + 8, rgb565(170, 126, 80));
    fillRect(x0 + 6, top + 8, x1 - 6, ROOM_FLOOR_Y - 1, rgb565(140, 100, 64));
    fillRect(x1 - 70, top + 20, x1 - 14, top + 22, rgb565(110, 78, 48));
    fillRect(x1 - 70, top + 44, x1 - 14, top + 46, rgb565(110, 78, 48));
  } else if (k == LK_SECURITY) {
    int x0 = GUARD_DESK_X - 130, x1 = GUARD_DESK_X + 130, top = ROOM_FLOOR_Y - 100;
    fillRect(x0, top, x1, ROOM_FLOOR_Y - 1, rgb565(40, 70, 140));
    fillRect(x0, top, x1, top + 6, rgb565(226, 230, 236));
    fillRect(x0 + 10, top + 22, x1 - 10, top + 24, rgb565(70, 100, 170));
    drawVectorText("SECURITY", (x0 + x1) / 2.0f, top + 50, 2.6f, rgb565(230, 236, 250), true);
  }
}

static bool binLidUp() {
  return s2Act == 3 && ((binPhase == 2 && binPT >= 0.15f) || (binPhase == 3 && binPT < 0.25f));
}

// The bin (big enough for a man): its front and lid
static void drawBinFront(bool lidUp, int fy) {
  int top = fy - BIN_H, x0 = BIN_X - 40, x1 = BIN_X + 40;
  fillRect(x0, top, x1, fy - 8, rgb565(60, 110, 70));
  fillRect(x0, top, x0 + 4, fy - 8, rgb565(90, 150, 100));
  fillRect(x1 - 4, top, x1, fy - 8, rgb565(40, 80, 50));
  for (int y = top + 16; y < fy - 16; y += 22) fillRect(x0, y, x1, y + 2, rgb565(44, 84, 52));
  fillRect(BIN_X - 18, top + 30, BIN_X + 18, top + 44, rgb565(230, 230, 220));
  drawVectorText("WASTE", BIN_X, top + 37, 1.2f, rgb565(40, 70, 46), false);
  fillCircle(x0 + 12, fy - 7, 7, rgb565(30, 30, 34));
  fillCircle(x1 - 12, fy - 7, 7, rgb565(30, 30, 34));
  if (lidUp) {                               // hinged at the back, standing up
    int lx[4] = { x1 + 4, x1 - 4, x1 - 12, x1 - 4 }, ly[4] = { top, top - 2, top - 80, top - 78 };
    fillPolygon(lx, ly, 4, rgb565(50, 96, 60));
    fillRect(x0, top - 3, x1, top, rgb565(20, 30, 22));
  } else {
    fillRect(x0 - 4, top - 9, x1 + 4, top, rgb565(50, 96, 60));
    fillRect(BIN_X - 10, top - 13, BIN_X + 10, top - 9, rgb565(40, 70, 46));
  }
}

// The maintenance man, on his walkway (fy: its floor on screen)
static void drawWorker(int fy) {
  if (wkState == WK_BINNED || bodyCarried) return;
  AgentPose p = PERSON_IDLE;
  int kind = wkState == WK_STRIPPED ? PK_MAINT_UNDIES : PK_MAINT;
  float t = wkT;
  switch (wkState) {
    case WK_WAIT:                            // waiting for the lift: looking up at it now and then, tapping a foot
      p.head = fmodf(clockT, 5.0f) < 2.0f ? -12 : 4;
      p.armN = 20; p.elbowN = 70; p.armF = 16; p.elbowF = 74;
      p.footN = fmodf(clockT, 0.8f) < 0.4f ? 10 : 0;
      break;
    case WK_SPOT:                            // he sees him: a start, hands up
      p.armN = 150; p.elbowN = 40; p.armF = 140; p.elbowF = 50;
      p.head = -10;
      p.torso = -8;
      break;
    case WK_RUN:
      agentSample(ANIM_RUN, t * (WK_RUN_SPEED / (RUN_SPEED * AGENT_SCALE)) * 1.6f, p);
      break;
    case WK_COWER:                           // cornered: crouched, arms over his head, shaking
      p.legN = 80; p.kneeN = -120; p.legF = 50; p.kneeF = -100;
      p.torso = 30 + 2 * sinf(clockT * 30); p.head = 20;
      p.armN = 150; p.elbowN = 110; p.armF = 140; p.elbowF = 120;
      p.plant = 1;
      break;
    case WK_FALL: {
      float f = fminf(1.0f, t / 0.5f);
      agentLerp(PERSON_IDLE, PERSON_LIE, f * f, p);
      p.spin = -90 * f * f;
      break;
    }
    default:                                 // out cold
      p = PERSON_LIE;
      break;
  }
  drawPersonU(p, wkX, (float)fy, wkFace, AGENT_SCALE, kind);
  if (wkState == WK_SPOT) drawVectorTextW("!", wkX, fy - 196, 5.0f, rgb565(255, 70, 50), 3.0f);
  if (wkState == WK_DOWN || wkState == WK_STRIPPED) {   // out cold: stars going round
    for (int k = 0; k < 3; k++) {
      float a = clockT * 3 + k * 2.09f;
      fillCircle((int)(wkX - wkFace * 70 + cosf(a) * 18), (int)(fy - 40 + sinf(a) * 6), 3, rgb565(255, 230, 90));
    }
  }
}

// The lab's lift car: a glass-backed white cabin
static void drawLabCar() {
  int cx0 = SHAFT_X0 + 8, cx1 = SHAFT_X1 - 9, fy = CAR_FLOOR_Y, top = fy - 192;
  for (int y = top + 12; y < fy; y++) {     // the glass back, lighter at the top
    int k = (y - top) * 40 / 192;
    fillRect(cx0 + 6, y, cx1 - 6, y, rgb565(170 - k, 206 - k, 222 - k));
  }
  drawLine(cx0 + 16, fy - 20, cx0 + 52, top + 30, rgb565(226, 242, 248), true);
  drawLine(cx0 + 34, fy - 20, cx0 + 64, top + 70, rgb565(216, 236, 244), false);
  fillRect(cx0, top, cx1, top + 11, rgb565(238, 240, 244));
  fillRect(cx0, top + 9, cx1, top + 11, rgb565(70, 170, 255));
  fillRect(cx0, top + 12, cx0 + 5, fy - 1, rgb565(232, 236, 240));
  fillRect(cx1 - 5, top + 12, cx1, fy - 1, rgb565(232, 236, 240));
  fillRect(cx0 + 5, top + 12, cx0 + 6, fy - 1, rgb565(170, 176, 186));
  fillRect(cx1 - 6, top + 12, cx1 - 5, fy - 1, rgb565(170, 176, 186));
  fillRect(cx0 + 6, fy - 72, cx1 - 6, fy - 69, rgb565(222, 226, 232));
  int n = 0;                                 // the buttons: one a stop
  for (int f = 0; f <= S2_ARRIVAL; f++) n += s2Stops(curShaft, f);
  fillRect(cx1 - 18, top + 60, cx1 - 8, top + 66 + n * 12, rgb565(40, 44, 52));
  for (int f = 0, k = 0; f <= S2_ARRIVAL; f++) {
    if (!s2Stops(curShaft, f)) continue;
    fillCircle(cx1 - 13, top + 68 + k * 12, 3, f == carFloor ? rgb565(90, 200, 255) : rgb565(140, 146, 156));
    k++;
  }
  fillRect(cx0, fy, cx1, fy + 13, rgb565(206, 210, 218));
  fillRect(cx0, fy, cx1, fy + 2, rgb565(246, 248, 250));
  labStripe(cx0, cx1, fy + 9, fy + 13);
  uint16_t lamp = carMoving ? rgb565(255, 170, 40) : rgb565(70, 170, 255);
  fillRect((cx0 + cx1) / 2 - 8, top + 3, (cx0 + cx1) / 2 + 8, top + 6, lamp);
}

// A door at the far end of a walkway: shut (with the level its reader wants) or open; a sign beside it
static void drawLabDoor(int side, int fy, int cy, int room) {
  bool locked = s2Locked(room);
  int x0 = side ? SCREEN_W - 22 : 0, x1 = side ? SCREEN_W - 1 : 21, top = fy - 168;
  fillRect(side ? x0 - 8 : x1 + 1, top - 10, side ? x0 - 1 : x1 + 8, fy - 1, rgb565(180, 186, 194));
  fillRect(x0, top - 10, x1, top - 1, rgb565(180, 186, 194));
  if (locked) {
    fillRect(x0, top, x1, fy - 1, rgb565(236, 238, 242));
    fillRect(x0, top + 70, x1, top + 82, rgb565(200, 30, 30));
  } else {
    fillRect(x0, top, x1, fy - 1, rgb565(40, 46, 56));
  }
  char b[24];
  int px0 = side ? SCREEN_W - 330 : 70, px1 = side ? SCREEN_W - 70 : 330;   // the sign: what is through it
  if (room == ARRIVAL_ROOM) snprintf(b, sizeof(b), "QUANTUMLOOP");
  else s2RoomName(room, b, sizeof(b));
  fillRect(px0, cy + 28, px1, cy + 66, rgb565(40, 52, 76));
  fillRect(px0 + 3, cy + 31, px1 - 3, cy + 63, rgb565(56, 72, 104));
  drawVectorText(b, (px0 + px1) / 2, cy + 47, 2.6f, rgb565(240, 244, 250), true);
  int ax = side ? px1 + 26 : px0 - 26, ay = cy + 47, d = side ? 1 : -1;   // an arrow to the door
  int axs[3] = { ax + d * 12, ax - d * 6, ax - d * 6 }, ays[3] = { ay, ay - 11, ay + 11 };
  fillPolygon(axs, ays, 3, rgb565(255, 220, 80));
  if (room == ARRIVAL_ROOM) return;
  int lx = side ? x0 - 30 : x1 + 14;         // the card reader beside it
  fillRect(lx, fy - 94, lx + 14, fy - 70, rgb565(50, 54, 62));
  fillRect(lx + 3, fy - 90, lx + 11, fy - 86, locked ? rgb565(255, 50, 40) : rgb565(60, 230, 110));
  snprintf(b, sizeof(b), "L%d", s2Lvl[room]);
  drawVectorText(b, lx + 7, fy - 58, 1.6f, locked ? rgb565(200, 30, 30) : rgb565(30, 150, 70), true);
}

// The lift area in stage 2: dark space, the shaft, and at each of this lift's levels its walkways
// (white walls, duct, the stripe, glass rails), doors with card readers, first aid, bins
static void drawLabShaft() {
  auto screenY = [](int f) { return (int)lroundf(CAR_FLOOR_Y + (f * LEVEL_GAP - carWorldY)); };
  int ts = topStop(curShaft), bs = bottomStop(curShaft);
  fillRect(0, HUD_H, SCREEN_W - 1, PANEL_Y - 1, rgb565(14, 16, 22));
  for (int x = 60; x < SCREEN_W; x += 150) fillRect(x, HUD_H, x + 5, PANEL_Y - 1, rgb565(22, 26, 34));
  int capY = screenY(ts) - CORR_H - 100, pitY = screenY(bs) + 40;
  // the shaft: steel rails down a dark well
  fillRect(SHAFT_X0, HUD_H, SHAFT_X1 - 1, PANEL_Y - 1, rgb565(20, 24, 32));
  fillRect(SHAFT_X0, HUD_H, SHAFT_X0 + 5, PANEL_Y - 1, rgb565(210, 214, 222));
  fillRect(SHAFT_X1 - 6, HUD_H, SHAFT_X1 - 1, PANEL_Y - 1, rgb565(210, 214, 222));
  fillRect(SHAFT_X0 + 6, HUD_H, SHAFT_X0 + 7, PANEL_Y - 1, rgb565(120, 126, 136));
  fillRect(SHAFT_X1 - 8, HUD_H, SHAFT_X1 - 7, PANEL_Y - 1, rgb565(120, 126, 136));
  float off = fmodf(carWorldY, 60.0f);
  for (int k = -1; k < 12; k++) {
    int y = (int)lroundf(HUD_H + k * 60 - off);
    if (y > capY && y < pitY) drawLine(SHAFT_X0 + 10, y, SHAFT_X1 - 11, y, rgb565(36, 42, 54), false);
  }
  for (int L = 0; L <= S2_ARRIVAL; L++) {
    if (!s2Stops(curShaft, L)) continue;
    int fy = screenY(L), cy = fy - CORR_H;
    if (fy + 40 < HUD_H || cy - 100 > PANEL_Y) continue;
    char name[12];
    s2LevelName(L, name, sizeof(name));
    for (int side = 0; side < 2; side++) {
      if (!s2Foyer(curShaft, L, side)) continue;
      int x0 = side ? SHAFT_X1 : 0, x1 = side ? SCREEN_W - 1 : SHAFT_X0 - 1;
      labDuctBand(x0, x1, cy - 92, cy - 38);
      labStripe(x0, x1, cy - 34, cy - 20);
      fillRect(x0, cy - 19, x1, cy, rgb565(214, 218, 224));
      fillRect(x0, cy - 2, x1, cy, rgb565(170, 176, 184));
      for (int x = x0 + 40; x < x1 - 40; x += 110) fillRect(x, cy - 6, x + 50, cy - 3, rgb565(250, 252, 255));
      labWall(x0, x1, cy + 1, fy - 1);
      int room = roomOff(curShaft, L, side == 1);
      if (room >= 0) drawLabDoor(side, fy, cy, room);
      else {                                   // the end of the walkway: a wall
        int ex = side ? SCREEN_W - 14 : 0;
        fillRect(ex, cy + 1, ex + 13, fy - 1, rgb565(186, 192, 200));
        fillRect(side ? ex : ex + 11, cy + 1, side ? ex + 2 : ex + 13, fy - 1, rgb565(236, 238, 242));
      }
      labRail(x0, x1, fy);
      labFloor(x0, x1, fy, fy + 22);
      for (int x = x0; x < x1; x += 40) fillRect(x, fy + 22, x + 3, fy + 34, rgb565(70, 76, 86));
      drawVectorText(name, side ? SHAFT_X1 + 70 : SHAFT_X0 - 70, cy + 34, 5.0f, rgb565(60, 72, 96), true);
      labCallButtons(side ? SHAFT_X1 + 28 : SHAFT_X0 - 28, fy);
      if (side == 0 && L != S2_ARRIVAL) labFirstAid(AID_X, fy - 92, s2Aid[curShaft][L]);
      if (side == 0 && curShaft == 0 && L != S2_ARRIVAL) drawBinFront(L == wkFloor && binLidUp(), fy);
    }
    // the shaft's edges at this floor (red)
    if (s2Foyer(curShaft, L, 0)) fillRect(SHAFT_X0 - 8, fy, SHAFT_X0 - 1, fy + 5, rgb565(200, 30, 30));
    fillRect(SHAFT_X1, fy, SHAFT_X1 + 7, fy + 5, rgb565(200, 30, 30));
    if (curShaft == 0 && L == wkFloor) drawWorker(fy);
  }
  // the head of the shaft and its bottom
  if (capY >= HUD_H) {
    fillRect(SHAFT_X0, HUD_H, SHAFT_X1 - 1, capY, rgb565(150, 156, 166));
    labStripe(SHAFT_X0, SHAFT_X1 - 1, capY - 12, capY);
  }
  if (pitY <= PANEL_Y) {
    fillRect(SHAFT_X0, pitY, SHAFT_X1 - 1, PANEL_Y - 1, rgb565(30, 32, 38));
    fillRect(SHAFT_X0, pitY, SHAFT_X1 - 1, pitY + 3, rgb565(80, 86, 96));
  }
  int cableTop = capY > HUD_H ? capY : HUD_H;
  drawLine(SHAFT_X0 + 30, cableTop, SHAFT_X0 + 30, CAR_FLOOR_Y - 192, rgb565(190, 196, 206), false);
  drawLine(SHAFT_X1 - 31, cableTop, SHAFT_X1 - 31, CAR_FLOOR_Y - 192, rgb565(190, 196, 206), false);
  drawLabCar();
}

// A door on this side of the shaft at the car's floor? (side 0 = left)
static bool sideDoor(int side) { return roomOff(curShaft, carFloor, side == 1) >= 0; }

// The shaft: walls on both sides, and a catwalk out to each door - only at
// the floors where there is one. The car stays put on screen; all this scrolls.
static void drawShaftScene() {
  if (stage == 2) {                    // the QUANTUM LAB's lift areas
    drawLabShaft();
    return;
  }
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
  for (int L = 0; L <= SUB_FLOOR; L++) {
    int fy = screenY(L);
    if (fy < HUD_H - 40 || fy - CORR_H - 20 > PLAY_Y1) continue;
    int cy = fy - CORR_H;
    char name[8];
    if (stage == 2) snprintf(name, sizeof(name), "Q%d", L + 1);
    else if (L == SUB_FLOOR) snprintf(name, sizeof(name), "SB");
    else snprintf(name, sizeof(name), "%d", L + 1);
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
      if (stage == 1 && curShaft == ventShaft && L == ventFloor && (ventX > SHAFT_X1) == (side == 1)) drawVent(fy, cy);
      if (side == 1 && room == SUB_ROOM) {           // the sub basement: the way to the QUANTUMLOOP
        int sx0 = SHAFT_X1 + 150, sx1 = SHAFT_X1 + 410, sy0 = cy + 84, sy1 = cy + 140;
        fillRect(sx0, sy0, sx1, sy1, rgb565(20, 26, 36));
        drawRectOutline(sx0, sy0, sx1, sy1, rgb565(90, 180, 255), 2);
        drawVectorText("QUANTUMLOOP", sx0 + 104, sy0 + 28, 2.8f, rgb565(230, 240, 255), true);
        int ax = sx1 - 44, ay = sy0 + 28;
        int xs[7] = { ax + 26, ax + 6, ax + 6, ax - 18, ax - 18, ax + 6, ax + 6 }, ys[7] = { ay, ay - 20, ay - 9, ay - 9, ay + 9, ay + 9, ay + 20 };
        fillPolygon(xs, ys, 7, rgb565(255, 220, 80));
        glowDisc((sx0 + sx1) / 2.0f, (sy0 + sy1) / 2.0f, 160, 60, 140, 255, 0.12f);
      }
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
// The two rooms that aren't random: one level, just the floor
static const WideRoom controlRoomT = { "QUANTUM CONTROL", 1, { { 0, SCREEN_W, ROOM_FLOOR_Y } }, 0, {}, 0, {}, 0, {}, 640, 0 };
static const WideRoom subRoomT = { "SUB BASEMENT", 1, { { 0, SCREEN_W, ROOM_FLOOR_Y } }, 0, {}, 0, {}, 0, {}, 640, 0 };
// Stage 2's: an empty lab room (one level), the kitchen (the floor, a deck for its stores, a lift pad up to it)
static const WideRoom labRoomT = { "LAB", 1, { { 0, SCREEN_W, ROOM_FLOOR_Y } }, 0, {}, 0, {}, 0, {}, 640, 300 };
static const WideRoom kitchenT = { "KITCHEN", 2, { { 0, SCREEN_W, ROOM_FLOOR_Y }, { 220, SCREEN_W, KITCHEN_DECK_Y } },
                                   1, { { 140, 220, { KITCHEN_DECK_Y, ROOM_FLOOR_Y }, 2 } }, 0, {}, 0, {}, 1020, 120 };
static const WideRoom cleaningT = { "CLEANING", 3, { { 0, SCREEN_W, ROOM_FLOOR_Y }, { 220, SCREEN_W, CLEAN_MID_Y }, { 220, SCREEN_W, CLEAN_TOP_Y } },
                                    1, { { 140, 220, { CLEAN_TOP_Y, CLEAN_MID_Y, ROOM_FLOOR_Y }, 3 } }, 0, {}, 0, {}, 640, 100 };
static const WideRoom medicalT = { "MEDICAL BAY", 2, { { 0, SCREEN_W, ROOM_FLOOR_Y }, { 220, SCREEN_W, MED_DECK_Y } },
                                   1, { { 140, 220, { MED_DECK_Y, ROOM_FLOOR_Y }, 2 } }, 0, {}, 0, {}, 760, 140 };
static const WideRoom storeT = { "STORE ROOM", 3, { { 0, SCREEN_W, ROOM_FLOOR_Y }, { 0, 1060, STORE_MID_Y }, { 0, 1060, STORE_TOP_Y } },
                                 1, { { 1060, 1140, { STORE_TOP_Y, STORE_MID_Y, ROOM_FLOOR_Y }, 3 } }, 0, {}, 0, {}, 1180, 120 };
static const WideRoom adminT = { "ADMIN", 2, { { 0, SCREEN_W, ROOM_FLOOR_Y }, { 700, 1090, ADMIN_DECK_Y } },
                                 1, { { 1090, 1170, { ADMIN_DECK_Y, ROOM_FLOOR_Y }, 2 } }, 0, {}, 0, {}, 880, 120 };
static const WideRoom &roomTemplate(int idx) {
  if (idx == CONTROL_ROOM) return controlRoomT;
  if (idx == SUB_ROOM || idx == ARRIVAL_ROOM) return subRoomT;
  if (stage == 2 && idx < S2_ROOMS) return s2Kind[idx] == LK_KITCHEN ? kitchenT : s2Kind[idx] == LK_CLEANING ? cleaningT : s2Kind[idx] == LK_MEDICAL ? medicalT :
                                           s2Kind[idx] == LK_STORE ? storeT : s2Kind[idx] == LK_ADMIN ? adminT : labRoomT;
  return wideRooms[rooms[idx].tpl];
}

static void buildRoom(int idx, bool fromLeft) {
  const RoomInfo &ri = rooms[idx];
  const WideRoom &t = roomTemplate(idx);
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
  if (stage == 2 && idx < S2_ROOMS) ornCount = 0;   // (the lab: none of stage 1's lights and fans)
  else placeOrnaments(ri);
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
  // its scan: the beam turning right round, starting pointing away from the door he came in by
  drScan = true;
  drAlerted = false;
  bool onRight = drX > SCREEN_W / 2;
  drScanAng = onRight ? 90.0f : -90.0f;
  drScanDir = onRight ? 1.0f : -1.0f;
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].life = 0;
}

static void computeBonuses();

static void enterRoom(int idx, bool fromLeft) {
  mode = MODE_ROOM;
  curRoom = idx;
  buildRoom(idx, fromLeft);
  paintRoom();
  rooms[idx].visited = true;
  if (idx == SUB_ROOM && stage == 1) {     // the QUANTUMLOOP station: stage 1 is over (its bonuses), then stage 2
    computeBonuses();
    scene = SC_STAGE_CLEAR;
    sceneT = 0;
    sfxPlay(SFX_FANFARE);
  }
  curHazard = rooms[idx].hazard;
  leverT = -1;
  powerPanelT = 0;
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
    c.period = k ? CAM2_PERIOD : CAM_PERIOD;
    c.t = c.period * (k ? 0.75f : 0.25f);
    c.angle = c.mid + (c.mid > 0 ? 1 : -1) * CAM_SWING_DEG * sinf(TWO_PI * c.t / c.period);
    c.movingT = 0;
    c.view = 0;
  }
  resetHazardsForEntry(fromLeft);
  // the lab's people: on their paths, walking away from the door he came in by
  npcCount = 0;
  susp = 0;
  suspT = 0;
  s2Recog = false;
  auto addNpc = [&](uint8_t kind, float y, float a, float b, float start) {
    Npc &n = npcs[npcCount++];
    n.kind = kind;
    n.y = y; n.a = a; n.b = b;
    n.dir = fromLeft ? 1 : -1;
    n.x = fromLeft ? a + start : b - start;
    n.t = n.pause = 0;
    n.watching = false;
    n.dead = false;
    n.dieT = 9;
    n.aimT = 0;
    n.sight = NPC_SIGHT;
    n.turns = a == b;
    n.knifeOnly = n.seated = false;
    int slot = npcCount - 1;
    if (s2NpcDead[idx][slot]) {              // killed: where he fell
      n.dead = true;
      n.x = s2NpcDX[idx][slot];
      n.y = s2NpcDY[idx][slot];
      n.dir = s2NpcDDir[idx][slot];
    }
  };
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_KITCHEN) {   // the chef: on one of his two paths
    if (fxRnd(0, 2)) addNpc(PK_CHEF, KITCHEN_DECK_Y, 420, 1180, 140);
    else addNpc(PK_CHEF, ROOM_FLOOR_Y, 330, 1100, 140);
  }
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_CLEANING) {  // two cleaners: the floor and the office
    addNpc(PK_CLEANER, ROOM_FLOOR_Y, 300, 1150, 160);
    addNpc(PK_CLEANER, CLEAN_MID_Y, 380, 1180, 300);
  }
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_MEDICAL) {   // the nurse at her station, the doctor on the ward
    addNpc(PK_NURSE, ROOM_FLOOR_Y, 590, 700, 40);
    addNpc(PK_SCIENTIST, MED_DECK_Y, 300, 1110, 200);
  }
  secAlertT = -1;
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_SECURITY) {  // a guard at the door, one behind the desk, one walking
    float gx = fromLeft ? 150.0f : SCREEN_W - 150.0f;    // the door guard: just inside the door he came in by, facing into the room
    addNpc(PK_SECURITY, ROOM_FLOOR_Y, gx, gx, 0);
    Npc &d = npcs[npcCount - 1];
    if (!d.dead) {
      d.x = gx;
      d.dir = fromLeft ? 1 : -1;
    }
    d.turns = false;
    d.knifeOnly = true;
    d.sight = SCREEN_W;                      // (he sees the whole room: no searching the gun racks under his nose)
    addNpc(PK_SECURITY, ROOM_FLOOR_Y, GUARD_DESK_X, GUARD_DESK_X, 0);
    npcs[npcCount - 1].dir = fromLeft ? 1 : -1;   // (looking away from the door; he turns now and then)
    if (!npcs[npcCount - 1].dead) npcs[npcCount - 1].x = GUARD_DESK_X;
    addNpc(PK_SECURITY, ROOM_FLOOR_Y, 300, 1160, 560);
    bool others = !npcs[1].dead || !npcs[2].dead;
    if (npcs[0].dead && others) secAlertT = SEC_ALERT_SECONDS;   // (back after knifing the door guard: they're waiting for him)
  }
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_ADMIN) {     // one at a desk, one walking
    addNpc(PK_OFFICE, ROOM_FLOOR_Y, ADMIN_DESK_X, ADMIN_DESK_X, 0);
    Npc &d = npcs[npcCount - 1];
    if (!d.dead) { d.x = ADMIN_DESK_X; d.dir = -1; }   // (at his desk, facing the counter)
    d.turns = false;
    d.seated = true;
    addNpc(PK_OFFICE, ROOM_FLOOR_Y, 560, 1040, 200);
  }
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
  if (curRoom == SUB_ROOM) {                // (its only door: back to the lift it is under)
    enterShaft(subShaft, SUB_FLOOR, false);
    return;
  }
  if (curRoom == ARRIVAL_ROOM) {            // stage 2: out to lift A, under its first level
    enterShaft(0, S2_ARRIVAL, false);
    return;
  }
  if (stage == 2) {                         // a lab room: its lift on the left, the next one on the right
    if (right) enterShaft(ri.col + 1, ri.row, true);
    else enterShaft(ri.col, ri.row, false);
    return;
  }
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
  if (mode == MODE_SHAFT) {
    int r = roomOff(curShaft, carFloor, right);
    return !carMoving && r >= 0 && !(stage == 2 && s2Locked(r));   // (stage 2: the card reader)
  }
  if (right && rooms[curRoom].noRightDoor) return false;
  if (!right && curRoom == SUB_ROOM && stage == 2) return false;   // (stage 2 begun: no way back up)
  return fabsf(agY - (right ? doorYR : doorYL)) < 0.5f;
}

// A walkway on this side of the shaft (stage 2: on both sides at most levels, a door or not)
static bool sideWalk(int side) { return stage == 2 ? s2Foyer(curShaft, carFloor, side) : sideDoor(side); }
static float edgeMin() {
  if (mode == MODE_SHAFT && !sideWalk(0)) return SHAFT_X0 + 20;   // no door on this side: the wall
  return 34.0f * myScale;
}
static float edgeMax() {
  if (mode == MODE_SHAFT && !sideWalk(1)) return SHAFT_X1 - 20;
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

static void endGame(int why) {
  if (gameOver == END_NONE) {
    gameOver = (EndReason)why;
    gameOverT = 0;
    if (why == END_TIME) {               // the power fails: the generators wind down, the lights go
      sfxPlay(SFX_SHUTDOWN);
      termOn = false;
      powerPanelT = 0;
    }
    computeBonuses();
  }
}

// The bonuses (end of a stage, end of the game): minutes left, and each thing
// that never happened (since the last stage ended)
static void computeBonuses() {
  int mins = timeLeft > 0 ? (int)(timeLeft / 60) : 0;
  bonusPts[0] = (long)mins * BONUS_PER_MINUTE;
  bonusPts[1] = nShocks == 0 ? BONUS_CLEAN : 0;
  bonusPts[2] = nDroneHits == 0 ? BONUS_CLEAN : 0;
  bonusPts[3] = nPitFalls == 0 ? BONUS_CLEAN : 0;
  bonusPts[4] = nCaught == 0 ? BONUS_CLEAN : 0;
  bonusPts[5] = nKitsUsed == 0 ? BONUS_CLEAN : 0;
  bonusTotal = 0;
  for (int i = 0; i < BONUS_LINES; i++) bonusTotal += bonusPts[i];
  score += bonusTotal;
  nShocks = nDroneHits = nPitFalls = nCaught = nKitsUsed = 0;   // (counted afresh for the next stage)
}

// Time off the clock (the electrified floor, a pit), with a message
static void loseTime(int secs, const char *what) {
  timeLeft -= secs;
  char b[64];
  snprintf(b, sizeof(b), "%s  -%d SECONDS", what, secs);
  showMsg(b, 2.6f);
  if (timeLeft <= 0) {
    timeLeft = 0;
    endGame(END_TIME);
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
  while (next >= 0 && next <= SUB_FLOOR && !isStop(curShaft, next)) next += dir;
  if (next < 0 || next > SUB_FLOOR) return false;
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
    if (fabsf(agY - d.y) < 0.5f && fabsf(agX - d.x) < dW(i) / 2 + 14) return i;
  }
  return -1;
}

// A piece that can't be searched (any more): searched already, or a used first aid kit
static bool pieceDone(int i) {
  if (decor[i].kind == DEC_LAB) return s2Searched[curRoom][i];
  const RoomInfo &ri = rooms[curRoom];
  if (decor[i].kind == DEC_GENERATOR) return ri.genOn;
  if (decor[i].kind == DEC_TERMINAL) return false;      // (it can be logged into again)
  return decor[i].kind == DEC_FIRSTAID ? ri.kitUsed : ri.searched[i];
}
// How far it has been searched (the kit's and the generator's are kept after the equipment's)
static float &pieceProgress(int i) {
  if (decor[i].kind == DEC_LAB) return s2Prog[curRoom][i];
  RoomInfo &ri = rooms[curRoom];
  if (decor[i].kind == DEC_GENERATOR) return ri.progress[MAX_DECOR + 1];
  if (decor[i].kind == DEC_TERMINAL) { static float ft; return ft; }
  return decor[i].kind == DEC_FIRSTAID ? ri.progress[MAX_DECOR] : ri.progress[i];
}
static int generatorsOn() {
  int n = 0;
  for (int i = 0; i < ROOM_COUNT; i++) n += rooms[i].gen && rooms[i].genOn;
  return n;
}

// ============================================================ QUANTUM CONTROL: the drive bay and the main computer
static float bayStepT = 0;
static int bayInserted = 0;
enum TermPage : uint8_t { TP_BOOT, TP_FAIL, TP_MENU, TP_MSG, TP_UNLOCK, TP_LOCKDOWN };
static bool lockdownWasOn = false;          // (the lockdown page: it was already on)
static uint8_t termPage = TP_BOOT;
static int termKind = 0;                   // 0 the main computer, 1 the facility terminal
static int termSel = 0, termMsg = 0;
static float termT = 0;
#define TERM_ITEMS 9
#define TERM_UNLOCK 5                  // (the menu items that matter)
#define TERM_LOCKDOWN 6
static const char *const termItems[TERM_ITEMS] = {
  "QUANTUM CORE STATUS", "TEMPORAL FIELD MONITOR", "PERSONNEL FILES", "SECURITY CAMERA NETWORK",
  "REACTOR COOLANT", "UNLOCK SUB BASEMENT LIFT ACCESS", "QUANTUM LOCKDOWN", "COMMUNICATIONS", "LOG OFF" };
#define BOOT_CHECK 3.0f                // the boot: the drive bay is checked at this point...
#define BOOT_DONE  4.4f                // ...and with all 36 loaded, the menu comes up here

// The sites' news: red words coming out of the middle of the screen at you, growing and fading
// (the game carries on underneath)
static void flyerAdd(const char *text) {   // (a free one, else the oldest)
  int k = 0;
  for (int i = 1; i < FLYERS; i++) if (flyers[i].t > flyers[k].t) k = i;
  for (int i = 0; i < FLYERS; i++) if (!flyers[i].on) { k = i; break; }
  snprintf(flyers[k].text, sizeof(flyers[k].text), "%s", text);
  flyers[k].t = 0;
  flyers[k].on = true;
}
static void updateFlyers(float dt) {
  for (int i = 0; i < FLYERS; i++)
    if (flyers[i].on && (flyers[i].t += dt) >= FLYER_SECONDS) flyers[i].on = false;
}
static void drawFlyers() {
  gfxClip(HUD_H, PLAY_Y1);
  for (int i = 0; i < FLYERS; i++) {
    if (!flyers[i].on) continue;
    float k = flyers[i].t / FLYER_SECONDS;                       // 0..1
    float size = 1.4f / (1.05f - k);                             // coming at him: small in the distance, then huge at the glass
    float a = k < 0.12f ? k / 0.12f : k > 0.6f ? 1.0f - (k - 0.6f) / 0.4f : 1.0f;
    drawVectorTextA(flyers[i].text, SCREEN_W / 2.0f, (HUD_H + PLAY_Y1) / 2.0f, size, 255, 40, 30, 0.85f * a, fmaxf(1.5f, size * 0.55f));
  }
  gfxClipAll();
}

// The launch sites: hacking, launching, landing (in play time)
static void updateNukes(float dt) {
  char b[64];
  if (!lockdown && sitesHacked < NUKE_SITES) {
    hackT += dt;
    if (hackT >= HACK_MINUTES[sitesHacked] * 60) {
      hackT = 0;
      snprintf(b, sizeof(b), "%s HACKED", siteName[sitesHacked]);
      flyerAdd(b);
      sfxPlay(SFX_TICK24);
      sitesHacked++;
    }
  }
  bool launching = sitesHacked == NUKE_SITES || lockdown;
  if (launching && sitesLaunched < sitesHacked) {
    launchT += dt;
    if (launchT >= LAUNCH_MINUTES[sitesLaunched] * 60) {
      launchT = 0;
      landLeft[sitesLaunched] = 60.0f * (LAND_MIN_MINUTES + (LAND_MAX_MINUTES - LAND_MIN_MINUTES) * (rnd32() % 1000) / 1000.0f);
      snprintf(b, sizeof(b), "%s LAUNCHED", siteName[sitesLaunched]);
      flyerAdd(b);
      sfxPlay(SFX_TICK24);
      sfxPlay(SFX_ALARM);
      sitesLaunched++;
    }
  }
  int landed = 0;
  for (int i = 0; i < sitesLaunched; i++) {
    if (landLeft[i] > 0) {
      landLeft[i] -= dt;
      if (landLeft[i] <= 0) {
        snprintf(b, sizeof(b), "%s IMPACT", siteName[i]);
        flyerAdd(b);
        sfxPlay(SFX_SHUTDOWN);
      }
    }
    landed += landLeft[i] <= 0;
  }
  if (launching && sitesHacked > 0 && sitesLaunched == sitesHacked && landed == sitesHacked && gameOver == END_NONE) endGame(END_NUKES);
}

// The left of the panel: the five sites, a box each for HACKED, LAUNCHED, LANDED (yellow, orange,
// red when done; blinking while that stage is under way). No countdowns: he has to learn the timing.
static void drawNukePanel(int x0, int y0, int x1, int y1) {
  (void)y1;
  bool blink = fmodf(clockT, 0.8f) < 0.5f;
  drawVectorText("NUCLEAR SITES", x0 + 14 + vectorTextWidth("NUCLEAR SITES", 1.9f) / 2, y0 + 16, 1.9f, rgb565(255, 220, 80), false);
  if (lockdown) drawVectorText("LOCKDOWN", x1 - 14 - vectorTextWidth("LOCKDOWN", 1.6f) / 2, y0 + 16, 1.6f, rgb565(120, 230, 140), true);
  static const char *const head[3] = { "HACKED", "LAUNCH", "LANDED" };
  const int bx0 = x0 + 150, gap = 8, bw = (x1 - 12 - bx0 - 2 * gap) / 3;
  for (int c = 0; c < 3; c++) drawVectorText(head[c], bx0 + c * (bw + gap) + bw / 2.0f, y0 + 40, 1.15f, rgb565(190, 200, 220), false);
  const uint16_t done[3] = { rgb565(240, 210, 40), rgb565(250, 130, 30), rgb565(230, 40, 30) };
  for (int i = 0; i < NUKE_SITES; i++) {
    int y = y0 + 60 + i * 26;
    bool hacked = i < sitesHacked, launched = i < sitesLaunched, landed = launched && landLeft[i] <= 0;
    bool safe = lockdown && !hacked;
    drawVectorText(siteName[i], x0 + 14 + vectorTextWidth(siteName[i], 1.35f) / 2, y, 1.35f, safe ? rgb565(110, 120, 140) : rgb565(220, 226, 240), false);
    bool st[3] = { hacked, launched, landed };
    int active = -1;
    if (launched && !landed) active = 2;
    else if (hacked && !launched && i == sitesLaunched && (sitesHacked == NUKE_SITES || lockdown)) active = 1;
    else if (!hacked && !lockdown && i == sitesHacked) active = 0;
    for (int c = 0; c < 3; c++) {
      int bx = bx0 + c * (bw + gap);
      uint16_t col = st[c] ? done[c] : (active == c && blink ? done[c] : safe ? rgb565(36, 60, 48) : rgb565(44, 48, 62));
      fillRect(bx, y - 9, bx + bw, y + 9, col);
      drawRectOutline(bx, y - 9, bx + bw, y + 9, rgb565(130, 138, 160), 1);
    }
  }
}

static bool onFloorStill() { return mode == MODE_ROOM && fabsf(agY - ROOM_FLOOR_Y) < 0.5f && agState == ST_IDLE; }
static bool atConsole() { return curRoom == CONTROL_ROOM && onFloorStill() && fabsf(agX - CONSOLE_X) < 80; }
static bool atBay() { return curRoom == CONTROL_ROOM && onFloorStill() && fabsf(agX - BAY_X) < 90; }

static void useBay() {
  char b[64];
  if (drivesLoaded >= ITEMS_HIDDEN) {
    snprintf(b, sizeof(b), "ALL %d QUANTUM DRIVES LOADED", ITEMS_HIDDEN);
    showMsg(b, 2.5f);
    return;
  }
  if (drivesCarried <= 0) {
    snprintf(b, sizeof(b), "NO QUANTUM DRIVES TO INSERT - %d OF %d LOADED", drivesLoaded, ITEMS_HIDDEN);
    showMsg(b, 2.8f);
    sfxPlay(SFX_DENIED);
    return;
  }
  bayBusy = true;                      // (he stands at it while they go in)
  bayStepT = 0;
  bayInserted = 0;
  searchItem = -1;
  setState(ST_USE, 0.18f);
}

static void updateBay(float dt) {      // (in ST_USE, while bayBusy)
  bayStepT += dt;
  while (bayStepT >= 0.12f && drivesCarried > 0) {
    bayStepT -= 0.12f;
    drivesCarried--;
    drivesLoaded++;
    bayInserted++;
    sfxPlay(SFX_CLICK);
  }
  if (drivesCarried > 0) return;
  bayBusy = false;
  char b[64];
  if (drivesLoaded >= ITEMS_HIDDEN) {
    snprintf(b, sizeof(b), "ALL %d QUANTUM DRIVES LOADED", ITEMS_HIDDEN);
    sfxPlay(SFX_FANFARE);
  } else {
    snprintf(b, sizeof(b), "%d INSERTED - %d OF %d LOADED", bayInserted, drivesLoaded, ITEMS_HIDDEN);
  }
  showMsg(b, 3.0f);
  setState(ST_IDLE, 0.18f);
}

static void useConsole() {
  if (generatorsOn() < GENERATORS) {   // no power: the panel, with a generator's picture
    powerPanelT = 5.0f;
    sfxPlay(SFX_DENIED);
    return;
  }
  termOn = true;
  termKind = 0;
  termPage = TP_BOOT;
  termT = 0;
  termSel = 0;
  sfxPlay(SFX_FOUND);
}

// ---- the facility terminal: logged into, DOWNLOAD FACILITY BLUEPRINT (the generators on the maps)
enum FtPage : uint8_t { FT_LOGIN, FT_MENU, FT_DOWNLOAD, FT_DONE, FT_STATUS, FT_LOG, FT_NOPOWER, FT_SCAN, FT_SCAN_DONE };
#define FT_ITEMS 5
static const char *const ftItems[FT_ITEMS] = { "DOWNLOAD FACILITY BLUEPRINT", "QUANTUM DRIVE SCAN", "FACILITY STATUS", "MAINTENANCE LOG", "LOG OFF" };
#define SCAN_ROOM_SECONDS 0.5f               // the drive scan: a room every half second (30 rooms: 15 seconds)
// A room with a quantum drive in it still to find
static bool driveLeftIn(int i) {
  if (i < 0 || i >= ROOM_COUNT) return false;
  for (int k = 0; k < MAX_DECOR; k++)
    if (rooms[i].holds[k] && !rooms[i].searched[k]) return true;
  return false;
}
#define FT_LOGIN_SECONDS 2.4f
#define FT_DOWNLOAD_SECONDS 2.8f
static void useFacilityTerminal() {
  termOn = true;
  termKind = 1;
  termPage = FT_LOGIN;
  termT = 0;
  termSel = 0;
  sfxPlay(SFX_FOUND);
}
static void ftLogOff() {
  termOn = false;
  sfxPlay(SFX_KEY);
  if (blueprint && !bpMapShown) {          // the first time after the download: the map, the generators' rooms marked
    bpMapShown = true;
    mapOn = true;
  }
  if (driveScan && !scanMapShown) {        // ...and after the drive scan: the drives' rooms
    scanMapShown = true;
    mapOn = true;
  }
}
static void updateFacilityTerminal(float dt) {
  float was = termT;
  termT += dt;
  bool go = (pressed & (BTN_USE | BTN_JUMP)) != 0;
  switch (termPage) {
    case FT_LOGIN:
      if ((int)(was * 8) != (int)(termT * 8) && termT < 1.6f) sfxPlay(SFX_KEY);   // (the typing)
      if ((go && termT > 0.3f) || termT >= FT_LOGIN_SECONDS) { termPage = FT_MENU; termT = 0; sfxPlay(SFX_KEY); }
      break;
    case FT_MENU:
      if (pressed & BTN_UP) { termSel = (termSel + FT_ITEMS - 1) % FT_ITEMS; sfxPlay(SFX_KEY); }
      if (pressed & BTN_DOWN) { termSel = (termSel + 1) % FT_ITEMS; sfxPlay(SFX_KEY); }
      if (go && termT > 0.2f) {
        sfxPlay(SFX_KEY);
        termT = 0;
        if (termSel == 0) termPage = blueprint ? FT_DONE : FT_DOWNLOAD;
        else if (termSel == 1) {               // the drive scan: only with all the generators on
          if (generatorsOn() < GENERATORS) { termPage = FT_NOPOWER; sfxPlay(SFX_DENIED); }
          else { termPage = FT_SCAN; sfxPlay(SFX_POWERUP); }
        }
        else if (termSel == 2) termPage = FT_STATUS;
        else if (termSel == 3) termPage = FT_LOG;
        else ftLogOff();
      }
      break;
    case FT_DOWNLOAD:
      if ((int)(was * 5) != (int)(termT * 5)) sfxPlay(SFX_BEEP);
      if (termT >= FT_DOWNLOAD_SECONDS) {
        blueprint = true;
        termPage = FT_DONE;
        termT = 0;
        addScore(SEARCH_SCORE * 5, agX, agY - 200);
        sfxPlay(SFX_FANFARE);
      }
      break;
    case FT_SCAN: {                          // room by room: a beep each, a ping where a drive is still hidden
      int r0 = (int)(was / SCAN_ROOM_SECONDS), r1 = (int)(termT / SCAN_ROOM_SECONDS);
      if (r1 != r0 && r0 < ROOM_COUNT) sfxPlay(driveLeftIn(r0) ? SFX_FOUND : SFX_BEEP);
      if ((int)(was * 1.25f) != (int)(termT * 1.25f)) sfxPlay(SFX_KEY);   // (the sweep)
      if (termT >= ROOM_COUNT * SCAN_ROOM_SECONDS + 0.4f) {
        driveScan = true;
        termPage = FT_SCAN_DONE;
        termT = 0;
        sfxPlay(SFX_FANFARE);
      }
      break;
    }
    default:                                 // a page: back to the menu
      if (go && termT > 0.4f) { termPage = FT_MENU; termT = 0; sfxPlay(SFX_KEY); }
      break;
  }
}

// UNLOCK SUB BASEMENT LIFT ACCESS: a shaft (A to I) whose bottom stop is the
// complex's bottom floor, so its car can go one level further down (if no
// shaft reaches that far, one of those that go deepest)
static void unlockSubBasement() {
  if (subUnlocked) return;
  int best = -1, cand[COLS], n = 0;
  for (int sh = 0; sh < COLS; sh++) {
    int b = bottomStop(sh);
    if (b < 0) continue;
    if (b > best) { best = b; n = 0; }
    if (b == best) cand[n++] = sh;
  }
  subShaft = n > 0 ? cand[fxRnd(0, n)] : 0;
  subUnlocked = true;
  sfxPlay(SFX_FANFARE);
}

static void termChoose(int i) {
  sfxPlay(SFX_KEY);
  termT = 0;
  if (i == TERM_ITEMS - 1) { termOn = false; return; }   // LOG OFF
  if (i == TERM_UNLOCK) {
    unlockSubBasement();
    termPage = TP_UNLOCK;
    return;
  }
  if (i == TERM_LOCKDOWN) {            // QUANTUM LOCKDOWN: the hacked sites are final, no more hacking
    lockdownWasOn = lockdown;
    if (!lockdown) {
      lockdown = true;
      hackT = 0;
      sfxPlay(SFX_POWERUP);
      sfxPlay(SFX_FANFARE);
    }
    termPage = TP_LOCKDOWN;
    return;
  }
  termPage = TP_MSG;
  termMsg = i;
}

static void updateTerminal(float dt) {
  termT += dt;
  bool go = (pressed & (BTN_USE | BTN_JUMP)) != 0;
  switch (termPage) {
    case TP_BOOT:
      if (go && termT > 0.3f && termT < BOOT_CHECK) termT = BOOT_CHECK;   // (skip on to the drive check)
      if (termT >= BOOT_CHECK && drivesLoaded < ITEMS_HIDDEN) {
        termPage = TP_FAIL;
        termT = 0;
        sfxPlay(SFX_DENIED);
      } else if (termT >= BOOT_DONE) {
        termPage = TP_MENU;
        termT = 0;
        sfxPlay(SFX_KEY);
      }
      break;
    case TP_FAIL:
      if (go && termT > 0.5f) termOn = false;
      break;
    case TP_MENU:
      if (pressed & BTN_UP) { termSel = (termSel + TERM_ITEMS - 1) % TERM_ITEMS; sfxPlay(SFX_KEY); }
      if (pressed & BTN_DOWN) { termSel = (termSel + 1) % TERM_ITEMS; sfxPlay(SFX_KEY); }
      if (go && termT > 0.2f) termChoose(termSel);
      break;
    default:                           // a page: back to the menu
      if (go && termT > 0.4f) { termPage = TP_MENU; termT = 0; sfxPlay(SFX_KEY); }
      break;
  }
}

// ---- the QUANTUMLOOP station: the power switch, then the doors (then on board)
static bool atLoopPower() { return curRoom == SUB_ROOM && stage == 2 && onFloorStill() && fabsf(agX - LOOP_POWER_X) < 60; }
static bool atLoopDoor() { return curRoom == SUB_ROOM && stage == 2 && onFloorStill() && fabsf(agX - LOOP_DOOR_X) < 80; }
static void useLoopPower() {
  if (loopPowered) {
    showMsg("THE QUANTUMLOOP HAS POWER", 2.0f);
    return;
  }
  loopPowered = true;
  loopPowerT = 0;
  sfxPlay(SFX_LOOPPOWER);
  showMsg("QUANTUMLOOP POWER ON", 2.5f);
}
static void useLoopDoor() {
  if (!loopPowered) {
    showMsg("NO POWER TO THE QUANTUMLOOP", 2.5f);
    sfxPlay(SFX_DENIED);
    return;
  }
  if (loopDoorDir != 0 || loopDoorT > 0) return;
  loopDoorDir = 1;                     // (open: then he goes on board)
  sfxPlay(SFX_DOORS);
}

// ---- stage 2: card readers, first aid on the walkways, the maintenance man, the chef
// Walked into a door his card won't open: what it needs
static void s2Bump(bool right) {
  if (stage != 2 || mode != MODE_SHAFT || s2BumpT > 0) return;
  int r = roomOff(curShaft, carFloor, right);
  if (r < 0 || !s2Locked(r)) return;
  char b[48];
  snprintf(b, sizeof(b), "ACCESS LEVEL %d REQUIRED", s2Lvl[r]);
  showMsg(b, 2.0f);
  sfxPlay(SFX_DENIED);
  s2BumpT = 1.5f;
}

// Recognised (or shot) in a lab room: out to the lift area, by the door he came in by
static void s2Eject() {
  s2Recog = false;
  susp = 0;
  suspT = 0;
  leaveThroughEdge(entryFacing < 0);
}

// Beside someone in a lab room, with the knife?
static int atNpcToKill() {
  if (!hasKnife || mode != MODE_ROOM || stage != 2) return -1;
  for (int i = 0; i < npcCount; i++) {
    const Npc &n = npcs[i];
    if (!n.dead && n.aimT <= 0 && fabsf(agY - n.y) < 0.5f && fabsf(agX - n.x) < KNIFE_RANGE) return i;
  }
  return -1;
}
static int killTarget = -1;
static bool canFire() {                      // the pistol: in a lab room, standing on something
  return hasGun && stage == 2 && mode == MODE_ROOM && curRoom < S2_ROOMS && gameOver == END_NONE;
}

static bool onWalkway() { return stage == 2 && mode == MODE_SHAFT && !carMoving && fabsf(agY - CAR_FLOOR_Y) < 0.5f; }
static bool atWorker() {                     // close enough to knock him out
  return onWalkway() && curShaft == 0 && carFloor == wkFloor && wkState <= WK_COWER && fabsf(agX - wkX) < KNOCK_RANGE;
}
static bool atBody() {                       // beside him, out cold (to search him, then to bin him)
  return onWalkway() && curShaft == 0 && carFloor == wkFloor && (wkState == WK_DOWN || wkState == WK_STRIPPED) && fabsf(agX - wkX) < 100;
}
static bool atFoyerAid() {
  return onWalkway() && carFloor != S2_ARRIVAL && s2Foyer(curShaft, carFloor, 0) && fabsf(agX - AID_X) < 45;
}
static void useFoyerAid() {
  bool &used = s2Aid[curShaft][carFloor];
  if (used) {
    showMsg("THE FIRST AID CABINET IS EMPTY", 2.0f);
    sfxPlay(SFX_CLUNK);
  } else if (health >= HEALTH_MAX) {
    showMsg("HEALTH FULL", 1.6f);
  } else {
    used = true;
    health = HEALTH_MAX;
    nKitsUsed++;
    showMsg("HEALTH RESTORED", 2.0f);
    sfxPlay(SFX_FANFARE);
  }
}

static void foundCard(int level);
static void startS2Act(int what) {
  s2Act = (uint8_t)what;
  s2ActT = 0;
  binPhase = 0;
  binPT = 0;
  agFacing = wkX < agX ? -1 : 1;
  agState = ST_IDLE;
  agVX = 0;
  sfxPlay(SFX_CLICK);
}

// What he does instead of moving: knocking the maintenance man out, searching him (his card, his
// overalls), and carrying him to the bin (NUCLEAR MISSION's). The pose is set here.
static void updateS2Act(float dt) {
  float before = s2ActT;
  s2ActT += dt;
  float t = s2ActT;
  AgentPose &p = shownPose;
  p = PERSON_IDLE;
  if (s2Act == 1) {                          // the knock out: arm up and down
    if (t < 0.25f) {
      float k = t / 0.25f;
      p.armN = 30 + 140 * k; p.elbowN = 50;
      p.torso = -4 * k;
    } else if (t < 0.42f) {
      float k = (t - 0.25f) / 0.17f;
      p.armN = 170 - 110 * k; p.elbowN = 50 - 40 * k;
      p.torso = -4 + 14 * k;
    } else {
      p.armN = 50; p.elbowN = 20; p.torso = 6;
    }
    if (before < 0.33f && t >= 0.33f) {    // down he goes: the countdown to him being found
      wkState = WK_FALL;
      wkT = 0;
      sfxPlay(SFX_HIT);
      sfxPlay(SFX_CLUNK);
      addScore(500, wkX, CAR_FLOOR_Y - 170);
      bodyT = BODY_FOUND_SECONDS;
    }
    if (t >= 0.9f) {
      s2Act = 0;
      showMsg("HE'LL BE FOUND IN 1 MINUTE - SEARCH HIM", 3.5f);
      setState(ST_IDLE, 0.2f);
    }
  } else if (s2Act == 2) {                   // searching him: crouched over him, then changing
    p.legN = 80; p.kneeN = -120; p.legF = 50; p.kneeF = -100;
    p.torso = 30 + 6 * sinf(t * 12); p.armN = 70 + 30 * sinf(t * 9); p.elbowN = 40; p.armF = 60; p.elbowF = 40;
    p.plant = 1;
    if ((int)(before / SEARCH_BEEP_SECONDS) != (int)(t / SEARCH_BEEP_SECONDS) && t < 1.1f) sfxPlay(SFX_BEEP);
    if (before < 1.1f && t >= 1.1f) {      // his card, his overalls
      foundCard(1);
      s2Disguised = true;
      wkState = WK_STRIPPED;
    }
    if (t >= 1.7f) {
      s2Act = 0;
      addScore(1000, agX, CAR_FLOOR_Y - 170);
      showMsg("DISGUISED AS MAINTENANCE", 4.0f);
      setState(ST_IDLE, 0.2f);
    }
  } else if (s2Act == 4) {                   // the knife: a step in and a thrust
    if (t < 0.18f) {
      float k = t / 0.18f;
      p.armN = 30 + 50 * k; p.elbowN = 90 - 60 * k; p.torso = -4 * k;
    } else if (t < 0.32f) {
      float k = (t - 0.18f) / 0.14f;
      p.armN = 80 + 10 * k; p.elbowN = 30 - 28 * k; p.torso = -4 + 16 * k;
      p.legN = 20 * k; p.kneeN = -10 * k;
    } else {
      p.armN = 70; p.elbowN = 10; p.torso = 10; p.legN = 20; p.kneeN = -10;
    }
    if (before < 0.3f && t >= 0.3f && killTarget >= 0 && killTarget < npcCount) {
      Npc &n = npcs[killTarget];
      n.dead = true;
      n.dieT = 0;
      n.watching = false;
      s2NpcDead[curRoom][killTarget] = true;
      s2NpcDX[curRoom][killTarget] = n.x;
      s2NpcDY[curRoom][killTarget] = n.y;
      s2NpcDDir[curRoom][killTarget] = (int8_t)n.dir;
      sfxPlay(SFX_HIT);
      addScore(500, n.x, n.y - 170);
      susp = 0;
      suspT = 0;
      if (n.knifeOnly && s2Kind[curRoom] == LK_SECURITY) {   // the door guard: the others will open fire
        secAlertT = SEC_ALERT_SECONDS;
        for (int k = 0; k < npcCount; k++) npcs[k].watching = false;
      }
    }
    if (t >= 0.8f) {
      s2Act = 0;
      setState(ST_IDLE, 0.2f);
    }
  } else if (s2Act == 5) {                   // the pistol: arm out, fire
    static const AgentPose AIMP = { 0, -2, 88, 2, 80, 14, 6, -4, -8, -3, 0, 0, 0, 1, 0 };
    p = AIMP;
    if (before < 0.08f && t >= 0.08f) {
      sfxPlay(SFX_SHOT);
      gunFlashT = 0.08f;
      int hit = -1;
      float best = 1e9f;
      for (int k = 0; k < npcCount; k++) {     // the first one in line on his level
        const Npc &n = npcs[k];
        float d = agFacing * (n.x - agX);
        if (n.dead || fabsf(agY - n.y) > 60 || d <= 0 || d > SCREEN_W) continue;
        if (d < best) { best = d; hit = k; }
      }
      if (hit >= 0 && npcs[hit].knifeOnly) {   // (the door guard: it doesn't stop him)
        sfxPlay(SFX_CLUNK);
        showMsg("BULLET-PROOF VEST - ONLY THE KNIFE WILL DO", 2.5f);
      } else if (hit >= 0) {
        Npc &n = npcs[hit];
        n.dead = true;
        n.dieT = 0;
        n.watching = false;
        n.aimT = 0;
        s2NpcDead[curRoom][hit] = true;
        s2NpcDX[curRoom][hit] = n.x;
        s2NpcDY[curRoom][hit] = n.y;
        s2NpcDDir[curRoom][hit] = (int8_t)n.dir;
        sfxPlay(SFX_HIT);
        addScore(500, n.x, n.y - 170);
        susp = 0;
        suspT = 0;
      }
    }
    if (t >= 0.35f) {
      s2Act = 0;
      setState(ST_IDLE, 0.15f);
    }
  } else if (s2Act == 3) {                   // into the bin with him
    binPT += dt;
    const float target = BIN_X - 58;
    switch (binPhase) {
      case 0:                                // crouch and lift him
        agFacing = wkX < agX ? -1 : 1;
        p.legN = 80; p.kneeN = -120; p.legF = 50; p.kneeF = -100;
        p.torso = 30; p.armN = 80; p.elbowN = 30; p.armF = 70; p.elbowF = 30;
        p.plant = 1;
        if (binPT >= 0.7f) { binPhase = 1; binPT = 0; bodyCarried = true; sfxPlay(SFX_CLUNK); }
        break;
      case 1: {                              // carry him over to the bin
        int dir = target > agX ? 1 : -1;
        agFacing = dir;
        agX += dir * 130.0f * dt;
        float sn = sinf(binPT * 9.0f);
        p.legN = 20 * sn; p.kneeN = -12 - 12 * fmaxf(0, sn);
        p.legF = -20 * sn; p.kneeF = -12 - 12 * fmaxf(0, -sn);
        p.armN = 160; p.elbowN = 50; p.armF = 150; p.elbowF = 50;
        p.plant = 1;
        if ((dir > 0 && agX >= target) || (dir < 0 && agX <= target)) { agX = target; binPhase = 2; binPT = 0; }
        if ((int)((binPT - dt) * 4) != (int)(binPT * 4)) sfxPlay(SFX_STEP);
        break;
      }
      case 2:                                // lid up, and in he goes
        agFacing = 1;
        p.armN = 160 - 40 * fminf(1.0f, binPT / 0.8f); p.elbowN = 30; p.armF = 150 - 40 * fminf(1.0f, binPT / 0.8f); p.elbowF = 30;
        p.torso = 10 * fminf(1.0f, binPT / 0.8f);
        if (binPT - dt < 0.15f && binPT >= 0.15f) sfxPlay(SFX_CLUNK);
        if (binPT >= 0.85f && bodyCarried) { bodyCarried = false; wkState = WK_BINNED; sfxPlay(SFX_LAND); }
        if (binPT >= 1.0f) { binPhase = 3; binPT = 0; }
        break;
      default:                               // the lid comes down: the countdown is the mission's again
        p.armN = 60; p.elbowN = 20; p.torso = 4;
        if (binPT - dt < 0.25f && binPT >= 0.25f) sfxPlay(SFX_CLUNK);
        if (binPT >= 0.6f) {
          s2Act = 0;
          bodyT = -1;
          addScore(500, BIN_X, CAR_FLOOR_Y - 150);
          showMsg("HIDDEN IN THE BIN - NOBODY WILL FIND HIM", 3.0f);
          setState(ST_IDLE, 0.2f);
        }
        break;
    }
  }
  held = pressed = 0;
}

// The maintenance man: waiting for lift A; when the car stops at his level he runs for the end of
// the walkway and cowers there
static void updateWorker(float dt) {
  wkT += dt;
  bool here = mode == MODE_SHAFT && curShaft == 0 && carFloor == wkFloor && !carMoving;
  switch (wkState) {
    case WK_WAIT:
      if (here) {
        wkState = WK_SPOT;
        wkT = 0;
        wkFace = 1;
      }
      break;
    case WK_SPOT:
      if (wkT >= 0.45f) {
        wkState = WK_RUN;
        wkT = 0;
        wkFace = -1;
      }
      break;
    case WK_RUN: {
      float p0 = fmodf((wkT - dt) * 2.6f, 1.0f), p1 = fmodf(wkT * 2.6f, 1.0f);
      if (p1 < p0 && here) sfxPlay(SFX_STEP);
      wkX -= WK_RUN_SPEED * dt;
      if (wkX <= WK_CORNER_X) {
        wkX = WK_CORNER_X;
        wkState = WK_COWER;
        wkT = 0;
        wkFace = 1;
      }
      break;
    }
    case WK_FALL:
      if (wkT >= 0.5f) wkState = WK_DOWN;
      break;
  }
}

// The lab's people: back and forth along their paths, a pause at each end (working); stopped while
// they're suspicious of him
static void updateNpcs(float dt) {
  if (secAlertT >= 0 && mode == MODE_ROOM) {   // the security room on alert: the guards turn to him, then open fire
    int shooter = -1;
    bool aiming = false;
    for (int i = 0; i < npcCount; i++) {
      Npc &n = npcs[i];
      if (n.dead || n.knifeOnly || n.kind != PK_SECURITY) continue;
      n.dir = agX < n.x ? -1 : 1;
      n.watching = true;
      aiming |= n.aimT > 0;
      if (shooter < 0) shooter = i;
    }
    if (shooter < 0) secAlertT = -1;           // (none left)
    else if (!s2Recog) {
      secAlertT -= dt;
      if (secAlertT <= 0 && !aiming) { secAlertT = 0; npcs[shooter].aimT = 0.001f; }
    }
  }
  for (int i = 0; i < npcCount; i++) {
    Npc &n = npcs[i];
    n.t += dt;
    if (n.dead) {
      n.dieT += dt;
      continue;
    }
    if (n.aimT > 0) {                        // a guard drawing his pistol: then he fires
      float was = n.aimT;
      n.aimT += dt;
      if (was < GUARD_AIM_SECONDS && n.aimT >= GUARD_AIM_SECONDS) {
        sfxPlay(SFX_SHOT);
        shotFlashT = 0.12f;
        s2Recog = true;                      // (when he comes round: out in the lift area)
        searchItem = -1;
        if (agState == ST_LIFT && agLift >= 0) lifts[agLift].moving = false;
        hurt(SHOT_HURT);
        loseTime(SHOT_TIME_LOST, "SHOT BY A GUARD!");
        if (gameOver == END_NONE) knockOut();
      }
      continue;
    }
    if (n.watching) continue;
    if (n.a == n.b) {                        // standing at his post: looking one way, then the other (or not)
      if (n.turns && fmodf(n.t, 4.0f) < dt) n.dir = -n.dir;
      continue;
    }
    if (n.pause > 0) {
      n.pause -= dt;
      if (n.pause <= 0) n.dir = -n.dir;
      continue;
    }
    n.x += n.dir * NPC_SPEED * dt;
    if ((n.dir > 0 && n.x >= n.b) || (n.dir < 0 && n.x <= n.a)) {
      n.x = clampv(n.x, n.a, n.b);
      n.pause = 1.8f;
    }
  }
}

// Can this one see him? (facing him, on his level, close enough)
static bool npcSees(const Npc &n) {
  return !n.dead && fabsf(agY - n.y) < 60 && n.dir * (agX - n.x) > 0 && fabsf(agX - n.x) < n.sight;
}

// Suspicion: someone who can see him while he looks at them, or while he searches, gets
// suspicious - a point every SUSP_STEP (twice as fast if he's searching). Looking away and not
// searching clears it. SUSP_MAX: recognised.
static void updateSuspicion(float dt) {
  if (npcCount == 0 || s2Recog || agState == ST_CAUGHT || agState == ST_KO || s2Act || secAlertT >= 0) return;
  for (int i = 0; i < npcCount; i++)
    if (npcs[i].aimT > 0) return;            // (a guard is about to shoot)
  bool searching = agState == ST_USE, any = false;
  float step = SUSP_STEP;
  int guard = -1;
  for (int i = 0; i < npcCount; i++) {
    Npc &n = npcs[i];
    bool looking = agFacing * (n.x - agX) > 0;
    n.watching = npcSees(n) && (looking || searching);
    any |= n.watching;
    if (n.watching && n.kind == PK_SECURITY) {
      step = SUSP_STEP_GUARD;
      if (guard < 0) guard = i;
    }
  }
  if (!any) {
    susp = 0;
    suspT = 0;
    return;
  }
  suspT += dt * (searching ? 2.0f : 1.0f);
  if (suspT < step) return;
  suspT -= step;
  susp++;
  sfxPlay(SFX_WARN);
  if (guard >= 0 && susp >= SUSP_MAX - 1) {  // a guard: one point short, he draws and fires
    npcs[guard].aimT = 0.001f;
    return;
  }
  if (susp >= SUSP_MAX) {                    // recognised: hands up, then out to the lift area, the clock down
    susp = SUSP_MAX;
    s2Recog = true;
    searchItem = -1;
    if (agState == ST_LIFT && agLift >= 0) lifts[agLift].moving = false;
    sfxPlay(SFX_ALARM);
    loseTime(RECOGNISED_TIME_LOST, "RECOGNISED!");
    setState(ST_CAUGHT, 0.15f);
  }
}

static void drawNpcs() {
  for (int i = 0; i < npcCount; i++) {
    const Npc &n = npcs[i];
    AgentPose p;
    if (n.dead) {                            // killed: down, a pool spreading under him
      float f = fminf(1.0f, n.dieT / 0.5f);
      int pw = (int)(10 + 50 * fminf(1.0f, n.dieT / 3.0f));
      if (n.dieT > 0.4f) fillRect((int)n.x - pw, (int)n.y - 3, (int)n.x + pw, (int)n.y - 1, rgb565(130, 10, 14));
      agentLerp(PERSON_IDLE, PERSON_LIE, f * f, p);
      p.spin = -90 * f * f;
      drawPersonU(p, n.x, n.y, n.dir, AGENT_SCALE, n.kind);
      continue;
    }
    if (n.aimT > 0) {                        // a guard: pistol up, at him
      static const AgentPose AIM = { 0, -2, 88, 2, 80, 14, 6, -4, -8, -3, 0, 0, 0, 1, 0 };
      agentLerp(PERSON_IDLE, AIM, fminf(1.0f, n.aimT / 0.25f), p);
      drawPersonU(p, n.x, n.y, n.dir, AGENT_SCALE, n.kind);
      if (shotFlashT > 0) {
        int hx, hy, mx, my;
        agentHand(p, n.x, n.y, n.dir, AGENT_SCALE, hx, hy, mx, my);
        glowDisc((float)mx, (float)my, 22, 255, 220, 120, 0.9f);
        fillCircle(mx, my, 5, rgb565(255, 250, 220));
      }
      continue;
    }
    if (n.watching) {                        // stopped, staring at him
      p = PERSON_IDLE;
      p.head = -4;
      p.torso = -2;
    } else if (n.seated) {                   // at a desk: sitting, typing
      memset(&p, 0, sizeof(p));
      p.legN = 88; p.kneeN = -86; p.legF = 82; p.kneeF = -84;
      p.armN = 70 + 4 * sinf(n.t * 14); p.elbowN = 40; p.armF = 64 + 4 * sinf(n.t * 13 + 1); p.elbowF = 44;
      p.torso = 4; p.head = 6;
      p.plant = 1;
    } else if (n.pause > 0) {                // busy: reaching forward, working
      p = PERSON_IDLE;
      p.armN = 70 + 12 * sinf(n.t * 8); p.elbowN = 50; p.armF = 60 + 10 * sinf(n.t * 8 + 1.5f); p.elbowF = 56;
      p.torso = 8;
      p.head = 10;
    } else {
      personWalk(p, n.t);
    }
    drawPersonU(p, n.x, n.y, n.dir, AGENT_SCALE, n.kind);
    if (n.kind == PK_CLEANER) {              // a mop in the near hand
      int hx, hy, mx, my;
      agentHand(p, n.x, n.y, n.dir, AGENT_SCALE, hx, hy, mx, my);
      float fx = hx + n.dir * 34.0f;
      drawLineF((float)hx - n.dir * 6.0f, hy - 40.0f, fx, n.y - 8.0f, rgb565(170, 130, 80), 3.0f);
      fillRectF(fx - 16, n.y - 12, fx + 16, n.y - 1, rgb565(230, 230, 220));
      fillRectF(fx - 4, n.y - 16, fx + 4, n.y - 11, rgb565(60, 140, 200));
    }
    if (secAlertT >= 0 && n.kind == PK_SECURITY && !n.knifeOnly) {   // on alert: ! over their heads
      drawVectorTextW("!", n.x, n.y - 200, 5.0f, rgb565(255, 60, 40), 3.0f);
    } else if (n.watching && susp > 0) {     // ? over their head, redder with each point
      float k = (float)susp / SUSP_MAX;
      uint16_t c = rgb565(255, (int)(220 - 170 * k), (int)(80 - 40 * k));
      drawVectorTextW("?", n.x, n.y - 200, 4.0f + k * 2.0f, c, 2.6f);
    }
  }
}

// SUSPICION: five pips at the top of the screen, in a room with people in it
static void drawSuspicionMeter() {
  int x0 = 20, y0 = HUD_H + 8, x1 = 330, y1 = HUD_H + 48;
  fillRect(x0, y0, x1, y1, rgb565(12, 14, 20));
  drawRectOutline(x0, y0, x1, y1, susp > 0 ? rgb565(255, 90, 60) : rgb565(90, 100, 120), 1);
  drawVectorText("SUSPICION", x0 + 74, (y0 + y1) / 2, 2.4f, susp > 0 ? rgb565(255, 200, 80) : rgb565(190, 200, 220), false);
  for (int k = 0; k < SUSP_MAX; k++) {
    int bx = x0 + 156 + k * 32, by = y0 + 10;
    bool on = k < susp;
    fillRect(bx, by, bx + 22, by + 18, on ? (susp >= SUSP_MAX - 1 ? rgb565(255, 60, 40) : rgb565(255, 170, 50)) : rgb565(40, 44, 54));
    drawRectOutline(bx, by, bx + 22, by + 18, rgb565(120, 128, 146), 1);
  }
}

// ACCESS CARD LEVEL n FOUND: the card, big, for a moment
static bool panelGun = false;
static void foundCard(int level) {          // (-1: the knife, -2: the pistol)
  panelKnife = level == -1;
  panelGun = level == -2;
  if (level > s2Card) s2Card = level;
  cardShown = level;
  cardShowT = CARD_SHOW;
  sfxPlay(SFX_FOUND);
  sfxPlay(SFX_FANFARE);
}
static void drawCardPanel() {
  gfxClipAll();
  int x0 = 340, y0 = 150, x1 = 940, y1 = 420;
  float k = fminf(1.0f, (CARD_SHOW - cardShowT) / 0.2f);
  blendRect(x0, y0, x1, y1, 0, 0, 0, 0.78f * k);
  drawRectOutline(x0, y0, x1, y1, rgb565(90, 200, 255), 3);
  char b[40];
  if (panelGun) {                            // the pistol, and how to fire it
    drawVectorText("PISTOL FOUND", SCREEN_W / 2, y0 + 40, 3.4f, rgb565(240, 244, 250), true);
    int kx = SCREEN_W / 2, ky = y0 + 140;
    fillRect(kx - 90, ky - 30, kx + 80, ky - 4, rgb565(40, 42, 48));          // the slide
    fillRect(kx - 90, ky - 30, kx + 80, ky - 26, rgb565(90, 94, 104));
    fillRect(kx + 80, ky - 24, kx + 92, ky - 10, rgb565(30, 30, 34));         // the muzzle
    int gx[4] = { kx - 80, kx - 30, kx - 46, kx - 96 }, gy[4] = { ky - 4, ky - 4, ky + 70, ky + 70 };
    fillPolygon(gx, gy, 4, rgb565(50, 52, 58));                               // the grip
    fillRect(kx - 30, ky - 4, kx - 6, ky + 24, rgb565(40, 42, 48));           // the trigger guard
    fillRect(kx - 26, ky, kx - 10, ky + 20, rgb565(0, 0, 0));
    drawVectorText("R1 OR R2 TO FIRE", SCREEN_W / 2, y1 - 40, 2.2f, rgb565(255, 220, 120), false);
    return;
  }
  if (panelKnife) {                          // the knife, and what it's for
    drawVectorText("KNIFE FOUND", SCREEN_W / 2, y0 + 40, 3.4f, rgb565(240, 244, 250), true);
    int kx = SCREEN_W / 2, ky = y0 + 140;
    int bx[4] = { kx - 90, kx + 70, kx + 110, kx - 90 }, by[4] = { ky - 12, ky - 12, ky, ky + 6 };
    fillPolygon(bx, by, 4, rgb565(214, 220, 230));
    fillRect(kx - 90, ky - 12, kx + 70, ky - 10, rgb565(250, 252, 255));
    fillRect(kx - 170, ky - 14, kx - 90, ky + 8, rgb565(40, 32, 28));
    for (int k = 0; k < 3; k++) fillCircle(kx - 150 + k * 22, ky - 3, 3, rgb565(200, 200, 206));
    drawVectorText("X CLOSE TO SOMEONE TO USE IT", SCREEN_W / 2, y1 - 40, 2.2f, rgb565(255, 220, 120), false);
    return;
  }
  snprintf(b, sizeof(b), "ACCESS CARD LEVEL %d FOUND", cardShown);
  drawVectorText(b, SCREEN_W / 2, y0 + 40, 3.4f, rgb565(240, 244, 250), true);
  int cx0 = SCREEN_W / 2 - 110, cy0 = y0 + 80, cx1 = SCREEN_W / 2 + 110, cy1 = y0 + 220;   // the card
  fillRect(cx0, cy0, cx1, cy1, rgb565(240, 242, 246));
  fillRect(cx0, cy0, cx1, cy0 + 30, rgb565(40, 110, 200));
  drawVectorText("QUANTUM LAB", (cx0 + cx1) / 2, cy0 + 15, 2.0f, rgb565(240, 244, 250), true);
  fillRect(cx0 + 18, cy0 + 50, cx0 + 58, cy0 + 80, rgb565(220, 180, 60));                 // its chip
  fillRect(cx0 + 18, cy0 + 64, cx0 + 58, cy0 + 65, rgb565(160, 120, 30));
  fillRect(cx0 + 37, cy0 + 50, cx0 + 38, cy0 + 80, rgb565(160, 120, 30));
  snprintf(b, sizeof(b), "L%d", cardShown);
  drawVectorTextW(b, cx1 - 60, cy0 + 74, 6.0f, rgb565(200, 30, 30), 3.0f);
  drawVectorText("ACCESS", cx0 + 60, cy1 - 22, 1.6f, rgb565(80, 90, 110), false);
  fillRect(cx0, cy1 - 8, cx1, cy1, rgb565(200, 30, 30));
}

// The maintenance man over his shoulder, or going into the bin (drawn after him)
static void drawCarriedBody() {
  if (s2Act != 3 || !(bodyCarried || binPhase == 2)) return;
  if (binPhase == 2 && !bodyCarried) { drawBinFront(true, CAR_FLOOR_Y); return; }
  float x = agX - agFacing * 6, y = agY - 100;
  if (binPhase == 2) {                       // up and over the rim, and down inside
    float k = fminf(1.0f, binPT / 0.85f), top = CAR_FLOOR_Y - BIN_H;
    if (k < 0.5f) {
      float e = k / 0.5f;
      x = x + (BIN_X - x) * e;
      y = y + ((top - 26) - y) * e;
    } else {
      float e = (k - 0.5f) / 0.5f;
      x = BIN_X;
      y = (top - 26) + 90 * e * e;
    }
  }
  AgentPose b = PERSON_LIE;
  if (binPhase == 2) b.spin = -90 - 70 * fminf(1.0f, binPT / 0.85f);
  drawPersonU(b, x, y, agFacing, AGENT_SCALE, PK_MAINT_UNDIES);
  if (binPhase == 2) drawBinFront(true, CAR_FLOOR_Y);
}

// What X / SQUARE would do here (stage 2's lift areas)
static void drawS2Prompts() {
  if (s2Act || agState != ST_IDLE || gameOver != END_NONE) return;
  const char *t = NULL;
  float x = agX;
  if (atWorker()) t = "X: KNOCK OUT";
  else if (atBody()) t = wkState == WK_DOWN ? "SQUARE: SEARCH" : "SQUARE: INTO THE BIN";
  else if (atFoyerAid() && !s2Aid[curShaft][carFloor]) t = "SQUARE: FIRST AID";
  if (!t) return;
  float w = vectorTextWidth(t, 2.4f);
  x = clampv(x, w / 2 + 24, SCREEN_W - w / 2 - 24);
  blendRect((int)(x - w / 2 - 10), CAR_FLOOR_Y - 196, (int)(x + w / 2 + 10), CAR_FLOOR_Y - 172, 0, 0, 0, 0.55f);
  drawVectorText(t, x, CAR_FLOOR_Y - 184, 2.4f, rgb565(255, 220, 120), true);
}

// Each frame of play in stage 2
static void updateStage2(float dt) {
  if (s2BumpT > 0) s2BumpT -= dt;
  updateWorker(dt);
  if (mode == MODE_ROOM && curRoom < S2_ROOMS) {
    updateSuspicion(dt);
    updateNpcs(dt);
  }
  if (bodyT >= 0) {                          // he'll be found
    float was = bodyT;
    bodyT -= dt;
    if (bodyT <= 10 && (int)ceilf(was) != (int)ceilf(bodyT)) sfxPlay(SFX_WARN);
    if (bodyT <= 0) {
      bodyT = 0;
      sfxPlay(SFX_ALARM);
      endGame(END_BODY);
    }
  }
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
      } else if ((pressed & BTN_JUMP) && atWorker()) {
        startS2Act(1);                  // stage 2: knock the maintenance man out
      } else if ((pressed & BTN_JUMP) && atNpcToKill() >= 0) {
        killTarget = atNpcToKill();     // ...the knife
        startS2Act(4);
        agFacing = npcs[killTarget].x < agX ? -1 : 1;
      } else if ((pressed & BTN_FIRE) && canFire()) {
        int f = want ? want : agFacing;  // ...the pistol
        startS2Act(5);
        agFacing = f;
      } else if (pressed & BTN_JUMP) {
        if (want) agFacing = want;
        startJump();
      } else if ((pressed & BTN_USE) && atBody()) {
        startS2Act(wkState == WK_DOWN ? 2 : 3);   // ...search him, then into the bin with him
      } else if ((pressed & BTN_USE) && atFoyerAid()) {
        useFoyerAid();                  // ...the walkway's first aid cabinet
      } else if ((pressed & BTN_USE) && atLoopPower()) {
        useLoopPower();                 // the QUANTUMLOOP's power...
      } else if ((pressed & BTN_USE) && atLoopDoor()) {
        useLoopDoor();                  // ...and its doors
      } else if ((pressed & BTN_USE) && atConsole()) {
        useConsole();                   // QUANTUM CONTROL: the main computer...
      } else if ((pressed & BTN_USE) && atBay()) {
        useBay();                       // ...and the QUANTUM DRIVE BAY
      } else if ((held & BTN_USE) && mode == MODE_ROOM && curRoom >= ROOM_COUNT) {
        // (nothing to search in QUANTUM CONTROL or the sub basement)
      } else if ((held & BTN_USE) && mode == MODE_ROOM && decorAt() >= 0 && pieceDone(decorAt())) {
        // searched already: it isn't searched again
        if (pressed & BTN_USE) {
          int i = decorAt();
          snprintf(floatText, sizeof(floatText), decor[i].kind == DEC_FIRSTAID ? "EMPTY" : (decor[i].kind == DEC_GENERATOR ? "ALREADY ON" : "SEARCHED"));
          floatColour = rgb565(150, 156, 170);
          nothingShowT = NOTHING_SHOW;
          nothingX = decor[i].x;
          nothingY = decor[i].y - dH(i) - 22;
        }
      } else if ((held & BTN_USE) && mode == MODE_ROOM) {   // (nothing to search in the lift)
        searchItem = -1;
        setState(ST_USE, 0.18f);
      } else if (want) {
        agFacing = want;
        bool blocked = (want > 0) ? (agX >= edgeMax() && !exitOpen(true)) : (agX <= edgeMin() && !exitOpen(false));
        if (!blocked) setState(ST_RUN, BLEND_SECONDS);
        else s2Bump(want > 0);          // (stage 2: a door his card won't open)
      }
      break;

    case ST_RUN: {
      if ((pressed & BTN_JUMP) && atWorker()) {
        setState(ST_IDLE, 0.1f);
        startS2Act(1);
        break;
      }
      if ((pressed & BTN_JUMP) && atNpcToKill() >= 0) {
        setState(ST_IDLE, 0.1f);
        killTarget = atNpcToKill();
        startS2Act(4);
        agFacing = npcs[killTarget].x < agX ? -1 : 1;
        break;
      }
      if ((pressed & BTN_FIRE) && canFire()) {
        int f = agFacing;
        setState(ST_IDLE, 0.1f);
        startS2Act(5);
        agFacing = f;
        break;
      }
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
        s2Bump(agFacing > 0);
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
      } else if (agY > SCREEN_H + 160) {   // down a pit: 15 health and 30 seconds
        nPitFalls++;
        loseTime(PIT_TIME_LOST, "FELL DOWN A PIT!");
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
      if (bayBusy) {                   // putting the drives in the bay
        updateBay(dt);
        break;
      }
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
          float px = decor[i].x, py = decor[i].y - dH(i) - 22;
          if (decor[i].kind == DEC_GENERATOR) {   // the power generator: the lever goes up, its lamp turns green
            ri.genOn = true;
            leverT = 0;
            int bonus = GENERATOR_BONUS;      // (5 more minutes on the clock)
            timeLeft += bonus;
            addScore(SEARCH_SCORE, px, py - 26);
            sfxPlay(SFX_POWERUP);
            snprintf(floatText, sizeof(floatText), "POWER ON");
            floatColour = rgb565(100, 240, 140);
            nothingShowT = NOTHING_SHOW;
            nothingX = px;
            nothingY = py;
            char b[64];
            int n = generatorsOn();
            if (n >= GENERATORS) snprintf(b, sizeof(b), "ALL %d POWER GENERATORS ON   +%d MINUTES", GENERATORS, bonus / 60);
            else snprintf(b, sizeof(b), "POWER GENERATOR ON - %d OF %d   +%d MINUTES", n, GENERATORS, bonus / 60);
            showMsg(b, 3.5f);
            break;
          }
          if (decor[i].kind == DEC_TERMINAL) {   // the facility terminal: logged into
            searchT = 0;
            useFacilityTerminal();
            break;
          }
          if (decor[i].kind == DEC_FIRSTAID) {   // the first aid kit
            if (health >= HEALTH_MAX) {
              snprintf(floatText, sizeof(floatText), "HEALTH FULL");
              floatColour = rgb565(170, 170, 180);
              searchT = 0;                    // (not used: it can be searched again later)
            } else {
              health = HEALTH_MAX;
              ri.kitUsed = true;
              nKitsUsed++;
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
          if (decor[i].kind == DEC_LAB) {      // the lab's things: nothing in them yet
            bool first = !s2Searched[curRoom][i];
            s2Searched[curRoom][i] = true;
            if (first) addScore(SEARCH_SCORE, px, py - 26);
            if (first && s2Kind[curRoom] == LK_KITCHEN && i == s2KnifeItem) {   // the kitchen's knife
              hasKnife = true;
              foundCard(-1);
              break;
            }
            if (first && s2Kind[curRoom] == LK_SECURITY && i == s2GunItem) {    // a pistol in a gun rack
              hasGun = true;
              foundCard(-2);
              break;
            }
            if (first && s2Kind[curRoom] == LK_MEDICAL && i == s2CardItem) {    // a patient's level 2 card
              foundCard(2);
              break;
            }
            nothingShowT = NOTHING_SHOW;
            nothingX = px;
            nothingY = py;
            snprintf(floatText, sizeof(floatText), "NOTHING");
            sfxPlay(SFX_CLUNK);
            floatColour = rgb565(170, 170, 180);
            break;
          }
          bool first = !ri.searched[i];
          ri.searched[i] = true;
          if (first) addScore(SEARCH_SCORE, px, py - 26);
          if (first && ri.docs[i]) {         // TOP SECRET DOCUMENTS: bonus points, more for each one found
            docValue = DOC_VALUES[docsFound < DOCUMENTS ? docsFound : DOCUMENTS - 1];
            docsFound++;
            sfxPlay(SFX_FOUND);
            sfxPlay(SFX_FANFARE);
            addScore(docValue, px, py - 56);
            docShowT = DOC_SHOW;
            itemShowT = 0;
            nothingShowT = 0;
          } else if (first && ri.holds[i]) {        // a quantum drive in it
            sfxPlay(SFX_FOUND);
            sfxPlay(SFX_FANFARE);
            itemsFound++;
            drivesCarried++;
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
      if (gameOver == END_NONE && agT >= CAUGHT_SECONDS) {
        if (s2Recog) s2Eject();         // recognised in the lab: out through the door he came in by
        else respawn();
      }
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
        nShocks++;
        loseTime(SHOCK_TIME_LOST, "ELECTROCUTED!");   // (10 health and 30 seconds)
        hurt(ELEC_HURT);
        knockOut();                    // (he wakes at the door he came in by)
      }
      break;

    case ST_KO:
      if (gameOver != END_NONE) break;
      if (koGetUp && agT >= HURT_SECONDS) setState(ST_IDLE, 0.35f);
      else if (!koGetUp && agT >= RESPAWN_SECONDS) {
        if (s2Recog) s2Eject();         // (shot by a guard: he comes round in the lift area)
        else respawn();
      }
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
    c.angle = c.mid + (c.mid > 0 ? 1 : -1) * CAM_SWING_DEG * sinf(TWO_PI * c.t / c.period);
    bool seen = inBeam(c, agX, fy - 8) || inBeam(c, agX, fy - h * 0.5f) || inBeam(c, agX, fy - h * 0.9f);
    if (camGrace > 0 || agState == ST_KO || agState == ST_SHOCK) seen = false;
    if (seen && agentMoving()) {
      c.movingT += dt;
      c.view = 1;
      if (c.movingT >= CAM_REACT) {    // CAUGHT
        c.view = 2;
        nCaught++;
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
  if (drGrace > 0) drGrace -= dt;
  if (drScan) {                        // the red beam turning right round it: it sees him through anything, still or not
    drScanAng += drScanDir * DRONE_SCAN_DEG_S * dt;
    if (drScanAng > 180.0f) drScanAng -= 360.0f;
    if (drScanAng < -180.0f) drScanAng += 360.0f;
    float h = 100.0f * myScale;
    const float ys[3] = { fy - 8, fy - h * 0.5f, fy - h * 0.9f };
    bool seen = false;
    for (int k = 0; k < 3 && !seen && agState != ST_KO && agState != ST_CAUGHT; k++) {
      float dx = agX - drX, dy = ys[k] - drY;
      float diff = fabsf(fmodf(atan2f(dx, dy) * 57.29578f - drScanAng + 540.0f, 360.0f) - 180.0f);
      seen = diff <= DRONE_SCAN_HALF;
    }
    if (seen) {                        // found him: the beam goes off, and it flies to where he was (just over his head)
      drScan = false;
      drTX = clampv(agX, 40.0f, SCREEN_W - 40.0f);
      drTY = clampv(fy - 150.0f * myScale, CEIL_Y + 30.0f, ROOM_FLOOR_Y - 30.0f);
      if (!drAlerted) {
        drAlerted = true;
        sfxPlay(SFX_ALARM);
        showMsg("THE DRONE HAS SEEN YOU", 2.2f);
      } else {
        sfxPlay(SFX_BEEP);
      }
    }
  }
  float tx = drScan ? drX : drTX, ty = drScan ? drY : drTY;
  float dx = tx - drX, dy = ty - drY, d = sqrtf(dx * dx + dy * dy);
  if (!drScan && d < 8.0f && fabsf(drVX) + fabsf(drVY) < 30.0f) {   // there: scan again, the beam starting the way it was going
    drScan = true;
    drScanAng = drVX >= 0 ? 90.0f : -90.0f;
    drScanDir = fxRnd(0, 2) ? 1.0f : -1.0f;
  }
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
      nDroneHits++;
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

static void drawGoggleReadout();
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
  drawGoggleReadout();
}
static void drawGoggleReadout() {         // (NIGHT VISION and the corners of the view)
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
  if (drScan) {                          // the scanning beam: narrow, red, right across the room
    float a0 = (drScanAng - DRONE_SCAN_HALF) * 0.017453293f, a1 = (drScanAng + DRONE_SCAN_HALF) * 0.017453293f, am = drScanAng * 0.017453293f;
    float xs[3] = { x, x + sinf(a0) * 1600, x + sinf(a1) * 1600 }, ys[3] = { y + 8, y + 8 + cosf(a0) * 1600, y + 8 + cosf(a1) * 1600 };
    blendPolygonF(xs, ys, 3, 255, 30, 20, 0.28f);
    drawLineF(x, y + 8, x + sinf(am) * 1600, y + 8 + cosf(am) * 1600, rgb565(255, 90, 70), 1.5f);
  }
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

// ---- QUANTUM CONTROL, each frame: the racks' lights, the tape reels, the
// fan, the status display, the main computer's screen, the drive bay's slots
static void drawReel(int cx, int cy, float a) {
  fillCircle(cx, cy, 22, rgb565(70, 66, 60));          // the tape wound on it
  fillCircle(cx, cy, 20, rgb565(180, 184, 192));       // the flange
  for (int k = 0; k < 3; k++) {
    float b = a + k * TWO_PI / 3;
    fillCircle(cx + (int)lroundf(cosf(b) * 11), cy + (int)lroundf(sinf(b) * 11), 5, rgb565(40, 44, 52));
  }
  fillCircle(cx, cy, 4, rgb565(90, 94, 104));
}

static void drawControlRoomAnim() {
  int fy = ROOM_FLOOR_Y;
  float t = clockT;
  bool power = generatorsOn() >= GENERATORS;
  bool blink = fmodf(t, 1.0f) < 0.6f;
  static const uint8_t ledRGB[4][3] = { { 60, 230, 100 }, { 255, 180, 40 }, { 255, 60, 40 }, { 80, 160, 255 } };
  for (int k = 0; k < 3; k++)            // rack lights
    for (int r = 0; r < 14; r++)
      for (int c = 0; c < 5; c++) {
        int n = k * 97 + r * 5 + c;
        float period = 0.18f + (n * 37 % 11) * 0.06f;
        uint32_t h = (uint32_t)(t / period) * 2654435761u + n * 2246822519u;
        if (((h >> 15) & 3) == 0) continue;
        const uint8_t *col = ledRGB[(n * 7 + (h >> 20)) % 4 == 2 ? 2 : (n % 3 == 0 ? 1 : (n % 5 == 0 ? 3 : 0))];
        int x = ccRackX[k] + 10 + c * 9, y = fy - 216 + r * 9;
        fillRect(x, y, x + 4, y + 3, rgb565(col[0], col[1], col[2]));
      }
  for (int k = 0; k < 2; k++) {          // the tape reels: running, stopping, changing direction
    float a = 3.2f * t + 2.6f * sinf(t * 0.45f + k * 2.0f) * 3.0f;
    int x0 = ccTapeX[k];
    drawReel(x0 + 31, fy - 188, a);
    drawReel(x0 + 31, fy - 134, -a * 0.8f);
    drawLine(x0 + 53, fy - 188, x0 + 53, fy - 96, rgb565(70, 66, 60), false);
    bool lamp = ((int)(t * 3 + k) & 1);
    fillRect(x0 + 50, fy - 72, x0 + 54, fy - 64, lamp ? rgb565(255, 190, 40) : rgb565(80, 60, 20));
  }
  {                                      // the fan
    float a = t * 14.0f;
    for (int k = 0; k < 3; k++) {
      float b = a + k * TWO_PI / 3;
      float xs[4] = { CC_FAN_X + cosf(b - 0.25f) * 8, CC_FAN_X + cosf(b - 0.35f) * 38, CC_FAN_X + cosf(b + 0.30f) * 38, CC_FAN_X + cosf(b + 0.25f) * 8 };
      float ys[4] = { CC_FAN_Y + sinf(b - 0.25f) * 8, CC_FAN_Y + sinf(b - 0.35f) * 38, CC_FAN_Y + sinf(b + 0.30f) * 38, CC_FAN_Y + sinf(b + 0.25f) * 8 };
      fillPolygonF(xs, ys, 4, rgb565(120, 126, 136));
    }
    fillCircle(CC_FAN_X, CC_FAN_Y, 8, rgb565(70, 74, 84));
    for (int k = -36; k <= 36; k += 12)
      drawLine(CC_FAN_X + k, CC_FAN_Y - 36 + abs(k) / 3, CC_FAN_X + k, CC_FAN_Y + 36 - abs(k) / 3, rgb565(60, 64, 72), false);
  }
  // the status display: the countdown once there is power
  if (power) {
    drawVectorText("QUANTUM CORE COLLAPSE IN", CONSOLE_X, 306, 1.6f, rgb565(200, 80, 60), false);
    int sec = (int)ceilf(fmaxf(timeLeft, 0));
    char buf[24];
    snprintf(buf, sizeof(buf), "%d:%02d", sec / 60, sec % 60);
    drawVectorText("88:88", CONSOLE_X, 344, 4.6f, rgb565(44, 12, 10), true);
    drawVectorText(buf, CONSOLE_X, 344, 4.6f, rgb565(255, 50, 36), true);
  } else if (blink) {
    drawVectorText("POWER OFFLINE", CONSOLE_X, 331, 3.0f, rgb565(200, 40, 30), true);
  }
  // the main computer's screen
  int x = CONSOLE_X;
  fillRect(x - 66, fy - 188, x + 66, fy - 84, power ? rgb565(6, 16, 10) : rgb565(8, 8, 10));
  uint16_t g = rgb565(100, 255, 140);
  if (!power) {
    if (blink) drawVectorText("NO POWER", x, fy - 136, 2.4f, rgb565(200, 40, 30), true);
  } else if (subUnlocked) {
    char b[24];
    snprintf(b, sizeof(b), "LIFT %c", shaftLetter(subShaft));
    drawVectorText("SUB BASEMENT", x, fy - 164, 1.7f, g, false);
    drawVectorText(b, x, fy - 132, 3.2f, rgb565(255, 220, 80), true);
    drawVectorText("UNLOCKED", x, fy - 102, 1.9f, g, true);
  } else {
    drawVectorText("QUANTUM OS", x, fy - 156, 1.9f, g, true);
    if (blink) drawVectorText("READY", x, fy - 120, 2.4f, g, true);
  }
  fillCircle(x + 86, fy - 37, 3, power ? rgb565(80, 230, 120) : rgb565(255, 50, 40));
  // the drive bay: ITEMS_HIDDEN slots (6 across), lit as drives are loaded, and the counter
  for (int i = 0; i < ITEMS_HIDDEN; i++) {
    int c = i % 6, r = i / 6;
    int rowsUsed = (ITEMS_HIDDEN + 5) / 6;
    int sx = BAY_X - 84 + c * 28, sy = BAY_TOP + 52 + (3 - rowsUsed) * 23 + r * 46;
    bool in = i < drivesLoaded;
    fillRect(sx, sy, sx + 22, sy + 34, in ? rgb565(44, 50, 62) : rgb565(6, 8, 10));
    if (in) {
      fillRect(sx + 8, sy + 4, sx + 13, sy + 28, rgb565(90, 230, 255));   // the drive's glowing strip
      fillRect(sx + 17, sy + 29, sx + 20, sy + 32, rgb565(80, 255, 120));
    } else {
      fillRect(sx + 17, sy + 29, sx + 20, sy + 32, rgb565(120, 24, 20));
    }
  }
  char cb[16];
  snprintf(cb, sizeof(cb), "%02d/%02d", drivesLoaded, ITEMS_HIDDEN);
  drawVectorText(cb, BAY_X, BAY_TOP + 219, 2.0f, drivesLoaded >= ITEMS_HIDDEN ? rgb565(80, 255, 120) : rgb565(255, 190, 70), true);
  // USE, over what he is standing at
  if (blink && !termOn) {
    if (atConsole()) drawVectorText("USE", CONSOLE_X, fy - 216, 3.0f, rgb565(255, 220, 120), true);
    if (atBay()) drawVectorText("USE", BAY_X, BAY_TOP - 18, 3.0f, rgb565(255, 220, 120), true);
  }
}

// NO POWER: the main computer won't start until all the generators are on
static void drawPowerPanel() {
  int x0 = 330, y0 = 96, x1 = 950, y1 = 470;
  fillRect(x0, y0, x1, y1, rgb565(10, 12, 20));
  drawRectOutline(x0, y0, x1, y1, rgb565(255, 80, 60), 3);
  drawVectorText("NO POWER", SCREEN_W / 2, y0 + 34, 4.0f, rgb565(255, 80, 60), true);
  drawVectorText("THE MAIN COMPUTER NEEDS POWER", SCREEN_W / 2, y0 + 76, 2.2f, rgb565(230, 236, 245), false);
  fillRect(SCREEN_W / 2 - 110, y0 + 100, SCREEN_W / 2 + 110, y0 + 250, rgb565(36, 42, 54));   // the picture
  drawRectOutline(SCREEN_W / 2 - 110, y0 + 100, SCREEN_W / 2 + 110, y0 + 250, rgb565(90, 100, 120), 2);
  fillRect(SCREEN_W / 2 - 108, y0 + 232, SCREEN_W / 2 + 108, y0 + 248, rgb565(60, 64, 72));
  paintGenerator(SCREEN_W / 2, y0 + 232);
  drawGeneratorLive(SCREEN_W / 2, y0 + 232, 0.0f, false);
  drawVectorText("POWER GENERATOR", SCREEN_W / 2, y0 + 270, 2.0f, rgb565(255, 220, 80), true);
  char b[64];
  snprintf(b, sizeof(b), "FIND THE %d POWER GENERATORS AND TURN THEM ON", GENERATORS);
  drawVectorText(b, SCREEN_W / 2, y0 + 308, 2.2f, rgb565(230, 236, 245), true);
  snprintf(b, sizeof(b), "%d OF %d ON", generatorsOn(), GENERATORS);
  drawVectorText(b, SCREEN_W / 2, y0 + 344, 2.6f, rgb565(100, 240, 140), true);
}

// The main computer, close up (as NUCLEAR MISSION's launch computer): the
// monitor fills the view; UP / DOWN to choose, X or SQUARE to pick
#define TX0 168
#define TX1 1112
#define TY0 96
#define TY1 552
static void drawTermCase(const char *label) {
  gfxClip(HUD_H, PLAY_Y1);
  fillRect(0, HUD_H, SCREEN_W - 1, PLAY_Y1, rgb565(22, 24, 28));
  uint16_t case1 = rgb565(176, 170, 150), case2 = rgb565(146, 140, 122);
  fillRect(TX0 - 56, TY0 - 44, TX1 + 56, TY1 + 46, case1);                // the monitor
  fillRect(TX0 - 56, TY0 - 44, TX1 + 56, TY0 - 40, rgb565(206, 200, 182));
  fillRect(TX1 + 52, TY0 - 40, TX1 + 56, TY1 + 46, case2);
  fillRect(TX0 - 56, TY1 + 38, TX1 + 56, TY1 + 46, case2);
  fillRect(TX0 - 22, TY0 - 18, TX1 + 22, TY1 + 18, rgb565(40, 40, 38));  // inner bezel
  fillRect(TX0, TY0, TX1, TY1, rgb565(4, 18, 9));                         // the screen
  for (int y = TY0; y <= TY1; y += 3) fillRect(TX0, y, TX1, y, rgb565(2, 10, 5));
  fillCircle(TX1 + 30, TY1 + 30, 3, rgb565(80, 230, 120));
  drawVectorText(label, TX0 + 60, TY1 + 30, 1.4f, rgb565(90, 86, 74), false);
  int kxs[4] = { TX0 - 50, TX1 + 50, TX1 + 80, TX0 - 80 }, kys[4] = { TY1 + 60, TY1 + 60, PLAY_Y1, PLAY_Y1 };   // the keyboard
  fillPolygon(kxs, kys, 4, rgb565(184, 178, 158));
  for (int row = 0; row < 3; row++)
    for (int k = 0; k < 18 - row; k++) {
      int kx = TX0 - 30 + row * 20 + k * 54, ky = TY1 + 70 + row * 32;
      if (ky + 24 > PLAY_Y1) continue;
      fillRect(kx, ky, kx + 46, ky + 24, rgb565(120, 114, 100));
      fillRect(kx + 3, ky + 2, kx + 43, ky + 18, rgb565(214, 208, 190));
    }
}

static void drawFacilityTerminal();
static void drawTerminal() {
  if (termKind == 1) {
    drawFacilityTerminal();
    return;
  }
  drawTermCase("QC-9 QUANTUM CONTROL");
  uint16_t g = rgb565(100, 255, 140), gd = rgb565(40, 140, 70), dark = rgb565(4, 18, 9), red = rgb565(255, 90, 70);
  bool cur = ((int)(termT * 2.5f) & 1);
  float lx = TX0 + 30;
  drawTextLeft("QUANTUM OS 7.2   QUANTUM CONTROL QC-9", lx, TY0 + 26, 2.2f, gd, false);
  fillRect(TX0 + 18, TY0 + 46, TX1 - 18, TY0 + 47, gd);
  char b[64];
  switch (termPage) {
    case TP_BOOT: {
      float r = termT;
      drawTextLeft("BOOTING...", lx, TY0 + 90, 2.6f, g, true);
      if (r > 0.4f) {
        snprintf(b, sizeof(b), "MEMORY TEST  %dK", (int)(fminf((r - 0.4f) / 1.2f, 1.0f) * 4096) / 64 * 64);
        drawTextLeft(b, lx, TY0 + 136, 2.4f, g, false);
      }
      if (r > 1.6f) drawTextLeft("POWER GRID ......... 3 GENERATORS ONLINE", lx, TY0 + 176, 2.4f, g, false);
      if (r > 2.2f) drawTextLeft("CHECKING QUANTUM DRIVE BAY...", lx, TY0 + 216, 2.4f, g, false);
      if (r > BOOT_CHECK) {
        snprintf(b, sizeof(b), "QUANTUM DRIVES ..... %d OF %d LOADED", drivesLoaded, ITEMS_HIDDEN);
        drawTextLeft(b, lx, TY0 + 256, 2.4f, g, false);
        drawTextLeft("LOADING QUANTUM KERNEL", lx, TY0 + 306, 2.4f, g, true);
        int w = (int)(fminf((r - BOOT_CHECK) / (BOOT_DONE - BOOT_CHECK - 0.2f), 1.0f) * 600);
        drawRectOutline((int)lx, TY0 + 330, (int)lx + 604, TY0 + 352, g, 1);
        if (w > 0) fillRect((int)lx + 2, TY0 + 332, (int)lx + 2 + w, TY0 + 350, g);
      }
      break;
    }
    case TP_FAIL:
      drawTextLeft("CHECKING QUANTUM DRIVE BAY...", lx, TY0 + 90, 2.4f, g, false);
      snprintf(b, sizeof(b), "QUANTUM DRIVES ..... %d OF %d LOADED", drivesLoaded, ITEMS_HIDDEN);
      drawTextLeft(b, lx, TY0 + 130, 2.4f, g, false);
      drawVectorText("UNABLE TO BOOT", (TX0 + TX1) / 2, TY0 + 210, 5.0f, red, true);
      snprintf(b, sizeof(b), "ALL %d QUANTUM DRIVES MUST BE LOADED", ITEMS_HIDDEN);
      drawVectorText(b, (TX0 + TX1) / 2, TY0 + 274, 2.6f, red, true);
      drawVectorText("INTO THE QUANTUM DRIVE BAY", (TX0 + TX1) / 2, TY0 + 310, 2.6f, red, true);
      if (cur) drawVectorText("PRESS X TO LOG OFF", (TX0 + TX1) / 2, TY1 - 40, 2.4f, gd, false);
      break;
    case TP_MENU:
      drawTextLeft("WELCOME, OPERATOR. SELECT:", lx, TY0 + 84, 2.4f, g, false);
      for (int i = 0; i < TERM_ITEMS; i++) {
        int y = TY0 + 118 + i * 34;
        snprintf(b, sizeof(b), "%d  %s", i + 1, termItems[i]);
        if (i == termSel) {
          fillRect(TX0 + 20, y - 15, TX1 - 20, y + 15, rgb565(70, 220, 120));
          drawTextLeft(b, lx + 12, y, 2.6f, dark, true);
        } else {
          drawTextLeft(b, lx + 12, y, 2.6f, i == TERM_UNLOCK || i == TERM_LOCKDOWN ? rgb565(255, 220, 80) : g, true);
        }
      }
      drawTextLeft("UP / DOWN TO CHOOSE    X TO SELECT", lx, TY1 - 22, 1.8f, gd, false);
      break;
    case TP_MSG: {
      static const char *const msg[TERM_ITEMS][4] = {
        { "QUANTUM CORE STATUS", "CORE OUTPUT ........ 312%", "CONTAINMENT ........ FAILING", "TIME DILATION ...... 0.94" },
        { "TEMPORAL FIELD MONITOR", "FIELD RADIUS ....... EXPANDING", "ANOMALIES .......... 17 DETECTED", "LOCAL TIME ......... UNSTABLE" },
        { "PERSONNEL FILES", "ACCESS DENIED", "DIRECTOR VOSS", "CLEARANCE ONLY" },
        { "SECURITY CAMERA NETWORK", "ALL FEEDS LOST", "NETWORK OVERRIDDEN", "BY DIRECTOR VOSS" },
        { "REACTOR COOLANT", "PUMPS 1 TO 3 ....... ONLINE", "PUMP 4 ............. OFFLINE", "CORE TEMPERATURE ... RISING" },
        { "", "", "", "" },
        { "", "", "", "" },
        { "COMMUNICATIONS", "SURFACE UPLINK", "JAMMED", "NO CARRIER" },
        { "", "", "", "" },
      };
      int m = termMsg;
      drawTextLeft(msg[m][0], lx, TY0 + 100, 3.4f, g, true);
      uint16_t c2 = (m == 2 || m == 3 || m == 7) ? red : g;
      for (int k = 1; k < 4; k++) drawTextLeft(msg[m][k], lx + 20, TY0 + 140 + k * 56, 2.8f, k == 1 ? c2 : g, true);
      if (cur) drawTextLeft("PRESS X TO RETURN", lx, TY1 - 30, 2.2f, gd, false);
      break;
    }
    case TP_UNLOCK:
      drawVectorText("SUB BASEMENT", (TX0 + TX1) / 2, TY0 + 110, 4.0f, g, true);
      drawVectorText("LIFT ACCESS UNLOCKED", (TX0 + TX1) / 2, TY0 + 160, 4.0f, g, true);
      drawVectorText("PROCEED TO", (TX0 + TX1) / 2, TY0 + 230, 2.8f, g, false);
      snprintf(b, sizeof(b), "LIFT %c", shaftLetter(subShaft));
      drawVectorTextW(b, (TX0 + TX1) / 2, TY0 + 300, 7.0f, rgb565(255, 220, 80), 3.0f);
      snprintf(b, sizeof(b), "RIDE LIFT %c TO THE BOTTOM AND PRESS DOWN", shaftLetter(subShaft));
      drawVectorText(b, (TX0 + TX1) / 2, TY0 + 372, 2.4f, g, false);
      if (cur) drawTextLeft("PRESS X TO RETURN", lx, TY1 - 30, 2.2f, gd, false);
      break;
  }
  if (termPage == TP_LOCKDOWN) {
    float mid = (TX0 + TX1) / 2.0f;
    drawVectorText("QUANTUM LOCKDOWN", mid, TY0 + 100, 4.4f, g, true);
    drawVectorText(lockdownWasOn ? "ALREADY ACTIVE" : "ENGAGED", mid, TY0 + 150, 3.4f, rgb565(255, 220, 80), true);
    snprintf(b, sizeof(b), "%d OF %d NUCLEAR SITES HACKED - FINAL", sitesHacked, NUKE_SITES);
    drawVectorText(b, mid, TY0 + 214, 2.6f, g, false);
    drawVectorText("NO FURTHER SITES CAN BE HACKED", mid, TY0 + 252, 2.6f, g, false);
    if (sitesHacked > sitesLaunched) drawVectorText("WARNING: LAUNCH SEQUENCE ACTIVE FOR THE HACKED SITES", mid, TY0 + 310, 2.2f, red, true);
    if (cur) drawTextLeft("PRESS X TO RETURN", lx, TY1 - 30, 2.2f, gd, false);
  }
  if (cur && termPage != TP_UNLOCK && termPage != TP_LOCKDOWN) fillRect(TX1 - 44, TY1 - 34, TX1 - 30, TY1 - 22, g);   // the cursor
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
    briefingOn = true;                   // STAGE 1's briefing, then he can go
    briefingT = 0;
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
    int top = d.y - dH(i);
    if ((d.kind < DEC_KINDS && ri.searched[i]) || (d.kind == DEC_LAB && s2Searched[curRoom][i])) {
      uint16_t c = ri.holds[i] ? rgb565(255, 210, 70) : (ri.docs[i] ? rgb565(255, 90, 70) : rgb565(80, 230, 120));   // (gold: a drive; red: documents)
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
    int x0 = d.x - 36, y0 = d.y - dH(i) - 30;
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
// A QUANTUM DRIVE, lying flat (cx, cy its middle; s its size: 1 = 120 units long)
static void drawQuantumDrive(float cx, float cy, float s, bool glow) {
  float x0 = cx - 60 * s, x1 = cx + 60 * s, y0 = cy - 20 * s, y1 = cy + 20 * s;
  fillRectF(x0, y0, x1, y1, rgb565(36, 40, 50));                            // the case
  fillRectF(x0, y0, x1, y0 + 3 * s, rgb565(110, 118, 136));
  fillRectF(x0, y1 - 3 * s, x1, y1, rgb565(20, 22, 28));
  for (int k = 0; k < 8; k++) fillRectF(x1, y0 + (6 + k * 4) * s, x1 + 6 * s, y0 + (8 + k * 4) * s, rgb565(220, 180, 70));   // the contacts
  fillRectF(x0 + 8 * s, cy - 4 * s, x1 - 30 * s, cy + 4 * s, glow ? rgb565(90, 230, 255) : rgb565(40, 90, 110));   // the core's glowing strip
  if (glow) glowDisc((x0 + x1 - 22 * s) / 2, cy, 70 * s, 60, 200, 255, 0.35f);
  drawVectorText("Q", x1 - 18 * s, cy, 2.2f * s, rgb565(200, 230, 255), true);
  fillRectF(x0 + 8 * s, y0 + 8 * s, x0 + 14 * s, y0 + 12 * s, rgb565(80, 255, 120));   // its lamp
}

// A buff folder stamped TOP SECRET (NUCLEAR MISSION's)
static void drawFolderIcon(int cx, int cy, const char *stamp) {
  fillRect(cx - 50, cy - 40, cx - 10, cy - 30, rgb565(200, 168, 100));   // tab
  fillRect(cx - 50, cy - 32, cx + 50, cy + 34, rgb565(214, 184, 116));
  fillRect(cx - 46, cy - 36, cx + 46, cy - 30, rgb565(240, 236, 224));   // papers inside
  drawRectOutline(cx - 50, cy - 32, cx + 50, cy + 34, rgb565(160, 128, 70), 1);
  int hw = (int)(strlen(stamp) * 6 * 1.4f / 2) + 6;
  drawRectOutline(cx - hw, cy - 10, cx + hw, cy + 12, rgb565(200, 30, 30), 2);
  drawVectorText(stamp, cx, cy + 1, 1.4f, rgb565(200, 30, 30), true);
}

// TOP SECRET DOCUMENTS, as NUCLEAR MISSION's panel: the folder, its points
static void drawDocPanel() {
  int x0 = 410, y0 = 120, x1 = 870, y1 = 340;
  fillRect(x0, y0, x1, y1, rgb565(10, 12, 20));
  drawRectOutline(x0, y0, x1, y1, rgb565(255, 210, 70), 3);
  drawVectorText("TOP SECRET DOCUMENTS", SCREEN_W / 2, y0 + 30, 3.2f, rgb565(255, 210, 70), true);
  drawFolderIcon(SCREEN_W / 2, y0 + 104, "TOP SECRET");
  char b[48];
  snprintf(b, sizeof(b), "+%ld POINTS", docValue);
  drawVectorText(b, SCREEN_W / 2, y1 - 62, 3.4f, rgb565(100, 240, 140), true);
  snprintf(b, sizeof(b), "%d OF %d FOUND", docsFound, DOCUMENTS);
  drawVectorText(b, SCREEN_W / 2, y1 - 26, 2.4f, rgb565(200, 206, 220), false);
}

// QUANTUM DRIVE FOUND, as NUCLEAR MISSION's panel: a box with the find in it
static void drawItemPanel() {
  int x0 = 410, y0 = 120, x1 = 870, y1 = 316;
  fillRect(x0, y0, x1, y1, rgb565(10, 12, 20));
  drawRectOutline(x0, y0, x1, y1, rgb565(90, 220, 255), 3);
  drawVectorText("QUANTUM DRIVE FOUND", SCREEN_W / 2, y0 + 30, 3.4f, rgb565(90, 220, 255), true);
  drawQuantumDrive(SCREEN_W / 2, y0 + 96, 1.2f, fmodf(clockT, 0.6f) < 0.45f);
  char b[48];
  snprintf(b, sizeof(b), "+%d   %d OF %d FOUND", FOUND_SCORE, itemsFound, ITEMS_HIDDEN);
  drawVectorText(b, SCREEN_W / 2, y1 - 30, 2.6f, rgb565(100, 240, 140), true);
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
  if (bodyT >= 0) {                      // stage 2: the maintenance man will be found (in the bin: the mission's clock again)
    int bs = (int)ceilf(bodyT);
    snprintf(b, sizeof(b), "BODY FOUND IN %d:%02d", bs / 60, bs % 60);
    bool fl = bs <= 10 && fmodf(clockT, 0.5f) < 0.25f;
    drawVectorText(b, SCREEN_W / 2, HUD_H / 2, 2.8f, fl ? rgb565(120, 30, 24) : rgb565(255, 70, 50), true);
  } else {
  int secs = (int)ceilf(timeLeft > 0 ? timeLeft : 0);
  snprintf(b, sizeof(b), "%d:%02d", secs / 60, secs % 60);
  bool lastMin = secs <= 60, flash = secs <= 10 && fmodf(clockT, 0.5f) < 0.25f;
  drawVectorText(b, SCREEN_W / 2, HUD_H / 2, 3.2f, flash ? rgb565(120, 30, 24) : (lastMin ? rgb565(255, 70, 50) : rgb565(255, 190, 70)), true);
  }
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


static void drawFacilityTerminal() {
  drawTermCase("FT-7 FACILITY TERMINAL");
  uint16_t g = rgb565(100, 255, 140), gd = rgb565(40, 140, 70), dark = rgb565(4, 18, 9), y = rgb565(255, 220, 80);
  bool cur = ((int)(termT * 2.5f) & 1);
  float lx = TX0 + 30, mid = (TX0 + TX1) / 2.0f;
  drawTextLeft("FACILITY NETWORK   TERMINAL FT-7", lx, TY0 + 26, 2.2f, gd, false);
  fillRect(TX0 + 18, TY0 + 46, TX1 - 18, TY0 + 47, gd);
  char b[64];
  switch (termPage) {
    case FT_LOGIN: {                          // typed in, a character at a time
      const char *user = "MAINTENANCE", *pass = "**********";
      int n1 = clampv((int)(termT * 14), 0, (int)strlen(user)), n2 = clampv((int)((termT - 0.9f) * 14), 0, (int)strlen(pass));
      snprintf(b, sizeof(b), "LOGIN:     %.*s", n1, user);
      drawTextLeft(b, lx, TY0 + 100, 2.8f, g, true);
      if (termT > 0.9f) {
        snprintf(b, sizeof(b), "PASSWORD:  %.*s", n2, pass);
        drawTextLeft(b, lx, TY0 + 150, 2.8f, g, true);
      }
      if (termT > 1.8f) drawTextLeft("ACCESS GRANTED", lx, TY0 + 220, 3.4f, y, true);
      break;
    }
    case FT_MENU:
      drawTextLeft("LOGGED IN: MAINTENANCE. SELECT:", lx, TY0 + 84, 2.4f, g, false);
      for (int i = 0; i < FT_ITEMS; i++) {
        int yy = TY0 + 130 + i * 46;
        snprintf(b, sizeof(b), "%d  %s", i + 1, ftItems[i]);
        if (i == termSel) {
          fillRect(TX0 + 20, yy - 18, TX1 - 20, yy + 18, rgb565(70, 220, 120));
          drawTextLeft(b, lx + 12, yy, 3.0f, dark, true);
        } else {
          drawTextLeft(b, lx + 12, yy, 3.0f, i <= 1 ? y : g, true);
        }
      }
      if (blueprint) drawTextLeft("BLUEPRINT ON YOUR MAP", lx + 560, TY0 + 130, 1.8f, gd, false);
      if (driveScan) drawTextLeft("DRIVES ON YOUR MAP", lx + 560, TY0 + 176, 1.8f, gd, false);
      drawTextLeft("UP / DOWN TO CHOOSE    X TO SELECT", lx, TY1 - 22, 1.8f, gd, false);
      break;
    case FT_DOWNLOAD: {
      float k = fminf(1.0f, termT / FT_DOWNLOAD_SECONDS);
      drawTextLeft("DOWNLOADING FACILITY BLUEPRINT...", lx, TY0 + 100, 2.8f, g, true);
      static const char *const files[5] = { "LEVEL PLANS", "LIFT SHAFTS", "ROOM INDEX", "POWER GRID", "GENERATOR LOCATIONS" };
      for (int f = 0; f < 5; f++)
        if (k > f * 0.2f) {
          snprintf(b, sizeof(b), "%-20s %s", files[f], k >= (f + 1) * 0.2f ? "OK" : "...");
          drawTextLeft(b, lx + 20, TY0 + 150 + f * 34, 2.4f, g, false);
        }
      drawRectOutline((int)lx, TY0 + 340, (int)lx + 604, TY0 + 364, g, 1);
      fillRect((int)lx + 2, TY0 + 342, (int)lx + 2 + (int)(600 * k), TY0 + 362, g);
      snprintf(b, sizeof(b), "%d%%", (int)(k * 100));
      drawTextLeft(b, lx + 620, TY0 + 352, 2.6f, g, true);
      break;
    }
    case FT_DONE:
      drawVectorText("BLUEPRINT DOWNLOADED", mid, TY0 + 110, 4.4f, g, true);
      drawVectorText("POWER GENERATOR LOCATIONS", mid, TY0 + 180, 3.0f, y, true);
      drawVectorText("ARE NOW SHOWN ON YOUR MAP", mid, TY0 + 220, 3.0f, y, true);
      drawVectorText("THEIR ROOMS ARE MARKED IN YELLOW", mid, TY0 + 280, 2.4f, g, false);
      if (cur) drawTextLeft("PRESS X TO RETURN", lx, TY1 - 30, 2.2f, gd, false);
      break;
    case FT_NOPOWER: {
      uint16_t red = rgb565(255, 90, 70);
      drawVectorText("QUANTUM DRIVE SCAN", mid, TY0 + 100, 3.4f, g, true);
      drawVectorText("ERROR: NOT ENOUGH POWER", mid, TY0 + 170, 4.0f, red, true);
      snprintf(b, sizeof(b), "POWER GENERATORS ONLINE: %d OF %d", generatorsOn(), GENERATORS);
      drawVectorText(b, mid, TY0 + 236, 2.6f, red, false);
      drawVectorText("TURN ON ALL THE POWER GENERATORS", mid, TY0 + 290, 2.6f, y, true);
      drawVectorText("TO RUN THE SCAN", mid, TY0 + 324, 2.6f, y, true);
      if (cur) drawTextLeft("PRESS X TO RETURN", lx, TY1 - 30, 2.2f, gd, false);
      break;
    }
    case FT_SCAN: {
      int done = clampv((int)(termT / SCAN_ROOM_SECONDS), 0, ROOM_COUNT);
      int found = 0;
      for (int i = 0; i < done; i++) found += driveLeftIn(i);
      int secs = (int)ceilf(fmaxf(0, ROOM_COUNT * SCAN_ROOM_SECONDS - termT));
      drawTextLeft("QUANTUM DRIVE SCAN", lx, TY0 + 84, 3.0f, g, true);
      snprintf(b, sizeof(b), "TIME REMAINING  0:%02d", secs);
      drawTextLeft(b, lx + 470, TY0 + 84, 2.6f, y, true);
      for (int k = 0; k < 7; k++) {          // the last rooms scanned
        int i = done - 7 + k;
        if (i < 0) continue;
        bool hit = driveLeftIn(i);
        snprintf(b, sizeof(b), "ROOM %02d ...... %s", i + 1, hit ? "DRIVE DETECTED" : "CLEAR");
        drawTextLeft(b, lx + 10, TY0 + 130 + k * 30, 2.2f, hit ? y : gd, hit);
      }
      if (done < ROOM_COUNT && cur) {
        snprintf(b, sizeof(b), "ROOM %02d ...... SCANNING", done + 1);
        drawTextLeft(b, lx + 10, TY0 + 130 + 7 * 30, 2.2f, g, true);
      }
      // the sweep: a radar circle, its arm going round, a blip for each find
      float cx = TX1 - 150, cy = TY0 + 230, R = 90;
      for (int ring = 1; ring <= 3; ring++)
        for (int a = 0; a < 48; a++) {
          float t0 = a * 6.2832f / 48, t1 = (a + 1) * 6.2832f / 48, rr = R * ring / 3;
          drawLine((int)(cx + cosf(t0) * rr), (int)(cy + sinf(t0) * rr), (int)(cx + cosf(t1) * rr), (int)(cy + sinf(t1) * rr), gd, false);
        }
      float arm = termT * 5.0f;
      for (int k = 0; k < 6; k++) {
        float a = arm - k * 0.08f;
        drawLine((int)cx, (int)cy, (int)(cx + cosf(a) * R), (int)(cy + sinf(a) * R), k == 0 ? g : gd, k == 0);
      }
      for (int i = 0, n = 0; i < done; i++)
        if (driveLeftIn(i)) {
          float a = i * 2.39996f, rr = 25 + (n++ % 3) * 22;
          fillCircle((int)(cx + cosf(a) * rr), (int)(cy + sinf(a) * rr), 4, y);
        }
      float k = fminf(1.0f, termT / (ROOM_COUNT * SCAN_ROOM_SECONDS));
      drawRectOutline((int)lx, TY0 + 370, (int)lx + 604, TY0 + 392, g, 1);
      fillRect((int)lx + 2, TY0 + 372, (int)lx + 2 + (int)(600 * k), TY0 + 390, g);
      snprintf(b, sizeof(b), "%d OF %d ROOMS   %d WITH DRIVES", done, ROOM_COUNT, found);
      drawTextLeft(b, lx + 520, TY0 + 346, 2.0f, g, false);
      break;
    }
    case FT_SCAN_DONE: {
      int n = 0;
      for (int i = 0; i < ROOM_COUNT; i++) n += driveLeftIn(i);
      drawVectorText("SCAN COMPLETE", mid, TY0 + 110, 4.4f, g, true);
      snprintf(b, sizeof(b), "%d ROOMS WITH QUANTUM DRIVES TO FIND", n);
      drawVectorText(b, mid, TY0 + 180, 3.0f, y, true);
      drawVectorText("ARE NOW SHOWN ON YOUR MAP", mid, TY0 + 220, 3.0f, y, true);
      drawVectorText("THEIR ROOMS ARE MARKED IN BLUE", mid, TY0 + 280, 2.4f, g, false);
      if (cur) drawTextLeft("PRESS X TO RETURN", lx, TY1 - 30, 2.2f, gd, false);
      break;
    }
    case FT_STATUS: {
      int n = 0;
      for (int i = 0; i < ROOM_COUNT; i++) n += rooms[i].gen && rooms[i].genOn;
      drawTextLeft("FACILITY STATUS", lx, TY0 + 100, 3.4f, g, true);
      snprintf(b, sizeof(b), "POWER GENERATORS ... %d OF %d ONLINE", n, GENERATORS);
      drawTextLeft(b, lx + 20, TY0 + 160, 2.6f, n < GENERATORS ? rgb565(255, 90, 70) : g, true);
      int secs = (int)fmaxf(0, timeLeft);
      snprintf(b, sizeof(b), "POWER FAILURE IN ... %d:%02d", secs / 60, secs % 60);
      drawTextLeft(b, lx + 20, TY0 + 210, 2.6f, g, true);
      snprintf(b, sizeof(b), "QUANTUM DRIVES ..... %d OF %d MISSING", ITEMS_HIDDEN - itemsFound, ITEMS_HIDDEN);
      drawTextLeft(b, lx + 20, TY0 + 260, 2.6f, g, true);
      drawTextLeft("MAIN COMPUTER ...... QUANTUM CONTROL", lx + 20, TY0 + 310, 2.6f, g, true);
      if (cur) drawTextLeft("PRESS X TO RETURN", lx, TY1 - 30, 2.2f, gd, false);
      break;
    }
    case FT_LOG:
      drawTextLeft("MAINTENANCE LOG", lx, TY0 + 100, 3.4f, g, true);
      drawTextLeft("0412  GENERATORS TRIPPED IN THE SURGE", lx + 20, TY0 + 160, 2.4f, g, false);
      drawTextLeft("0415  RESTART: LEVER ON EACH GENERATOR", lx + 20, TY0 + 200, 2.4f, g, false);
      drawTextLeft("0420  QUANTUM DRIVES PULLED FROM THE BAY", lx + 20, TY0 + 240, 2.4f, g, false);
      drawTextLeft("0421  STAFF EVACUATED BY QUANTUMLOOP", lx + 20, TY0 + 280, 2.4f, g, false);
      drawTextLeft("0433  BACKUP POWER FAILING", lx + 20, TY0 + 320, 2.4f, rgb565(255, 90, 70), false);
      if (cur) drawTextLeft("PRESS X TO RETURN", lx, TY1 - 30, 2.2f, gd, false);
      break;
  }
  if (cur && termPage != FT_DONE && termPage != FT_SCAN_DONE && termPage != FT_NOPOWER && termPage != FT_SCAN) fillRect(TX1 - 44, TY1 - 34, TX1 - 30, TY1 - 22, g);
}

// Stage 2's map: lifts A..F, the rooms on their right (filled once he's been in), the corridors
// between; him blinking. big: the OPTIONS map (names, access levels)
static void drawLabMap(int x0, int y0, int x1, int y1, bool big, uint16_t ink, uint16_t paper) {
  int RW = big ? 128 : 54, SW = big ? 40 : 16, RH = big ? 50 : 12, GAP = big ? 20 : 6;
  int mw = S2_LIFTS * (RW + SW), mh = (S2_LEVELS + 1) * RH + S2_LEVELS * GAP;
  int left = (x0 + x1 - mw) / 2, top = (y0 + y1 - mh) / 2;
  auto rowY = [&](int f) { return top + f * (RH + GAP); };
  auto shaftX = [&](int s) { return left + s * (RW + SW) + SW / 2; };
  for (int s = 0; s < S2_LIFTS; s++) {
    int lo = topStop(s), hi = bottomStop(s);
    int x = shaftX(s);
    fillRect(x - (big ? 2 : 1), rowY(lo) + 2, x + (big ? 2 : 1), rowY(hi) + RH - 2, ink);
    if (big) {
      char b[2] = { (char)('A' + s), 0 };
      drawVectorText(b, x, rowY(lo) - 22, 3.0f, ink, true);
    }
  }
  for (int f = 0; f <= S2_ARRIVAL; f++)
    for (int s = 0; s < S2_LIFTS; s++) {
      int idx = f == S2_ARRIVAL ? (s == 0 ? ARRIVAL_ROOM : -1) : s2Room[s][f];
      if (idx < 0) continue;
      int rx = shaftX(s) + SW / 2 + 2, ry = rowY(f), cyy = ry + RH / 2;
      int rx1 = rx + RW - 4;
      bool seen = idx == ARRIVAL_ROOM || rooms[idx].visited;
      fillRect(shaftX(s), cyy - (big ? 1 : 0), rx, cyy + (big ? 1 : 0), ink);            // the corridor from its lift...
      if (idx != ARRIVAL_ROOM && !rooms[idx].noRightDoor) fillRect(rx1, cyy - (big ? 1 : 0), shaftX(s + 1), cyy + (big ? 1 : 0), ink);   // ...and to the next
      if (seen) fillRect(rx, ry, rx1, ry + RH - 1, ink);
      else drawRectOutline(rx, ry, rx1, ry + RH - 1, ink, big ? 2 : 1);
      if (big) {
        char b[24];
        if (idx == ARRIVAL_ROOM) snprintf(b, sizeof(b), "QUANTUMLOOP");
        else if (seen || s2Kind[idx] != LK_LAB) s2RoomName(idx, b, sizeof(b));
        else snprintf(b, sizeof(b), "LEVEL %d", s2Lvl[idx]);
        drawVectorText(b, (rx + rx1) / 2.0f, cyy - (idx == ARRIVAL_ROOM ? 0 : 8), 1.5f, seen ? paper : ink, false);
        if (idx != ARRIVAL_ROOM) {
          snprintf(b, sizeof(b), s2Locked(idx) ? "LOCKED L%d" : "L%d", s2Lvl[idx]);
          drawVectorText(b, (rx + rx1) / 2.0f, cyy + 12, 1.3f, seen ? paper : ink, false);
        }
      } else if (idx != ARRIVAL_ROOM && s2Kind[idx] == LK_KITCHEN) {
        drawVectorText("K", (rx + rx1) / 2.0f, (float)cyy, 1.2f, seen ? paper : ink, false);
      }
    }
  if (fmodf(clockT, 0.6f) < 0.4f) {           // him
    float x, y;
    if (mode == MODE_SHAFT) {
      x = (float)shaftX(curShaft);
      float f = carWorldY / LEVEL_GAP;
      int f0 = (int)f;
      y = rowY(f0) + RH / 2 + (rowY(f0 + 1) - rowY(f0)) * (f - f0);
    } else {
      int c = curRoom == ARRIVAL_ROOM ? 0 : rooms[curRoom].col, r = curRoom == ARRIVAL_ROOM ? S2_ARRIVAL : rooms[curRoom].row;
      x = shaftX(c) + SW / 2 + 2 + (RW - 4) / 2.0f;
      y = rowY(r) + RH / 2.0f;
    }
    int d = big ? 7 : 3;
    fillRect((int)x - d, (int)y - d, (int)x + d, (int)y + d, rgb565(250, 250, 240));
    drawRectOutline((int)x - d - 1, (int)y - d - 1, (int)x + d + 1, (int)y + d + 1, big ? rgb565(255, 70, 50) : ink, 1);
  }
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
  if (stage == 2) {                                               // the QUANTUM LAB
    drawLabMap(gx0, gy0, gx1, gy1, false, rgb565(40, 30, 10), rgb565(236, 206, 64));
    return;
  }
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
  for (int i = 0; i <= CONTROL_ROOM; i++) {
    const RoomInfo &ri = rooms[i];
    int rx = left + SW + ri.col * (RW + SW), ry = top + ri.row * (RH + GAP);
    if (i == CONTROL_ROOM && !ri.visited) {   // QUANTUM CONTROL: always marked (an outline till he has been in)
      drawRectOutline(rx + 2, ry, rx + RW - 3, ry + RH - 1, ink, 2);
      continue;
    }
    if (ri.term && ri.visited) {                // the facility terminal's room: purple, a T
      fillRect(rx + 2, ry, rx + RW - 3, ry + RH - 1, rgb565(130, 50, 190));
      drawRectOutline(rx + 2, ry, rx + RW - 3, ry + RH - 1, ink, 1);
      drawVectorText("T", rx + RW / 2.0f, ry + RH / 2.0f, 1.3f, rgb565(250, 240, 255), true);
      continue;
    }
    if (driveScan && driveLeftIn(i)) {         // the drive scan: rooms with a drive to find, blue
      fillRect(rx + 2, ry, rx + RW - 3, ry + RH - 1, rgb565(40, 110, 235));
      drawRectOutline(rx + 2, ry, rx + RW - 3, ry + RH - 1, ink, 1);
      drawVectorText(blueprint && ri.gen ? "QG" : "Q", rx + RW / 2.0f, ry + RH / 2.0f, 1.3f, rgb565(240, 246, 255), true);
      continue;
    }
    if (blueprint && ri.gen && i < ROOM_COUNT) {   // the blueprint: the generators' rooms, orange (green once on)
      fillRect(rx + 2, ry, rx + RW - 3, ry + RH - 1, ri.genOn ? rgb565(40, 150, 70) : rgb565(220, 90, 20));
      drawRectOutline(rx + 2, ry, rx + RW - 3, ry + RH - 1, ink, 1);
      drawVectorText("G", rx + RW / 2.0f, ry + RH / 2.0f, 1.3f, rgb565(250, 246, 220), true);
      continue;
    }
    if (!ri.visited) continue;
    fillRect(rx + 2, ry, rx + RW - 3, ry + RH - 1, ink);
    if (i == CONTROL_ROOM) fillRect(rx + 8, ry + 5, rx + RW - 9, ry + RH - 6, rgb565(236, 206, 64));
    int yl = floorY(ri.row * 2 + (ri.topL ? 0 : 1)), yr = floorY(ri.row * 2 + (ri.topR ? 0 : 1));
    fillRect(rx - SW / 2, yl - 1, rx + 2, yl + 1, ink);                // the corridors to the shafts
    fillRect(rx + RW - 3, yr - 1, rx + RW + SW / 2, yr + 1, ink);
  }
  if (fmodf(clockT, 0.6f) < 0.4f) {           // him
    int x = left + curShaft * (RW + SW) + SW / 2;
    float f = fminf(carWorldY / LEVEL_GAP, FLOORS - 1.0f);   // (the sub basement isn't on it: he shows at the bottom)
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
  // on the left: the nuclear launch sites
  bevelBox(20, PANEL_Y + 16, 372, SCREEN_H - 18, rgb565(70, 78, 120));
  char b[40];
  drawNukePanel(20, PANEL_Y + 16, 372, SCREEN_H - 18);
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
  if (stage == 2) {                            // the lab: rooms, his card, his disguise, the cabinets left
    seen = 0;
    int aid = 0;
    for (int i = 0; i < S2_ROOMS; i++) seen += rooms[i].visited;
    for (int s2 = 0; s2 < S2_LIFTS; s2++)
      for (int f = 0; f < S2_LEVELS; f++) aid += s2Stops(s2, f) && !s2Aid[s2][f];
    snprintf(b, sizeof(b), "%d/%d", seen, S2_ROOMS);
    line("ROOMS", b, PANEL_Y + 38, rgb565(240, 240, 245));
    if (s2AllAccess) snprintf(b, sizeof(b), "ALL (TEST)");
    else if (s2Card > 0) snprintf(b, sizeof(b), "LEVEL %d", s2Card);
    else snprintf(b, sizeof(b), "NONE");
    line("ACCESS CARD", b, PANEL_Y + 68, rgb565(90, 220, 255));
    line("DISGUISE", s2Disguised ? "OVERALLS" : "NONE", PANEL_Y + 98, rgb565(255, 220, 80));
    snprintf(b, sizeof(b), "%d", aid);
    line("FIRST AID LEFT", b, PANEL_Y + 128, rgb565(120, 230, 140));
    line("WEAPONS", hasKnife && hasGun ? "KNIFE PISTOL" : hasKnife ? "KNIFE" : hasGun ? "PISTOL" : "NONE", PANEL_Y + 158, rgb565(240, 240, 245));
    snprintf(b, sizeof(b), "V%s  FPS %d", GAME_VERSION, (int)lroundf(fpsShown));
    drawVectorText(b, (lx + SCREEN_W - 44) / 2, PANEL_Y + 184, 1.5f, rgb565(150, 160, 186), false);
    return;
  }
  snprintf(b, sizeof(b), "%d/%d", seen, ROOM_COUNT);
  line("ROOMS", b, PANEL_Y + 38, rgb565(240, 240, 245));
  snprintf(b, sizeof(b), "%d/%d", itemsFound, ITEMS_HIDDEN);
  line("DRIVES FOUND", b, PANEL_Y + 68, rgb565(90, 220, 255));
  snprintf(b, sizeof(b), "%d/%d", drivesLoaded, ITEMS_HIDDEN);
  line("DRIVES LOADED", b, PANEL_Y + 98, rgb565(90, 220, 255));
  snprintf(b, sizeof(b), "%d/%d", generatorsOn(), GENERATORS);
  line("GENERATORS ON", b, PANEL_Y + 128, rgb565(255, 220, 80));
  snprintf(b, sizeof(b), "%d", kitsLeft);
  line("FIRST AID LEFT", b, PANEL_Y + 158, rgb565(120, 230, 140));
  snprintf(b, sizeof(b), "V%s  FPS %d", GAME_VERSION, (int)lroundf(fpsShown));
  drawVectorText(b, (lx + SCREEN_W - 44) / 2, PANEL_Y + 184, 1.5f, rgb565(150, 160, 186), false);
}

// GAME OVER: why, and the score
static float gameOverShownT() { return gameOverT - (gameOver == END_TIME ? POWER_FAIL_PANEL : 0.0f); }   // (the power failing first)
static void drawGameOver() {
  float gameOverT = scene == SC_STAGE_CLEAR ? sceneT : gameOverShownT();
  if (gameOverT < 0) return;
  float k = gameOverT / 1.0f;
  if (k > 1) k = 1;
  blendRect(0, HUD_H, SCREEN_W - 1, SCREEN_H - 1, 0, 0, 0, 0.65f * k);
  if (gameOverT < 0.4f) return;
  if (scene == SC_STAGE_CLEAR) {
    drawVectorTextW("STAGE 1 COMPLETE", SCREEN_W / 2, 120, 7.0f, rgb565(20, 90, 40), 8.0f);
    drawVectorTextW("STAGE 1 COMPLETE", SCREEN_W / 2, 120, 7.0f, rgb565(100, 255, 140), 3.0f);
    drawVectorText("YOU REACHED THE QUANTUMLOOP", SCREEN_W / 2, 186, 3.2f, rgb565(240, 240, 245), true);
  } else if (gameOver == END_WIN) {
    drawVectorTextW("MISSION COMPLETE", SCREEN_W / 2, 120, 7.0f, rgb565(20, 90, 40), 8.0f);
    drawVectorTextW("MISSION COMPLETE", SCREEN_W / 2, 120, 7.0f, rgb565(100, 255, 140), 3.0f);
    drawVectorText("YOU REACHED THE SUB BASEMENT", SCREEN_W / 2, 186, 3.2f, rgb565(240, 240, 245), true);
  } else if (gameOver == END_TIME) {
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(120, 20, 14), 8.0f);
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(255, 70, 50), 3.0f);
    drawVectorText("POWER FAILURE - THE GENERATORS HAVE SHUT DOWN", SCREEN_W / 2, 186, 3.0f, rgb565(240, 240, 245), true);
  } else if (gameOver == END_NUKES) {
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(120, 20, 14), 8.0f);
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(255, 70, 50), 3.0f);
    drawVectorText("THE NUCLEAR MISSILES HAVE LANDED", SCREEN_W / 2, 186, 3.0f, rgb565(240, 240, 245), true);
  } else if (gameOver == END_BODY) {
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(120, 20, 14), 8.0f);
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(255, 70, 50), 3.0f);
    drawVectorText("THE MAINTENANCE MAN'S BODY WAS FOUND", SCREEN_W / 2, 186, 3.0f, rgb565(240, 240, 245), true);
  } else {
    drawVectorTextW("GAME OVER", SCREEN_W / 2, 120, 8.0f, rgb565(120, 20, 14), 8.0f);
    drawVectorTextW("GAME OVER", SCREEN_W / 2, 120, 8.0f, rgb565(255, 70, 50), 3.0f);
    drawVectorText("OUT OF HEALTH", SCREEN_W / 2, 186, 3.2f, rgb565(240, 240, 245), true);
  }
  // the end of game bonuses, one line at a time
  char b[80];
  int mins = (int)(bonusPts[0] / BONUS_PER_MINUTE);
  const char *what[BONUS_LINES] = { "", "NO ELECTROCUTION", "NO DRONE HITS", "NO FALLS DOWN PITS", "NEVER CAUGHT ON CAMERA", "NO FIRST AID USED" };
  drawVectorText(scene == SC_STAGE_CLEAR ? "STAGE 1 BONUS" : "END OF GAME BONUS", SCREEN_W / 2, 236, 2.6f, rgb565(255, 190, 70), true);
  for (int i = 0; i < BONUS_LINES; i++) {
    if (gameOverT < 0.6f + i * 0.2f) break;
    if (i == 0) snprintf(b, sizeof(b), "TIME LEFT  %d MINUTE%s X %d", mins, mins == 1 ? "" : "S", BONUS_PER_MINUTE);
    else snprintf(b, sizeof(b), "%s", what[i]);
    float y = 272 + i * 30;
    bool got = bonusPts[i] > 0;
    uint16_t c = got ? rgb565(230, 236, 245) : rgb565(110, 116, 130);
    drawVectorText(b, 360 + vectorTextWidth(b, 2.3f) / 2, y, 2.3f, c, false);
    snprintf(b, sizeof(b), "%+ld", bonusPts[i]);
    drawVectorText(got ? b : "-", 920 - vectorTextWidth(got ? b : "-", 2.5f) / 2, y, 2.5f, got ? rgb565(100, 240, 140) : rgb565(110, 116, 130), true);
  }
  if (gameOverT >= 0.6f + BONUS_LINES * 0.2f) {
    fillRect(360, 452, 920, 453, rgb565(90, 100, 120));
    snprintf(b, sizeof(b), "BONUS %+ld", bonusTotal);
    drawVectorText(b, 920 - vectorTextWidth(b, 2.6f) / 2, 474, 2.6f, rgb565(100, 240, 140), true);
    int ps = (int)playT;
    snprintf(b, sizeof(b), "SCORE %ld     TIME PLAYED %d:%02d", score, ps / 60, ps % 60);
    drawVectorText(b, SCREEN_W / 2, 528, 4.0f, rgb565(255, 190, 70), true);
    int seen = 0, total = stage == 2 ? S2_ROOMS : ROOM_COUNT;
    for (int i = 0; i < total; i++) seen += rooms[i].visited;
    if (stage == 2) snprintf(b, sizeof(b), "%d OF %d QUANTUM DRIVES   %d OF %d LAB ROOMS", itemsFound, ITEMS_HIDDEN, seen, total);
    else snprintf(b, sizeof(b), "%d OF %d QUANTUM DRIVES   %d OF %d ROOMS", itemsFound, ITEMS_HIDDEN, seen, ROOM_COUNT);
    drawVectorText(b, SCREEN_W / 2, 572, 2.4f, rgb565(170, 178, 196), false);
  }
  if (gameOverT > 2.2f && fmodf(clockT, 1.0f) < 0.65f) drawVectorText("PRESS X", SCREEN_W / 2, 640, 3.6f, rgb565(240, 240, 245), true);
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
  if (stage == 2) {                                               // the QUANTUM LAB
    drawVectorText("THE QUANTUM LAB", SCREEN_W / 2, 72, 4.0f, rgb565(120, 220, 255), true);
    drawLabMap(60, 100, SCREEN_W - 60, 640, true, rgb565(150, 210, 240), rgb565(10, 20, 30));
    drawVectorText("OPTIONS: BACK TO THE GAME", SCREEN_W / 2, 680, 2.2f, rgb565(200, 206, 220), true);
    return;
  }
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
  for (int i = 0; i <= CONTROL_ROOM; i++) {
    const RoomInfo &ri = rooms[i];
    int x0 = left + SW + ri.col * (RW + SW), y0 = top + ri.row * (RH + GAP);
    if (i == CONTROL_ROOM) {                   // QUANTUM CONTROL: always shown, in its own colour
      fillRect(x0, y0, x0 + RW - 1, y0 + RH - 1, rgb565(60, 36, 96));
      drawRectOutline(x0, y0, x0 + RW - 1, y0 + RH - 1, rgb565(200, 150, 255), 2);
      int yd = floorY(ri.row * 2 + 1);
      fillRect(x0 - SW / 2 + 6, yd - 2, x0 + 8, yd + 3, rgb565(80, 230, 120));
      fillRect(x0 + RW - 9, yd - 2, x0 + RW + SW / 2 - 6, yd + 3, rgb565(80, 230, 120));
      drawVectorText("QUANTUM", x0 + RW / 2, y0 + 18, 1.6f, rgb565(230, 210, 255), true);
      drawVectorText("CONTROL", x0 + RW / 2, y0 + 36, 1.6f, rgb565(230, 210, 255), true);
      if (mode == MODE_ROOM && i == curRoom && blink)
        drawRectOutline(x0 - 4, y0 - 4, x0 + RW + 3, y0 + RH + 3, rgb565(255, 190, 70), 3);
      continue;
    }
    bool bp = blueprint && ri.gen;           // the blueprint: the generators' rooms in yellow (green once on)
    bool ft = ri.term && ri.visited;         // the facility terminal's room, once he has been in it: purple
    bool ds = driveScan && driveLeftIn(i);   // the drive scan: rooms with a drive still to find in blue
    uint16_t fill = ft ? rgb565(130, 50, 190) : ds ? rgb565(40, 110, 235) : bp ? (ri.genOn ? rgb565(40, 140, 70) : rgb565(230, 196, 40)) : ri.visited ? rgb565(40, 64, 96) : rgb565(22, 26, 34);
    uint16_t edge = ft ? rgb565(220, 160, 255) : bp ? rgb565(255, 240, 150) : ds ? rgb565(150, 200, 255) : ri.visited ? rgb565(110, 160, 220) : rgb565(70, 78, 96);
    fillRect(x0, y0, x0 + RW - 1, y0 + RH - 1, fill);
    drawRectOutline(x0, y0, x0 + RW - 1, y0 + RH - 1, edge, 2);
    uint16_t door = rgb565(80, 230, 120);
    int yl = floorY(ri.row * 2 + (ri.topL ? 0 : 1)), yr = floorY(ri.row * 2 + (ri.topR ? 0 : 1));
    fillRect(x0 - SW / 2 + 6, yl - 2, x0 + 8, yl + 3, door);
    fillRect(x0 + RW - 9, yr - 2, x0 + RW + SW / 2 - 6, yr + 3, door);
    char b[8];
    snprintf(b, sizeof(b), "%d", i + 1);
    if (ds && bp) fillRect(x0 + 2, y0 + RH - 14, x0 + RW - 3, y0 + RH - 3, ri.genOn ? rgb565(40, 140, 70) : rgb565(230, 196, 40));   // (both: a yellow strip)
    if (ft) {                                  // a terminal's icon: a screen on a stand
      fillRect(x0 + 7, y0 + 7, x0 + 23, y0 + 19, rgb565(230, 220, 255));
      fillRect(x0 + 9, y0 + 9, x0 + 21, y0 + 17, rgb565(40, 200, 110));
      fillRect(x0 + 13, y0 + 19, x0 + 17, y0 + 27, rgb565(230, 220, 255));
    }
    if (ds) {                                  // a drive's icon
      fillRect(x0 + 8, y0 + 8, x0 + 20, y0 + 26, rgb565(200, 230, 255));
      fillRect(x0 + 12, y0 + 11, x0 + 15, y0 + 23, rgb565(30, 80, 200));
    }
    drawVectorText(b, x0 + RW / 2, y0 + RH / 2 - (ri.visited ? 6 : 0), 2.4f, ds ? rgb565(240, 246, 255) : bp ? rgb565(30, 24, 8) : ri.visited ? rgb565(230, 236, 245) : rgb565(110, 118, 136), true);
    if (ri.visited)   // its equipment: grey not searched, green nothing, gold something found
      for (int k = 0; k < ri.decorN; k++) {
        int dx = x0 + RW / 2 - (ri.decorN - 1) * 7 + k * 14;
        uint16_t c = !ri.searched[k] ? rgb565(80, 88, 104) : (ri.holds[k] ? rgb565(255, 210, 70) : (ri.docs[k] ? rgb565(255, 90, 70) : rgb565(80, 230, 120)));
        fillRect(dx - 4, y0 + RH - 15, dx + 4, y0 + RH - 8, c);
      }
    if ((ri.visited || bp) && ri.gen) {   // a power generator: its lamp, red off, green on
      int gx = x0 + RW - 16, gy = y0 + RH - 18;
      fillRect(gx - 7, gy - 3, gx + 7, gy + 8, rgb565(214, 168, 40));
      fillCircle(gx, gy - 6, 3, ri.genOn ? rgb565(80, 255, 120) : rgb565(230, 40, 30));
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
  // the sub basement, once the main computer has unlocked it: under the bottom row
  if (subUnlocked) {
    const RoomInfo &ri = rooms[SUB_ROOM];
    int x0 = left + SW + subShaft * (RW + SW), y0 = top + ROWS * (RH + GAP), yd = floorY(SUB_FLOOR);
    fillRect(x0, y0, x0 + RW - 1, y0 + 26, ri.visited ? rgb565(70, 70, 74) : rgb565(36, 36, 40));
    drawRectOutline(x0, y0, x0 + RW - 1, y0 + 26, rgb565(255, 220, 80), 2);
    fillRect(x0 - SW / 2 + 6, yd - 2, x0 + 8, yd + 3, rgb565(80, 230, 120));
    drawVectorText("SUB BASEMENT", x0 + RW / 2, y0 + 13, 1.25f, rgb565(255, 220, 80), true);
    if (mode == MODE_ROOM && curRoom == SUB_ROOM && blink) drawRectOutline(x0 - 4, y0 - 4, x0 + RW + 3, y0 + 30, rgb565(255, 190, 70), 3);
  }
  // him, in a shaft
  if (mode == MODE_SHAFT) {
    int x = left + curShaft * (RW + SW) + SW / 2;
    float f = carWorldY / LEVEL_GAP;
    int f0 = (int)f;
    float k = f - f0;
    float y = floorY(f0) + (f0 + 1 <= SUB_FLOOR ? (floorY(f0 + 1) - floorY(f0)) * k : 0);
    fillCircle(x, (int)y, 9, rgb565(10, 12, 16));
    fillCircle(x, (int)y, 7, blink ? rgb565(255, 190, 70) : rgb565(255, 240, 200));
  }
  int bottom = top + ROWS * RH + (ROWS - 1) * GAP;
  bool termKnown = false;
  for (int i = 0; i < ROOM_COUNT; i++) termKnown |= rooms[i].term && rooms[i].visited;
  if (blueprint || driveScan || termKnown) {   // the facility terminal's: what the purple, yellow and blue mean
    char key[160];
    snprintf(key, sizeof(key), "%s%s%s%s%s", termKnown ? "FACILITY TERMINAL IN PURPLE" : "", termKnown && (blueprint || driveScan) ? "     " : "",
             blueprint ? "GENERATORS IN YELLOW" : "", blueprint && driveScan ? "     " : "", driveScan ? "DRIVES TO FIND IN BLUE" : "");
    drawVectorText(key, SCREEN_W / 2, bottom + 64, 2.2f, blink ? rgb565(255, 220, 80) : rgb565(230, 196, 40), true);
  }
  else
    drawVectorText("EACH ROOM: A DOOR AT THE TOP OR THE BOTTOM ON EACH SIDE (GREEN) - THE LIFTS STOP ONLY AT DOORS", SCREEN_W / 2,
                   bottom + 64, 1.8f, rgb565(150, 158, 176), false);
  drawVectorText("OPTIONS: BACK TO THE GAME", SCREEN_W / 2, bottom + 92, 2.2f,
                 rgb565(200, 206, 220), true);
}

// ============================================================ between the stages
// STAGE 1 COMPLETE (its bonuses) -> STAGE 2's briefing -> (play: the switch,
// the doors) -> on board -> the journey -> the arrival station
static void startRide() {
  scene = SC_BOARD_WALK;
  sceneT = 0;
}

// Walk him towards x (true once there)
static bool walkTo(float x, float dt) {
  if (fabsf(agX - x) < 5.0f) {
    if (agState == ST_RUN) setState(ST_IDLE, 0.15f);
    agX = x;
    uint32_t h = held, p = pressed;
    held = pressed = 0;
    updateAgent(dt);
    held = h;
    pressed = p;
    return true;
  }
  uint32_t h = held, p = pressed;
  held = agX < x ? BTN_RIGHT : BTN_LEFT;
  pressed = 0;
  updateAgent(dt);
  held = h;
  pressed = p;
  return false;
}

static void updateScene(float dt) {
  sceneT += dt;
  bool go = (pressed & BTN_JUMP) != 0;
  switch (scene) {
    case SC_STAGE_CLEAR:
      if (go && sceneT > 2.2f) {
        scene = SC_STAGE2_BRIEF;
        sceneT = 0;
        stage = 2;
        layOutStage2();                  // (the lab's rooms take the place of stage 1's)
        timeLeft = GAME_SECONDS;         // stage 2's countdown: 15 minutes again
        warnSlot = 99;
      }
      break;
    case SC_STAGE2_BRIEF:
      if (go && sceneT > 0.8f) {
        scene = SC_NONE;
        showMsg("TURN ON THE QUANTUMLOOP'S POWER", 4.0f);
      }
      break;
    case SC_BOARD_WALK:                  // in through the doors...
      if (walkTo(LOOP_DOOR_X, dt) && sceneT > 0.6f) {
        scene = SC_FADE_OUT;
        sceneT = 0;
      }
      break;
    case SC_FADE_OUT:                    // ...and black...
      if (sceneT >= 1.0f) {
        scene = SC_RIDE;
        sceneT = 0;
      }
      break;
    case SC_RIDE:                        // ...the journey...
      if (sceneT >= RIDE_SECONDS) {
        enterRoom(ARRIVAL_ROOM, true);   // ...the arrival station: he steps out of the doors
        loopPowered = true;
        loopPowerT = 9;
        loopDoorT = 1;
        loopDoorDir = 0;
        placeAgent(LOOP_DOOR_X, ROOM_FLOOR_Y, -1);
        entryX = 36;
        entryFacing = 1;
        scene = SC_ARRIVE;
        sceneT = 0;
      }
      break;
    case SC_ARRIVE:
      if (sceneT < 1.0f) {
        uint32_t h = held; held = 0; updateAgent(dt); held = h;
        break;
      }
      if (loopDoorDir == 0 && loopDoorT >= 1.0f) {
        if (walkTo(LOOP_DOOR_X - 230, dt)) {
          loopDoorDir = -1;
          sfxPlay(SFX_DOORS);
        }
      } else if (loopDoorDir == 0 && loopDoorT <= 0.0f) {
        scene = SC_NONE;
        showMsg("STAGE 2: HEAD LEFT TO THE LIFT", 4.0f);
      } else {
        uint32_t h = held; held = 0; updateAgent(dt); held = h;
      }
      break;
  }
}

// STAGE 2's briefing
static void drawStage2Brief() {
  gfxClipAll();
  float k = fminf(1.0f, sceneT / 0.4f);
  blendRect(0, HUD_H, SCREEN_W - 1, SCREEN_H - 1, 2, 4, 8, 0.75f * k);
  if (sceneT < 0.2f) return;
  int x0 = 250, y0 = 100, x1 = 1030, y1 = 640;
  fillRect(x0, y0, x1, y1, rgb565(10, 12, 20));
  drawRectOutline(x0, y0, x1, y1, rgb565(90, 200, 255), 3);
  fillRect(x0 + 3, y0 + 3, x1 - 3, y0 + 64, rgb565(10, 40, 70));
  drawVectorTextW("STAGE 2", SCREEN_W / 2, y0 + 34, 5.0f, rgb565(120, 220, 255), 2.6f);
  drawVectorText("GET TO THE QUANTUM LAB", SCREEN_W / 2, y0 + 112, 4.0f, rgb565(230, 236, 245), true);
  drawVectorText("AND DISABLE THE QUANTUM CORE", SCREEN_W / 2, y0 + 162, 3.4f, rgb565(255, 90, 70), true);
  // the quantum core: a glowing heart in its orbits
  float qx = SCREEN_W / 2, qy = y0 + 320, t = clockT;
  glowDisc(qx, qy, 120, 40, 120, 255, 0.35f);
  fillCircle((int)qx, (int)qy, 26, rgb565(150, 220, 255));
  fillCircle((int)qx, (int)qy, 16, rgb565(240, 250, 255));
  for (int o = 0; o < 3; o++) {
    float rot = t * (0.6f + o * 0.2f) + o * 1.047f, cr = cosf(rot), sr = sinf(rot);
    float px = 0, py = 0;
    for (int s2 = 0; s2 <= 40; s2++) {
      float u = s2 * TWO_PI / 40, ex = cosf(u) * 110, ey = sinf(u) * 30;
      float x = qx + ex * cr - ey * sr, y = qy + ex * sr + ey * cr;
      if (s2) drawLineF(px, py, x, y, rgb565(90, 150, 230), 1.5f);
      px = x;
      py = y;
    }
    float u = t * (2.2f + o * 0.6f), ex = cosf(u) * 110, ey = sinf(u) * 30;
    glowDisc(qx + ex * cr - ey * sr, qy + ex * sr + ey * cr, 16, 140, 220, 255, 0.8f);
  }
  drawVectorText("TAKE THE QUANTUMLOOP", SCREEN_W / 2, y0 + 446, 2.6f, rgb565(170, 178, 196), false);
  if (sceneT > 0.8f && fmodf(clockT, 1.0f) < 0.65f) drawVectorText("PRESS X TO START", SCREEN_W / 2, y1 - 40, 3.4f, rgb565(240, 240, 245), true);
}

// On board: a seat by the window, the tunnel rushing past
static void drawRide(float t) {
  gfxClip(HUD_H, PLAY_Y1);
  fillRect(0, HUD_H, SCREEN_W - 1, PLAY_Y1, rgb565(176, 180, 188));    // the cabin wall
  for (int x = 0; x < SCREEN_W; x += 160) fillRect(x, 150, x + 2, 640, rgb565(150, 154, 162));
  fillRect(0, HUD_H, SCREEN_W - 1, 150, rgb565(56, 60, 70));              // its ceiling, lit along it
  fillRect(0, 138, SCREEN_W - 1, 146, rgb565(230, 238, 248));
  glowDisc(SCREEN_W / 2, 150, 700, 200, 220, 255, 0.10f);
  // the window band: the tunnel outside
  const int wy0 = 196, wy1 = 404;
  fillRect(40, wy0 - 12, SCREEN_W - 41, wy1 + 12, rgb565(40, 44, 52));
  fillRect(52, wy0, SCREEN_W - 53, wy1, rgb565(6, 8, 12));
  gfxClip(wy0, wy1);
  for (int k = 0; k < 7; k++) {                                          // the tunnel's ribs flashing by
    float x = fmodf(k * 260.0f - t * 1900.0f, 1820.0f);
    if (x < 0) x += 1820.0f;
    x -= 140;
    fillRect((int)x, wy0, (int)x + 22, wy1, rgb565(34, 40, 52));
    fillRect((int)x + 22, wy0, (int)x + 26, wy1, rgb565(60, 70, 88));
  }
  for (int i = 0; i < 44; i++) {                                         // streaks of light
    uint32_t h = (uint32_t)(i + 7) * 2654435761u;
    float sp = 1800 + (h >> 22) % 1600, len = 60 + (h >> 12) % 260;
    float y = wy0 + 8 + (h >> 4) % (wy1 - wy0 - 16);
    float x = SCREEN_W + len - fmodf(t * sp + (h % 2000), SCREEN_W + 2 * len);
    int pick = (h >> 9) % 3;
    uint16_t c = pick == 0 ? rgb565(120, 210, 255) : (pick == 1 ? rgb565(240, 244, 255) : rgb565(255, 170, 80));
    drawLineF(x, y, x + len, y, c, pick == 1 ? 2.5f : 1.6f);
  }
  for (int k = 0; k < 12; k++) {                                         // the rail lights
    float x = fmodf(k * 120.0f - t * 2600.0f, 1440.0f);
    if (x < 0) x += 1440.0f;
    fillRect((int)x - 80, wy1 - 18, (int)x - 40, wy1 - 15, rgb565(255, 220, 120));
  }
  if (fmodf(t, 4.5f) < 0.18f) blendRect(52, wy0, SCREEN_W - 53, wy1, 200, 230, 255, 0.25f);   // a station's lights
  gfxClip(HUD_H, PLAY_Y1);
  for (int k = 1; k < 4; k++) fillRect(k * 320 - 10, wy0 - 12, k * 320 + 10, wy1 + 12, rgb565(150, 154, 162));   // the frames
  blendRect(52, wy0, SCREEN_W - 53, wy0 + 40, 255, 255, 255, 0.05f);    // the glass
  // the display: where it is going, how long
  fillRect(440, 156, 840, 186, rgb565(10, 14, 24));
  char b[48];
  int left = (int)ceilf(fmaxf(0.0f, RIDE_SECONDS - t));
  snprintf(b, sizeof(b), "QUANTUMLOOP   STAGE 2   ARRIVING %d:%02d", left / 60, left % 60);
  drawVectorText(b, SCREEN_W / 2, 171, 1.6f, rgb565(255, 190, 70), true);
  // the seats (a little nearer than the rest: drawn larger), him on one, his head well over its back
  const float RIDE_SCALE = 1.9f;
  const int seatTop = 640 - (int)(22.0f * RIDE_SCALE) - 2;              // the cushion: at his hip
  for (int k = 0; k < 4; k++) {
    int c = 230 + k * 260;
    fillRect(c - 56, seatTop - 64, c + 56, seatTop, rgb565(54, 70, 104));   // the back: up to his neck
    fillRect(c - 56, seatTop - 64, c + 56, seatTop - 58, rgb565(84, 104, 140));
    fillRect(c - 62, seatTop, c + 62, seatTop + 12, rgb565(70, 90, 130));
    fillRect(c - 62, seatTop, c + 62, seatTop + 3, rgb565(96, 116, 156));
    fillRect(c - 6, seatTop + 12, c + 6, 640, rgb565(60, 64, 72));
  }
  fillRect(0, 640, SCREEN_W - 1, PLAY_Y1, rgb565(60, 64, 72));           // the floor
  fillRect(0, 640, SCREEN_W - 1, 644, rgb565(110, 116, 128));
  AgentPose p;
  memset(&p, 0, sizeof(p));
  p.legN = 88; p.kneeN = -86; p.legF = 82; p.kneeF = -84;                // sitting: thighs out, shins down
  p.armN = 26; p.elbowN = 64; p.armF = 20; p.elbowF = 58;                // hands on his knees
  p.torso = -4; p.head = 4 + 3 * sinf(t * 0.9f);
  p.plant = 1;
  p.lift = 1.5f * sinf(t * 7.0f);                                         // (the ride's hum)
  drawAgentU(p, 740, 640, 1, RIDE_SCALE);
}

// ============================================================ the high scores (as NUCLEAR MISSION's)
// Saved in the title's own storage on the PS5 (the rest of /data can't be written)
#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
#ifndef QL_DATA_DIR
#define QL_DATA_DIR "ql_data"             // (the PC test build: a folder where it runs)
#endif
#else
#ifndef QL_DATA_DIR
#define QL_DATA_DIR "/download0/quantum-lockdown"
#endif
#endif
#define HI_FILE QL_DATA_DIR "/hiscores.bin"

static void loadHiScores() {
  FILE *f = fopen(HI_FILE, "rb");
  if (!f) return;
  HiEntry t[HI_COUNT];
  size_t got = fread(t, 1, sizeof(t), f);
  fclose(f);
  if (got != sizeof(t)) return;
  for (int i = 0; i < HI_COUNT; i++)
    if (t[i].score < 0 || t[i].secs < 0 || t[i].name[3] != 0) return;   // (not ours: keep the defaults)
  memcpy(hiTable, t, sizeof(t));
}

static void saveHiScores() {
  mkdir(QL_DATA_DIR, 0777);
  FILE *f = fopen(HI_FILE, "wb");
  if (!f) return;
  fwrite(hiTable, 1, sizeof(hiTable), f);
  fclose(f);
}

static void hiLine(char *b, int n, const HiEntry &e, int rank) {
  snprintf(b, n, "%d.   %.3s   %07ld   %3d:%02d", rank, e.name, (long)(e.score % 10000000), (int)(e.secs / 60 % 1000), (int)(e.secs % 60));
}

static void goToTitle();

// The game is over (or won): a place in the table? then the initials, else the title
static void afterGame() {
  entrySlot = -1;
  for (int i = 0; i < HI_COUNT; i++)
    if (score > hiTable[i].score) { entrySlot = i; break; }
  if (entrySlot < 0 || score <= 0) {
    goToTitle();
    return;
  }
  entryOn = true;
  entryPos = 0;
  entryT = 0;
  strcpy(entryName, "AAA");
  sfxPlay(SFX_FANFARE);
}

static void updateEntry(float dt) {
  entryT += dt;
  char &c = entryName[entryPos];
  if (pressed & BTN_UP) { c = (c >= 'Z') ? 'A' : c + 1; sfxPlay(SFX_KEY); }
  if (pressed & BTN_DOWN) { c = (c <= 'A') ? 'Z' : c - 1; sfxPlay(SFX_KEY); }
  if (pressed & BTN_LEFT) { if (entryPos > 0) entryPos--; sfxPlay(SFX_CLICK); }
  if ((pressed & (BTN_USE | BTN_JUMP | BTN_RIGHT)) && entryT > 0.3f) {
    sfxPlay(SFX_CLICK);
    entryPos++;
    if (entryPos >= 3) {               // done: into the table
      for (int i = HI_COUNT - 1; i > entrySlot; i--) hiTable[i] = hiTable[i - 1];
      memcpy(hiTable[entrySlot].name, entryName, 4);
      hiTable[entrySlot].score = (int32_t)score;
      hiTable[entrySlot].secs = (int32_t)playT;
      saveHiScores();
      sfxPlay(SFX_FANFARE);
      entryOn = false;
      goToTitle();
    }
  }
}

// NEW HIGH SCORE: the initials, three letters, and the table as it will be
static void drawEntry() {
  gfxClipAll();
  fillRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, rgb565(6, 8, 16));
  for (int k = 0; k < 90; k++) {
    int x = (k * 263) % SCREEN_W, y = (k * 151) % SCREEN_H;
    fillRect(x, y, x, y, rgb565(120, 120, 150));
  }
  float pulse = 0.7f + 0.3f * sinf(entryT * 4);
  drawVectorText("NEW HIGH SCORE", SCREEN_W / 2, 70, 6.0f, dimmed(255, 210, 80, pulse), true);
  char b[64];
  int ps = (int)playT;
  snprintf(b, sizeof(b), "%07ld     %d:%02d", score, ps / 60, ps % 60);
  drawVectorText(b, SCREEN_W / 2, 140, 5.0f, rgb565(240, 240, 245), true);
  drawVectorText("ENTER YOUR INITIALS", SCREEN_W / 2, 196, 3.0f, rgb565(180, 184, 196), false);
  for (int k = 0; k < 3; k++) {
    int cx = SCREEN_W / 2 - 110 + k * 110, cy = 290;
    bool on = (k == entryPos);
    fillRect(cx - 42, cy - 54, cx + 42, cy + 54, on ? rgb565(30, 34, 50) : rgb565(16, 18, 28));
    drawRectOutline(cx - 42, cy - 54, cx + 42, cy + 54, on ? rgb565(255, 200, 70) : rgb565(80, 86, 100), on ? 3 : 1);
    char l[2] = { entryName[k], 0 };
    drawVectorText(l, cx, cy, 10.0f, on ? rgb565(255, 230, 120) : rgb565(220, 220, 228), true);
    if (on) {
      int ax[3] = { cx - 14, cx + 14, cx }, ay[3] = { cy - 64, cy - 64, cy - 80 };
      fillPolygon(ax, ay, 3, rgb565(255, 200, 70));
      int bx[3] = { cx - 14, cx + 14, cx }, by[3] = { cy + 64, cy + 64, cy + 80 };
      fillPolygon(bx, by, 3, rgb565(255, 200, 70));
    }
  }
  drawVectorText("UP / DOWN: LETTER     X: NEXT     LEFT: BACK", SCREEN_W / 2, 400, 2.4f, rgb565(150, 156, 170), false);
  drawVectorText("RANK  INITIALS    SCORE    TIME PLAYED", SCREEN_W / 2, 452, 2.2f, rgb565(255, 190, 70), false);
  for (int r = 0; r < HI_COUNT; r++) {
    HiEntry e;
    if (r == entrySlot) {
      memcpy(e.name, entryName, 4);
      e.score = (int32_t)score;
      e.secs = (int32_t)playT;
    } else {
      e = hiTable[r > entrySlot ? r - 1 : r];
    }
    hiLine(b, sizeof(b), e, r + 1);
    drawVectorText(b, SCREEN_W / 2, 494 + r * 40, 3.0f, r == entrySlot ? rgb565(255, 220, 120) : rgb565(170, 174, 186), r == entrySlot);
  }
}

// ============================================================ the settings (Triangle), STAGE 1's briefing
#define SETTINGS_FILE QL_DATA_DIR "/settings.bin"
#define SETTINGS_ITEMS 6
static void loadSettings() {
  FILE *f = fopen(SETTINGS_FILE, "rb");
  if (!f) return;
  uint8_t b[2];
  if (fread(b, 1, 2, f) == 2) {
    musicOn[1].store(b[0] != 0);
    musicOn[0].store(b[1] != 0);
  }
  fclose(f);
}
static void saveSettings() {
  mkdir(QL_DATA_DIR, 0777);
  FILE *f = fopen(SETTINGS_FILE, "wb");
  if (!f) return;
  uint8_t b[2] = { (uint8_t)musicOn[1].load(), (uint8_t)musicOn[0].load() };
  fwrite(b, 1, 2, f);
  fclose(f);
}

static void quitToTitle();
static void unlockSubBasement();

// JUMP TO STAGE 2 (for testing): in the sub basement lift area (unlocked), at the end of stage 1
static void jumpToStage2() {
  if (!subUnlocked) unlockSubBasement();
  stage = 1;
  scene = SC_NONE;
  loopPowered = false;
  loopPowerT = 9;
  loopDoorT = 0;
  loopDoorDir = 0;
  settingsOn = quitConfirm = mapOn = termOn = briefingOn = bayBusy = false;
  introT = -1;
  powerPanelT = 0;
  enterShaft(subShaft, SUB_FLOOR, true);
  placeAgent((SHAFT_X0 + SHAFT_X1) / 2.0f, CAR_FLOOR_Y, 1);
  showMsg("JUMPED TO THE END OF STAGE 1", 2.5f);
}

static void updateSettings() {
  if (pressed & BTN_UP) { settingsSel = (settingsSel + SETTINGS_ITEMS - 1) % SETTINGS_ITEMS; quitConfirm = false; sfxPlay(SFX_KEY); }
  if (pressed & BTN_DOWN) { settingsSel = (settingsSel + 1) % SETTINGS_ITEMS; quitConfirm = false; sfxPlay(SFX_KEY); }
  bool go = (pressed & (BTN_JUMP | BTN_USE)) != 0;
  bool side = (pressed & (BTN_LEFT | BTN_RIGHT)) != 0;
  if (settingsSel <= 1 && (go || side)) {          // the music: on / off
    int k = settingsSel == 0 ? 1 : 0;
    musicOn[k].store(!musicOn[k].load());
    saveSettings();
    sfxPlay(SFX_CLICK);
  } else if (settingsSel == 2 && go) {             // JUMP TO STAGE 2 (for testing): the sub basement's lift
    jumpToStage2();
    sfxPlay(SFX_CLICK);
  } else if (settingsSel == 3 && (go || side)) {   // ALL ACCESS (for testing stage 2): every card reader opens
    s2AllAccess = !s2AllAccess;
    sfxPlay(SFX_CLICK);
  } else if (settingsSel == 4 && go) {             // QUIT TO TITLE: X twice
    if (quitConfirm) quitToTitle();
    else quitConfirm = true;
    sfxPlay(SFX_CLICK);
  } else if (settingsSel == 5 && go) {             // RESUME
    settingsOn = false;
    sfxPlay(SFX_CLICK);
  }
}

static void drawSettings() {
  gfxClipAll();
  blendRect(0, HUD_H, SCREEN_W - 1, SCREEN_H - 1, 2, 4, 8, 0.72f);
  int x0 = 330, y0 = 100, x1 = 950, y1 = 640;
  fillRect(x0, y0, x1, y1, rgb565(12, 16, 26));
  drawRectOutline(x0, y0, x1, y1, rgb565(90, 220, 255), 3);
  drawVectorText("SETTINGS", SCREEN_W / 2, y0 + 40, 4.4f, rgb565(90, 220, 255), true);
  drawVectorText("GAME PAUSED", SCREEN_W / 2, y0 + 82, 2.2f, rgb565(255, 190, 70), false);
  for (int i = 0; i < SETTINGS_ITEMS; i++) {
    int y = y0 + 140 + i * 58;
    int item = i;
    bool sel = i == settingsSel;
    if (sel) fillRect(x0 + 24, y - 22, x1 - 24, y + 22, rgb565(36, 60, 90));
    uint16_t c = sel ? rgb565(255, 230, 120) : rgb565(220, 226, 236);
    const char *label = i == 0 ? "STAGE MUSIC" : i == 1 ? "ELEVATOR MUSIC" : i == 2 ? "JUMP TO STAGE 2" : i == 3 ? "ALL ACCESS (TEST)" :
                        i == 4 ? (quitConfirm ? "X AGAIN TO QUIT" : "QUIT TO TITLE") : "RESUME";
    if (item <= 1 || item == 3) {
      bool on = item == 3 ? s2AllAccess : musicOn[i == 0 ? 1 : 0].load();
      drawVectorText(label, x0 + 50 + vectorTextWidth(label, 3.0f) / 2, y, 3.0f, c, true);
      const char *v = on ? "ON" : "OFF";
      drawVectorText(v, x1 - 60 - vectorTextWidth(v, 3.0f) / 2, y, 3.0f, on ? rgb565(100, 240, 140) : rgb565(255, 90, 70), true);
    } else {
      drawVectorText(label, SCREEN_W / 2, y, 3.0f, i == 4 && quitConfirm ? rgb565(255, 90, 70) : c, true);
    }
  }
  drawVectorText("UP / DOWN: CHOOSE    X: CHANGE    TRIANGLE: BACK", SCREEN_W / 2, y1 - 26, 1.8f, rgb565(150, 158, 176), false);
}

// STAGE 1: what it is about, with the generators to find
static void drawBriefing() {
  gfxClipAll();
  float k = fminf(1.0f, briefingT / 0.4f);
  blendRect(0, HUD_H, SCREEN_W - 1, SCREEN_H - 1, 2, 4, 8, 0.75f * k);
  if (briefingT < 0.2f) return;
  int x0 = 250, y0 = 80, x1 = 1030, y1 = 660;
  fillRect(x0, y0, x1, y1, rgb565(10, 12, 20));
  drawRectOutline(x0, y0, x1, y1, rgb565(255, 190, 70), 3);
  fillRect(x0 + 3, y0 + 3, x1 - 3, y0 + 64, rgb565(60, 36, 10));
  drawVectorTextW("STAGE 1", SCREEN_W / 2, y0 + 34, 5.0f, rgb565(255, 210, 90), 2.6f);
  drawVectorText("ABANDONED FACILITY", SCREEN_W / 2, y0 + 106, 4.0f, rgb565(230, 236, 245), true);
  char b[64];
  snprintf(b, sizeof(b), "COUNTDOWN TO POWER FAILURE %d:%02d", GAME_SECONDS / 60, GAME_SECONDS % 60);
  drawVectorText(b, SCREEN_W / 2, y0 + 156, 3.0f, rgb565(255, 90, 70), true);
  snprintf(b, sizeof(b), "RESTORE %d GENERATORS", GENERATORS);
  drawVectorText(b, SCREEN_W / 2, y0 + 206, 3.4f, rgb565(100, 240, 140), true);
  // the generators: off, as he will find them
  for (int g = 0; g < GENERATORS; g++) {
    int cx = SCREEN_W / 2 - 200 + g * 200, fy = y0 + 400;
    fillRect(cx - 80, fy - 150, cx + 80, fy + 14, rgb565(30, 36, 48));
    drawRectOutline(cx - 80, fy - 150, cx + 80, fy + 14, rgb565(90, 100, 120), 2);
    fillRect(cx - 78, fy, cx + 78, fy + 12, rgb565(60, 64, 72));
    paintGenerator(cx, fy);
    drawGeneratorLive(cx, fy, 0.0f, false);
    snprintf(b, sizeof(b), "%d", g + 1);
    drawVectorText(b, cx - 62, fy - 134, 2.2f, rgb565(255, 190, 70), true);
  }
  drawVectorText("SEARCH EACH ONE TO THROW ITS LEVER", SCREEN_W / 2, y0 + 450, 2.4f, rgb565(170, 178, 196), false);
  if (briefingT > 0.8f && fmodf(clockT, 1.0f) < 0.65f) drawVectorText("PRESS X TO START", SCREEN_W / 2, y1 - 40, 3.4f, rgb565(240, 240, 245), true);
}

// ============================================================ the game
// ============================================================ the title
static bool onTitle = true;


static float titleT = 0;                 // seconds on the title screen
#define TITLE_STORY 15.0f                // the title's panel: the mission objectives this long...
#define TITLE_CYCLE 30.0f                // ...then the high scores as long, round and round

// The top 5, in the title's panel (it takes turns with the story)
#define TPANEL_Y0 214                    // the title's panel (objectives / high scores)
static void drawTitleScores(float t) {
  const int y0 = TPANEL_Y0;
  float f = fminf(1.0f, t / 0.5f);
  drawVectorText("TOP 5 AGENTS", SCREEN_W / 2, y0 + 16, 3.0f, rgb565(255, 190, 70), true);
  drawVectorText("RANK  INITIALS    SCORE    TIME PLAYED", SCREEN_W / 2, y0 + 62, 2.2f, dimmed(150, 200, 220, f), false);
  for (int r = 0; r < HI_COUNT; r++) {
    float k = clampv((t - 0.3f - r * 0.18f) / 0.3f, 0.0f, 1.0f);
    if (k <= 0) continue;
    char b[64];
    hiLine(b, sizeof(b), hiTable[r], r + 1);
    drawVectorText(b, SCREEN_W / 2, y0 + 108 + r * 44, 3.2f, r == 0 ? dimmed(255, 220, 120, k) : dimmed(236, 236, 242, k), true);
  }
}

// MISSION OBJECTIVES, in the title's panel (it takes turns with the high scores)
static void drawTitleObjectives(float t) {
  const int x0 = 196, y0 = TPANEL_Y0;
  bool blinkHead = fmodf(t, 1.2f) < 0.8f;
  drawVectorText("MISSION OBJECTIVES", SCREEN_W / 2, y0 + 16, 3.0f, blinkHead ? rgb565(255, 110, 80) : rgb565(170, 60, 44), true);
  static const char *const obj[3] = { "POWER ON THE GENERATORS", "FIND ALL QUANTUM DRIVES", "UNLOCK SUB BASEMENT ACCESS" };
  for (int i = 0; i < 3; i++) {
    float k = clampv((t - 0.3f - i * 0.5f) / 0.4f, 0.0f, 1.0f);
    if (k <= 0) continue;
    float y = y0 + 92 + i * 90;
    float slide = (1.0f - k) * 60;
    char b[48];
    snprintf(b, sizeof(b), "%d.", i + 1);
    drawVectorText(b, x0 + 70 - slide, y, 4.0f, dimmed(255, 190, 70, k), true);
    // its picture
    int ix = (int)(x0 + 160 - slide), iy = (int)y;
    if (i == 0) {                        // a generator, its lamp green
      fillRect(ix - 30, iy - 18, ix + 22, iy + 16, rgb565(214, 168, 40));
      fillRect(ix - 30, iy - 18, ix + 22, iy - 15, rgb565(246, 206, 90));
      fillRect(ix - 24, iy - 12, ix - 4, iy + 10, rgb565(34, 36, 40));
      fillRect(ix + 24, iy - 22, ix + 36, iy + 16, rgb565(70, 76, 88));
      fillCircle(ix + 30, iy - 16, 4, rgb565(80, 255, 120));
      fillRect(ix - 34, iy + 16, ix + 38, iy + 22, rgb565(34, 36, 40));
    } else if (i == 1) {                 // a quantum drive
      drawQuantumDrive((float)ix, (float)iy, 0.5f, fmodf(t, 0.8f) < 0.55f);
    } else {                             // the lift going down
      fillRect(ix - 26, iy - 26, ix + 26, iy + 26, rgb565(60, 66, 80));
      drawRectOutline(ix - 26, iy - 26, ix + 26, iy + 26, rgb565(150, 156, 168), 2);
      int ax[7] = { ix, ix + 16, ix + 7, ix + 7, ix - 7, ix - 7, ix - 16 };
      int ay[7] = { iy + 18, iy + 2, iy + 2, iy - 16, iy - 16, iy + 2, iy + 2 };
      fillPolygon(ax, ay, 7, rgb565(255, 220, 80));
    }
    drawVectorText(obj[i], x0 + 230 - slide + vectorTextWidth(obj[i], 3.4f) / 2, y, 3.4f, dimmed(236, 236, 242, k), true);
  }
}

// The title's effects: quantum motes drifting up, the quantum core turning
// behind the name, a camera's beam sweeping, sparks crackling along the floor
static void drawTitleEffects() {
  float t = clockT;
  // a security camera's beam, sweeping the dark
  {
    float cx = 1240, cy = 0, a = (-30.0f + 22.0f * sinf(t * 0.6f)) * 0.017453293f;
    float xs[3] = { cx, cx + sinf(a - 0.09f) * 1500, cx + sinf(a + 0.09f) * 1500 };
    float ys[3] = { cy, cy + cosf(a - 0.09f) * 1500, cy + cosf(a + 0.09f) * 1500 };
    blendPolygonF(xs, ys, 3, 255, 240, 170, 0.06f);
  }
  // the quantum core: a glowing heart, three orbits turning round it, a particle on each
  const float qx = SCREEN_W / 2, qy = 108;
  float pulse = 0.5f + 0.5f * sinf(t * 2.3f);
  glowDisc(qx, qy, 230, 40, 120, 255, 0.10f + 0.06f * pulse);
  glowDisc(qx, qy, 70, 120, 200, 255, 0.25f + 0.2f * pulse);
  for (int o = 0; o < 3; o++) {
    float rot = t * (0.25f + o * 0.08f) + o * 1.047f;
    float ra = 300, rb = 54;
    float cr = cosf(rot), sr = sinf(rot);
    float px = 0, py = 0;
    for (int k = 0; k <= 48; k++) {
      float u = k * TWO_PI / 48;
      float ex = cosf(u) * ra, ey = sinf(u) * rb;
      float x = qx + ex * cr - ey * sr, y = qy + ex * sr * 0.35f + ey * cr;
      if (k) drawLineF(px, py, x, y, o == 1 ? rgb565(70, 40, 110) : rgb565(30, 70, 110), 1.2f);
      px = x;
      py = y;
    }
    float u = t * (1.6f + o * 0.5f) + o * 2.0f;
    float ex = cosf(u) * ra, ey = sinf(u) * rb;
    float x = qx + ex * cr - ey * sr, y = qy + ex * sr * 0.35f + ey * cr;
    glowDisc(x, y, 22, 120, 220, 255, 0.7f);
    fillCircle((int)x, (int)y, 3, rgb565(220, 245, 255));
  }
  // quantum motes drifting up
  for (int i = 0; i < 46; i++) {
    uint32_t h = (uint32_t)(i + 1) * 2654435761u;
    float sp = 18 + (h >> 27) * 3.0f;
    float x = (float)((h >> 8) % SCREEN_W) + 14 * sinf(t * 0.7f + i);
    float y = SCREEN_H + 20 - fmodf(t * sp + (h >> 12) % SCREEN_H, (float)SCREEN_H + 40);
    bool violet = (h >> 5) & 1;
    float tw = 0.5f + 0.5f * sinf(t * 3.0f + i * 1.3f);
    glowDisc(x, y, 9, violet ? 180 : 80, violet ? 90 : 200, 255, 0.35f + 0.35f * tw);
  }
  // sparks crackling along the floor now and then
  if (fmodf(t, 3.2f) < 0.35f)
    for (int k = 0; k < 4; k++) {
      int x = fxRnd(60, SCREEN_W - 120);
      drawArc(x, 708, x + fxRnd(40, 120), 708 - fxRnd(0, 24), 8, fxRnd(0, 2) ? rgb565(230, 245, 255) : rgb565(120, 200, 255), false);
    }
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
  drawTitleEffects();
  // the name, in big vector letters with a glow behind (now and then reality glitches: it splits red and cyan)
  const char *w1 = "QUANTUM", *w2 = "LOCKDOWN";
  float pulse = 0.5f + 0.5f * sinf(clockT * 2.0f);
  uint16_t glow = rgb565(60 + (int)(40 * pulse), 34, 12), main = rgb565(255, 190, 70);
  const float ty1 = 62, ty2 = 156, ts = 10.0f;
  bool glitch = fmodf(clockT, 4.3f) < 0.16f;
  drawVectorTextW(w1, SCREEN_W / 2, ty1, ts, glow, 9.0f);
  drawVectorTextW(w2, SCREEN_W / 2, ty2, ts, glow, 9.0f);
  if (glitch) {
    float j = 9.0f * sinf(clockT * 97.0f);
    drawVectorTextW(w1, SCREEN_W / 2 - j, ty1, ts, rgb565(255, 40, 60), 3.0f);
    drawVectorTextW(w2, SCREEN_W / 2 + j, ty2, ts, rgb565(40, 220, 255), 3.0f);
    for (int k = 0; k < 3; k++) {          // tears across it
      int y = fxRnd(20, 210);
      blendRect(0, y, SCREEN_W - 1, y + fxRnd(2, 8), 120, 220, 255, 0.25f);
    }
  }
  drawVectorTextW(w1, SCREEN_W / 2, ty1, ts, main, 3.4f);
  drawVectorTextW(w2, SCREEN_W / 2, ty2, ts, main, 3.4f);
  // the panel: the mission objectives, then the high scores, 15 seconds each
  float cyc = fmodf(titleT, TITLE_CYCLE);
  if (cyc < TITLE_STORY) drawTitleObjectives(cyc);
  else drawTitleScores(cyc - TITLE_STORY);
  if (fmodf(clockT, 1.0f) < 0.65f) drawVectorText("PRESS X TO START", SCREEN_W / 2, 594, 4.0f, rgb565(240, 240, 245), true);
  if (tuneProblem[0][0]) drawVectorText(tuneProblem[0], SCREEN_W / 2, 636, 1.8f, rgb565(220, 120, 90), false);
  if (tuneProblem[1][0]) drawVectorText(tuneProblem[1], SCREEN_W / 2, 654, 1.8f, rgb565(220, 120, 90), false);
  if (tuneProblem[2][0]) drawVectorText(tuneProblem[2], SCREEN_W / 2, 672, 1.8f, rgb565(220, 120, 90), false);
  if (tuneProblem[3][0]) drawVectorText(tuneProblem[3], SCREEN_W / 2, 690, 1.8f, rgb565(220, 120, 90), false);
  char b[24];
  snprintf(b, sizeof(b), "VERSION %s", GAME_VERSION);
  drawVectorText(b, SCREEN_W / 2, 710, 1.8f, rgb565(100, 110, 130), false);
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
  docsFound = 0;
  docShowT = 0;
  health = HEALTH_MAX;
  hurtFlashT = 0;
  timeLeft = GAME_SECONDS;
  playT = drainT = 0;
  warnSlot = 99;
  warnT = 9;
  nShocks = nDroneHits = nPitFalls = nCaught = nKitsUsed = 0;
  bonusTotal = 0;
  gameOver = END_NONE;
  itemShowT = nothingShowT = 0;
  drivesCarried = drivesLoaded = 0;
  bayBusy = termOn = false;
  settingsOn = briefingOn = quitConfirm = false;
  stage = 1;
  scene = SC_NONE;
  loopPowered = false;
  loopPowerT = 9;
  loopDoorT = 0;
  loopDoorDir = 0;
  powerPanelT = 0;
  leverT = -1;
  s2Act = 0;
  bodyT = -1;
  s2Card = 0;
  s2Disguised = bodyCarried = false;
  for (int i = 0; i < MAX_POPS; i++) popT[i] = 0;
  msgT = 0;
}

static void goToTitle() {
  onTitle = true;
  titleT = 0;
  tuneWanted.store(3, std::memory_order_relaxed);   // (the title's tune, from the top)
  titleFromStart.store(true);
}

// QUIT TO TITLE: the game is dropped, as it is (no score, no table)
static void quitToTitle() {
  settingsOn = quitConfirm = false;
  scene = SC_NONE;
  rideHum.store(0, std::memory_order_relaxed);
  mapOn = termOn = briefingOn = bayBusy = false;
  gameOver = END_NONE;
  introT = -1;
  liftHum.store(0, std::memory_order_relaxed);
  droneHum.store(0, std::memory_order_relaxed);
  goToTitle();
}

void gameInit(uint32_t seed) {
  gfxInit();
  loadHiScores();
  loadSettings();
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
    tuneWanted.store(3, std::memory_order_relaxed);
    if (pressed & BTN_JUMP) {
      newGame();
      pressed = 0;                     // (that X starts the game; it isn't a somersault)
    } else {
      drawTitle();
      return;
    }
  }

  if (entryOn) {                        // initials for the high score table
    tuneWanted.store(3, std::memory_order_relaxed);
    liftHum.store(0, std::memory_order_relaxed);
    droneHum.store(0, std::memory_order_relaxed);
    updateEntry(dt);
    if (entryOn) drawEntry();
    else drawTitle();
    return;
  }

  if (gameOver != END_NONE) {           // the game is over: X (after a moment) to the high scores, or the title
    gameOverT += dt;
    if (gameOverShownT() > 2.2f && (pressed & BTN_JUMP)) {
      afterGame();
      liftHum.store(0, std::memory_order_relaxed);
      droneHum.store(0, std::memory_order_relaxed);
      if (entryOn) drawEntry();
      else drawTitle();
      return;
    }
    held = pressed = 0;
    mapOn = false;
  }

  // the settings (Triangle, not on the map): everything waits, the clock too
  if ((pressed & BTN_NEW) && !mapOn && gameOver == END_NONE) {
    settingsOn = !settingsOn;
    settingsSel = 0;
    quitConfirm = false;
    pressed &= ~BTN_NEW;
  }
  if (settingsOn) {
    updateSettings();
    if (onTitle) {                       // (QUIT TO TITLE)
      drawTitle();
      return;
    }
    pressed = 0;
  }

  if (introT >= 0 && !settingsOn) {      // the opening: no map, no clock, no control
    updateIntro(dt);
    pressed = 0;
  }

  if (briefingOn && !settingsOn) {       // STAGE 1's briefing: X and off he goes
    briefingT += dt;
    if ((pressed & BTN_JUMP) && briefingT > 0.8f) {
      briefingOn = false;
      showMsg("OPTIONS: THE MAP     TRIANGLE: SETTINGS", 5.0f);
    }
    pressed = 0;
  }

  // the QUANTUMLOOP's lights and doors
  if (!settingsOn && !mapOn) {
    loopPowerT += dt;
    if (loopDoorDir != 0) {
      loopDoorT += loopDoorDir * dt / 1.2f;
      if (loopDoorT >= 1.0f) {
        loopDoorT = 1.0f;
        loopDoorDir = 0;
        if (curRoom == SUB_ROOM && scene == SC_NONE) startRide();   // (open: he goes on board)
      } else if (loopDoorT <= 0.0f) {
        loopDoorT = 0.0f;
        loopDoorDir = 0;
      }
    }
  }
  // between the stages: the scenes run, the clock waits
  if (scene != SC_NONE && !settingsOn && gameOver == END_NONE) {
    updateScene(dt);
    pressed = 0;
  }

  if ((pressed & BTN_MAP) && !termOn && scene == SC_NONE) mapOn = !mapOn;   // (the map pauses everything, the clock too)
  if (mapOn || settingsOn || briefingOn || scene != SC_NONE) {
    pressed = 0;
  } else {
    if (gameOver == END_NONE && introT < 0) {
      playT += dt;
      drainT += dt;
      if (drainT >= HEALTH_DRAIN_SECONDS) {   // every minute of play: 1 health
        drainT -= HEALTH_DRAIN_SECONDS;
        hurt(1);
      }
      timeLeft -= dt;
      updateNukes(dt);
      updateFlyers(dt);
      if (timeLeft > 60) warnSlot = 99;  // (the last minute: an alert and the lights flickering every ten seconds)
      else if (timeLeft > 0) {
        int slot = (int)ceilf(timeLeft / WARN_EVERY);
        if (slot != warnSlot) {
          warnSlot = slot;
          warnT = 0;
          sfxPlay(SFX_WARN);
        }
      }
      warnT += dt;
      if (timeLeft <= 0) {
        timeLeft = 0;
        endGame(END_TIME);
      }
    }
    if (hurtFlashT > 0) hurtFlashT -= dt;
    if (msgT > 0) msgT -= dt;
    if (nothingShowT > 0) nothingShowT -= dt;
    if (itemShowT > 0) itemShowT -= dt;
    if (docShowT > 0) docShowT -= dt;
    if (cardShowT > 0) cardShowT -= dt;
    if (shotFlashT > 0) shotFlashT -= dt;
    if (gunFlashT > 0) gunFlashT -= dt;
    for (int i = 0; i < MAX_POPS; i++)
      if (popT[i] > 0) popT[i] -= dt;
    if (leverT >= 0) leverT += dt;
    if (powerPanelT > 0) {
      powerPanelT -= dt;
      if ((pressed & (BTN_JUMP | BTN_USE)) && powerPanelT < 4.6f) {   // (X or SQUARE puts it away)
        powerPanelT = 0;
        pressed = 0;
      }
    }
    updateLifts(dt);
    if (termOn) {                        // at the main computer (or the facility terminal): the controller works the menu
      if (gameOver == END_NONE) {
        if (termKind == 1) updateFacilityTerminal(dt);
        else updateTerminal(dt);
      }
    } else {
      if (introT < 0) {
        if (s2Act) updateS2Act(dt);      // (stage 2: knocking out, searching, binning the maintenance man)
        else updateAgent(dt);
      }
      if (introT < 0) updateHazards(dt);
      if (stage == 2 && gameOver == END_NONE && scene == SC_NONE) updateStage2(dt);
    }
  }

  // the picture
  gfxTarget = g_frame;
  if (scene == SC_RIDE) {
    drawRide(sceneT);
  } else if (mode == MODE_ROOM) {
    memcpy(g_frame, roomBg, sizeof(roomBg));
    gfxClip(HUD_H, PLAY_Y1);
    drawOrnamentsAnim();
    for (int i = 0; i < liftCount; i++) drawLiftPad(lifts[i]);
    drawDecorAnim();
    if (curRoom == CONTROL_ROOM) drawControlRoomAnim();
    if (stage == 2 && curRoom < S2_ROOMS) {
      drawLabRoomBack();
      drawNpcs();
      drawLabRoomFront();
    }
    if (curRoom == SUB_ROOM || curRoom == ARRIVAL_ROOM) drawLoopStation(curRoom == ARRIVAL_ROOM);
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
  if (scene != SC_RIDE) {
    if (stage == 2 && s2Disguised) drawPersonU(shownPose, agX, agY, agFacing, myScale, PK_MAINT);   // in the maintenance man's overalls
    else drawAgentU(shownPose, agX, agY, agFacing, myScale);
  }
  if (s2Act == 5) {                       // his pistol, and its flash
    int hx, hy, mx, my;
    agentHand(shownPose, agX, agY, agFacing, myScale, hx, hy, mx, my);
    fillRect(hx - 4, hy - 4, hx + 4, hy + 8, rgb565(30, 30, 34));
    fillRect(agFacing > 0 ? hx : hx - 18, hy - 6, agFacing > 0 ? hx + 18 : hx, hy, rgb565(30, 30, 34));
    if (gunFlashT > 0) {
      int fx = hx + agFacing * 26;
      glowDisc((float)fx, (float)(hy - 3), 22, 255, 220, 120, 0.9f);
      fillCircle(fx, hy - 3, 5, rgb565(255, 250, 220));
    }
  }
  if (mode == MODE_SHAFT && stage == 2) {
    drawCarriedBody();
    drawS2Prompts();
  }
  if (mode == MODE_ROOM && stage == 2 && !s2Act && agState == ST_IDLE && gameOver == END_NONE && atNpcToKill() >= 0) {
    const Npc &n = npcs[atNpcToKill()];
    drawVectorText("X: KNIFE", n.x, n.y - 190, 2.4f, rgb565(255, 120, 90), true);
  }
  gfxClip(HUD_H, PLAY_Y1);
  if (mode == MODE_ROOM && lightsOut) {   // dark: the goggles' view, then what glows, the search bars over it
    nightVision();
    drawDarkGlows();
    drawSearchMarks();
  }
  if (mode == MODE_ROOM && agState == ST_SHOCK) drawShockSparks();
  if (mode == MODE_ROOM && camCount > 0) drawCameras();
  if (mode == MODE_ROOM && curHazard == HZ_DRONE) drawDrone();
  if (mode == MODE_ROOM && powerPanelT > 0 && !mapOn) drawPowerPanel();
  if (termOn && mode == MODE_ROOM) drawTerminal();
  if (mode == MODE_SHAFT && scene != SC_RIDE) drawPanel();
  if (gameOver == END_NONE && scene == SC_NONE && warnT < 0.75f && flickerDim(warnT)) {   // the last minute's alert: the lights flicker
    gfxClip(HUD_H, PLAY_Y1);
    dimRoom();
  }
  if (gameOver == END_TIME) {            // the power has failed: the lights flicker and go, night vision comes on
    gfxClip(HUD_H, PLAY_Y1);
    if (gameOverT < POWER_OUT_SECONDS) {
      if (((int)(gameOverT * 14)) & 1) dimRoom();
    } else if (!(mode == MODE_ROOM && lightsOut)) {
      nightVision();
      drawGoggleReadout();
    }
  }
  if (mode == MODE_ROOM && !termOn) drawScorePops();
  if (itemShowT > 0 && mode == MODE_ROOM && !mapOn && gameOver == END_NONE) drawItemPanel();
  if (docShowT > 0 && mode == MODE_ROOM && !mapOn && gameOver == END_NONE) drawDocPanel();
  if (stage == 2 && mode == MODE_ROOM && npcCount > 0 && !mapOn && gameOver == END_NONE) drawSuspicionMeter();
  if (!mapOn) drawFlyers();
  if (cardShowT > 0 && !mapOn && gameOver == END_NONE) drawCardPanel();
  if (mapOn) drawMap();
  else if (itemShowT <= 0 && !briefingOn && !settingsOn && scene != SC_STAGE_CLEAR && scene != SC_STAGE2_BRIEF && scene != SC_RIDE) drawMessage();
  if (briefingOn && !mapOn) drawBriefing();
  if (settingsOn) drawSettings();
  {                                      // fading to black and back (on board, off again)
    float a = 0;
    if (scene == SC_FADE_OUT) a = sceneT;
    else if (scene == SC_RIDE) a = sceneT < 1.0f ? 1.0f - sceneT : (sceneT > RIDE_SECONDS - 1.0f ? sceneT - (RIDE_SECONDS - 1.0f) : 0.0f);
    else if (scene == SC_ARRIVE && sceneT < 1.0f) a = 1.0f - sceneT;
    if (a > 0) {
      gfxClipAll();
      blendRect(0, HUD_H, SCREEN_W - 1, SCREEN_H - 1, 0, 0, 0, clampv(a, 0.0f, 1.0f));
    }
  }
  if (scene == SC_STAGE_CLEAR) drawGameOver();
  if (scene == SC_STAGE2_BRIEF) drawStage2Brief();
  if (gameOver != END_NONE) drawGameOver();
  // the music: the lift's tune in the car, stage 1's everywhere else; the lift's motor
  int tune = mode == MODE_SHAFT && inCar(agX) && agState != ST_JUMP ? 1 : (stage == 2 ? 4 : 2);   // (the lift car's, else the stage's)
  if (tune == 1 && !musicOn[0].load(std::memory_order_relaxed)) tune = stage == 2 ? 4 : 2;   // (elevator music off: the stage's carries on in the car)
  if (gameOver != END_NONE || scene == SC_STAGE_CLEAR) tune = 0;
  else if (scene == SC_RIDE || scene == SC_STAGE2_BRIEF || scene == SC_FADE_OUT || scene == SC_ARRIVE) tune = 4;
  tuneWanted.store(tune, std::memory_order_relaxed);
  rideHum.store(scene == SC_RIDE && !settingsOn ? 1 : 0, std::memory_order_relaxed);
  int hum = 0;
  if (!mapOn && gameOver == END_NONE) {
    if (mode == MODE_SHAFT && carMoving) hum = carFloor * LEVEL_GAP < carWorldY ? 1 : 2;
    for (int i = 0; i < liftCount && mode == MODE_ROOM; i++)
      if (lifts[i].moving) hum = (lifts[i].stops[lifts[i].at] < lifts[i].y ? 1 : 2) + 2;   // (3, 4: a pad - quieter)
  }
  droneHum.store(mode == MODE_ROOM && curHazard == HZ_DRONE && !mapOn && !settingsOn && gameOver == END_NONE ? 1 : 0, std::memory_order_relaxed);
  int was = liftHum.exchange(hum, std::memory_order_relaxed);
  if (was != 0 && hum == 0 && !mapOn) sfxPlay(SFX_LIFT_STOP);
  drawHud();
}

// For the PC test build: where he is and what he is doing
const char *gameDebug() {
  static char b[200];
  static const char *st[] = { "IDLE", "RUN", "JUMP", "USE", "FALL", "LAND", "LIFT", "KO", "SHOCK", "CAUGHT" };
  if (mode == MODE_SHAFT)
    snprintf(b, sizeof(b), "shaft %c floor %d%s x=%.0f y=%.0f %s%s", shaftLetter(curShaft), carFloor + 1, carMoving ? " (moving)" : "", agX, agY, st[agState],
             subUnlocked ? (subShaft == curShaft ? " [sub basement under this lift]" : " [sub basement elsewhere]") : "");
  else if (curRoom >= ROOM_COUNT)
    snprintf(b, sizeof(b), "%s x=%.0f y=%.0f %s drives %d carried %d loaded, generators %d, terminal %s page %d, sub %c",
             curRoom == CONTROL_ROOM ? "QUANTUM CONTROL" : (curRoom == SUB_ROOM ? "QUANTUMLOOP STATION" : "ARRIVAL STATION"), agX, agY, st[agState], drivesCarried, drivesLoaded,
             generatorsOn(), termOn ? "on" : "off", termPage, subUnlocked ? shaftLetter(subShaft) : '-');
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
  briefingOn = settingsOn = false;
  if (onTitle) layOut(layoutSeed);
  onTitle = false;
  rooms[0].tpl = (uint8_t)(tpl % WIDE_ROOM_COUNT);
  rooms[0].topL = topL;
  rooms[0].topR = topR;
  rooms[0].hazard = (uint8_t)hazard;
  enterRoom(0, true);
}

// For the PC test build: drives in hand and generators on, then into QUANTUM CONTROL (by its left door)
void gameDebugControl(int drives, int gens) {
  briefingOn = settingsOn = false;
  if (onTitle) layOut(layoutSeed);
  onTitle = false;
  introT = -1;
  drivesCarried = drives;
  drivesLoaded = 0;
  itemsFound = drives;
  for (int i = 0; i < ROOM_COUNT; i++)
    if (rooms[i].gen) rooms[i].genOn = gens-- > 0;
  enterRoom(CONTROL_ROOM, true);
}
// For the PC test build: into a lift shaft (-1: the sub basement's) at a floor (-1: its lowest stop above the sub basement)
void gameDebugShaft(int s, int f) {
  briefingOn = settingsOn = false;
  if (s < 0) s = subShaft < 0 ? 0 : subShaft;
  if (f < 0) {
    f = 0;
    for (int k = 0; k < FLOORS; k++)
      if (isStop(s, k)) f = k;
  }
  enterShaft(s, f, true);
}
// For the PC test build: stage 2's lab, at lift s level f (S2_ARRIVAL: the QUANTUMLOOP's floor); wk >= 0: the maintenance man's level
void gameDebugStage2(int s, int f, int wk) {
  briefingOn = settingsOn = false;
  if (onTitle) layOut(layoutSeed);
  onTitle = false;
  introT = -1;
  stage = 2;
  scene = SC_NONE;
  layOutStage2();
  timeLeft = GAME_SECONDS;
  if (wk >= 0) wkFloor = wk;
  enterShaft(s, f, true);
  placeAgent((SHAFT_X0 + SHAFT_X1) / 2.0f, CAR_FLOOR_Y, 1);
}
// For the PC test build: a generator's room (n: 0, 1, 2), in by its left door
void gameDebugGenerator(int n) {
  briefingOn = settingsOn = false;
  if (onTitle) layOut(layoutSeed);
  onTitle = false;
  introT = -1;
  for (int i = 0; i < ROOM_COUNT; i++)
    if (rooms[i].gen && n-- == 0) {
      enterRoom(i, true);
      return;
    }
}
