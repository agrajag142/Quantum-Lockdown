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
#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================ settings
#define GAME_VERSION      "062"
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
enum Hazard : uint8_t { HZ_NONE, HZ_ELECTRIC, HZ_DARK, HZ_CAMERA, HZ_CAMERA2, HZ_DRONE, HZ_CAMDRONE };   // (HZ_CAMDRONE: stage 2's server room, a camera and a drone)
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
static float hackSecs[NUKE_SITES];          // how long each hack takes, picked each game: the first 7-10 minutes, then 2-4 each
static const float LAUNCH_MINUTES[NUKE_SITES] = { 5, 4, 3, 2, 1 };
#define LAND_MIN_MINUTES 10
#define LAND_MAX_MINUTES 15
static int sitesHacked = 0, sitesLaunched = 0;
static float hackT = 0, launchT = 0;        // seconds into the hack / launch under way
static float landLeft[NUKE_SITES];          // a launched one: seconds till it lands (<= 0: landed)
static float landTotal[NUKE_SITES];         // (and how long its flight is)
static bool lockdown = false;               // QUANTUM LOCKDOWN (the main computer): no more hacking
static bool prevented[NUKE_SITES];          // its launch stopped from one of the server room's terminals (it never launches)
#define PREVENT_SCORE 2000
static bool launchOn = false;               // a launch's (or a landing's) cut scene playing (beside the game)
static void resetStrikes();
static int launchQueued = 0;
#define FLYERS 4                            // (their news, flying out at him: see drawFlyers)
#define FLYER_SECONDS 2.6f
struct Flyer { char text[40]; float t; bool on, good; };   // (good: green, not red)
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
enum LabKind : uint8_t { LK_LAB, LK_KITCHEN, LK_MEDICAL, LK_SECURITY, LK_ADMIN, LK_CLEANING, LK_STORE, LK_CLONE, LK_SOLDIER, LK_SERVER, LK_QLAB1, LK_QLAB2, LK_ARMORY, LK_TEST, LK_HAZARD, LK_ARMORY2, LK_ARENA, LK_SIMON, LK_DESTRUCT, LK_BOSS };
static const char *const labKindName[] = { "LAB", "KITCHEN", "MEDICAL BAY", "SECURITY", "ADMIN", "CLEANING", "STORE ROOM", "CLONE LAB", "SOLDIER LAB", "SERVER ROOM", "QUANTUM LAB 1", "QUANTUM LAB 2", "ARMORY", "TEST CHAMBER", "HAZ STORAGE", "ARMORY 2", "SECURITY ZONE", "CODE ROOM", "SELF DESTRUCT", "DR. VOSS" };
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
  float scale, fxT;                          // his size (the QX-9: halved, or doubled); the QX-9's flash on him
  bool burn;                                 // (killed by the FX-5: burning to ash)
  bool foyer;                                // one of lift C's foyer guards (NUCLEAR MISSION's soldier: sees him, aims, fires)
  bool rsold;                                // (the same soldier, but in a room: lifts E's two rooms)
  int8_t lvl, fsh, fslot;                    // (which level's, which lift's (0 C, 1 D), which of its guards)
  uint8_t fstate;                            // (0 waiting, 1 aiming, 2 just fired)
  float fcool;
};
#define MAX_NPCS 3
static Npc npcs[MAX_NPCS];
static bool s2NpcDead[S2_ROOMS][MAX_NPCS];  // who has been killed, where they lie
static float s2NpcDX[S2_ROOMS][MAX_NPCS], s2NpcDY[S2_ROOMS][MAX_NPCS];
static int8_t s2NpcDDir[S2_ROOMS][MAX_NPCS];
static float s2NpcScale[S2_ROOMS][MAX_NPCS];  // (their sizes, the QX-9's doing)
static bool s2NpcAsh[S2_ROOMS][MAX_NPCS];     // (burnt to ash: nothing left of them)
// The foyer guards, on the walkway left of the lift: lift C, one at each of its 4 levels (one of them with the
// level 3 card); lift D, two at each of its 3 levels
#define FG_LIFTS 2                           // (lifts C and D: shafts 2 and 3)
#define FG_MAXL 4
#define FG_MAXS 2
#define FG_X (SHAFT_X0 - 190)
static const int fgLevels[FG_LIFTS] = { 4, 3 }, fgSlots[FG_LIFTS] = { 1, 2 };
static const float fgPostX[FG_LIFTS][FG_MAXS] = { { FG_X, 0 }, { FG_X, FG_X - 100 } };   // (clear of the first aid cabinet)
#define FG_AIM 0.55f                         // NUCLEAR MISSION's soldier: aims this long, fires...
#define FG_RECOVER 0.35f                     // ...lowers it...
#define FG_GRACE 0.5f                        // (and waits this long when he arrives)
#define FG_SHOT_SPEED 560.0f
static bool fgDead[FG_LIFTS][FG_MAXL][FG_MAXS], fgSearched[FG_MAXL];
static float fgDX[FG_LIFTS][FG_MAXL][FG_MAXS], fgScale[FG_LIFTS][FG_MAXL][FG_MAXS];
static int8_t fgDDir[FG_LIFTS][FG_MAXL][FG_MAXS];
static bool fgAsh[FG_LIFTS][FG_MAXL][FG_MAXS];
static int fgCard = 0, fgLoaded = -1;        // (lift C's level whose guard has the card; the walkway in npcs[] now: lift*8 + level)
struct GShot { bool on; float x, y; int dir; float ty; };   // (ty: in a room, the height it levels out at)
static GShot gShots[4];
// The QX-9 (the EXPERIMENTAL ARMORY's prototype): a shot halves someone (1 in 10 doubles them); a quarter size and they're gone
static bool hasXGun = false, xGunTaken = false;
static bool hasFlamer = false, flamerTaken = false;   // the FX-5 (the second armory's flamethrower)
static int arKilled[S2_ROOMS];
static bool arSearched[S2_ROOMS][16];        // (what's been searched in them; the level 5 card's in one)
static int arCardRoom = -1, arCardObs = 0;
enum ArHaz : uint8_t { AH_NONE, AH_DRONE, AH_TURRET };   // (one has a drone, one a wall turret)
static uint8_t arHaz[S2_ROOMS];               // (lift D's top-down rooms: how many each has lost; they don't come back)
// ---- lifts E and F: the CODE ROOM (a sequence game, for a NUKE SELF DESTRUCT CODE), the SELF DESTRUCT room below it
// (the code destroys one missile in flight), and THE PROFESSOR's office (killing him ends the game)
static bool s2NukeCode = false;              // holding a NUKE SELF DESTRUCT CODE (one at a time)
static char nukeCodeText[16] = "";
static bool nukeDestroyed[NUKE_SITES];       // (a launched one, destroyed in flight)
static int s2DestructCard = 1;               // (the self destruct room's thing with the ACCESS CARD LEVEL 5)
static float bossDeadT = -1, congratsT = 0;
static bool congratsDone = false, congratsAll = false;
enum SimState : uint8_t { SIM_SHOW, SIM_INPUT, SIM_GOOD, SIM_BAD, SIM_WON };
static bool simOn = false;                   // at the CODE ROOM's terminal, playing
static int simState = 0, simRound = 0, simPos = 0, simLit = -1, simCurX = 3, simCurY = 2;
static int simSeq[8];
static float simT = 0, simLitT = 0;
static void paintArmory2();
static void arenaEnter(bool fromLeft);
static int s2HazCard = 0;                    // (hazardous storage: which of its 6 things has the level 4 card)
static bool hasKnife = false;                // the kitchen's knife
static bool hasGun = false;                  // the security room's pistol (R1 / R2 to fire)
static int s2GunItem = 0;                    // (which of the two gun racks it's in)
static float secAlertT = -1;                 // the security room: seconds till the guards open fire (< 0: no alert)
static float gunFlashT = 0;
#define MY_SHOTS 4
#define MY_SHOT_SPEED 900.0f                 // (NUCLEAR MISSION's: you see your bullets fly)
struct MyShot { bool on; float x, y, level; int dir; bool ray; };   // (ray: the QX-9's)
static MyShot myShots[MY_SHOTS];
static int s2KnifeItem = 0, s2CardItem = 2;  // which of the kitchen's things the knife is in; which medical bay patient has the level 2 card
static bool panelKnife = false;              // (the FOUND panel: the knife, not a card)
static float shotFlashT = 0;
#define S2_DOCS 15                           // stage 2: TOP SECRET DOCUMENTS in lift A's and B's rooms (1000 points each)...
#define S2_DOC_VALUE 1000
#define VEST_SCORE 500                       // ...and a BULLET-PROOF VEST: a guard's shot only hurts (half), no knock down
#define KILL_TIME_BONUS 300                  // each person he kills: 5 minutes more on the clock
enum S2Hold : uint8_t { S2H_NONE, S2H_VEST, S2H_DOC, S2H_PASS };
#define S2_PASSWORDS 16                      // QUANTUM PASSWORDS: in lift A's, B's and C's rooms; a server room terminal takes 4 to log in
#define PASS_PER_LOGIN 4
#define PASS_SCORE 500
static int s2PassFound = 0;
static bool s2RoomShot[S2_ROOMS];            // (a room someone was shot in: when all its people are dead a drone comes)
static bool nodeUsed[4];                     // the server room's terminals: used (each takes one password, once)
static int passUsed = 0, curNode = 0, snSite = -1;
static char s2PassCode[S2_PASSWORDS][12];    // (what each one says)
static float passShowT = 0;
static uint8_t s2Hold[S2_ROOMS][S2_ITEMS];
static bool hasVest = false;
static int s2DocsFound = 0;
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
  memset(prevented, 0, sizeof(prevented));
  memset(nukeDestroyed, 0, sizeof(nukeDestroyed));
  for (int i = 0; i < NUKE_SITES; i++) hackSecs[i] = i == 0 ? 7 * 60.0f + rnd(3 * 60 + 1) : 2 * 60.0f + rnd(2 * 60 + 1);
  for (int i = 0; i < FLYERS; i++) flyers[i].on = false;
  launchOn = false;
  launchQueued = 0;
  resetStrikes();
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
static int s2ItemCount(int kind);
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
      for (int k = 0; k < MAX_NPCS; k++) { s2NpcDead[n][k] = false; s2NpcScale[n][k] = 1; s2NpcAsh[n][k] = false; }
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
  // lift B's five rooms, at random levels: the clone lab, the super soldier lab (two cameras), the server room (a camera and
  // a drone), QUANTUM LAB 1 (scientists) and QUANTUM LAB 2 (wrecked: holes in the floor)
  {
    int fl[S2_LEVELS - 1] = { 0, 1, 2, 3, 4 };
    for (int i = S2_LEVELS - 2; i > 0; i--) {
      int j = rnd(i + 1), t = fl[i];
      fl[i] = fl[j];
      fl[j] = t;
    }
    const uint8_t bk[5] = { LK_CLONE, LK_SOLDIER, LK_SERVER, LK_QLAB1, LK_QLAB2 }, bh[5] = { HZ_NONE, HZ_CAMERA2, HZ_CAMDRONE, HZ_NONE, HZ_NONE };
    for (int k = 0; k < 5; k++) {
      int idx = s2Room[1][fl[k]];
      s2Kind[idx] = bk[k];
      rooms[idx].hazard = bh[k];
    }
  }
  {                                          // lift C: three of its four rooms the EXPERIMENTAL ARMORY, the TEST CHAMBER, HAZARDOUS STORAGE
    int fl[4] = { 0, 1, 2, 3 };
    for (int i = 3; i > 0; i--) { int j = rnd(i + 1), t = fl[i]; fl[i] = fl[j]; fl[j] = t; }
    s2Kind[s2Room[2][fl[0]]] = LK_ARMORY;
    s2Kind[s2Room[2][fl[1]]] = LK_TEST;
    s2Kind[s2Room[2][fl[2]]] = LK_HAZARD;
    s2Kind[s2Room[2][fl[3]]] = LK_ARMORY2;     // (the FX-5)
    s2HazCard = rnd(6);                      // (the level 4 card: one of its 6 things)
  }
  memset(fgDead, 0, sizeof(fgDead));
  memset(fgAsh, 0, sizeof(fgAsh));
  memset(fgSearched, 0, sizeof(fgSearched));
  for (int a = 0; a < FG_LIFTS; a++) for (int l = 0; l < FG_MAXL; l++) for (int k = 0; k < FG_MAXS; k++) fgScale[a][l][k] = 1;
  fgCard = rnd(fgLevels[0]);
  fgLoaded = -1;
  hasXGun = xGunTaken = false;
  hasFlamer = flamerTaken = false;
  for (int f = 0; f < 3; f++) s2Kind[s2Room[3][f]] = LK_ARENA;   // lift D: SECURITY ZONES, seen from above
  memset(arKilled, 0, sizeof(arKilled));
  memset(arSearched, 0, sizeof(arSearched));
  memset(arHaz, 0, sizeof(arHaz));
  {                                          // one room a drone, one a turret, one just soldiers; the level 5 card in one
    int fl[3] = { 0, 1, 2 };
    for (int i = 2; i > 0; i--) { int j = rnd(i + 1), t = fl[i]; fl[i] = fl[j]; fl[j] = t; }
    arHaz[s2Room[3][fl[0]]] = AH_DRONE;
    arHaz[s2Room[3][fl[1]]] = AH_TURRET;
    arCardRoom = s2Room[3][rnd(3)];
    arCardObs = rnd(11);                     // (lift D's 11 pieces of furniture)
  }
  s2Kind[s2Room[4][0]] = LK_SIMON;          // lift E: the CODE ROOM, the SELF DESTRUCT room below it; lift F: THE PROFESSOR
  s2Kind[s2Room[4][1]] = LK_DESTRUCT;
  s2Kind[s2Room[5][0]] = LK_BOSS;
  for (int k = 0; k < 2; k++) { s2NpcScale[s2Room[4][0]][k] = 2; s2NpcScale[s2Room[4][1]][k] = 2; }   // (their soldiers, twice the size)
  s2NpcScale[s2Room[5][0]][0] = 2;
  s2DestructCard = 1 + rnd(4);
  s2NukeCode = false;
  bossDeadT = -1;
  congratsT = 0;
  congratsDone = congratsAll = false;
  simOn = false;
  simState = 0;
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
  for (int k = 0; k < MY_SHOTS; k++) myShots[k].on = false;
  s2KnifeItem = rnd(8);                      // (one of the kitchen's 8 things)
  s2GunItem = rnd(2);                        // (one of the security room's two gun racks)
  // the vest and three documents: in lift A's rooms, in things that hold nothing else
  memset(s2Hold, 0, sizeof(s2Hold));
  hasVest = false;
  s2DocsFound = 0;
  static const int beds[5] = { 2, 4, 5, 6, 7 };
  s2CardItem = beds[rnd(5)];                 // (one of the medical bay's 5 patients)
  // ...the vest in lift A's rooms; 15 TOP SECRET DOCUMENTS anywhere in lift A's and B's; 16 QUANTUM PASSWORDS in lift A's, B's and C's
  s2PassFound = 0;
  passShowT = 0;
  passUsed = 0;
  memset(nodeUsed, 0, sizeof(nodeUsed));
  memset(s2RoomShot, 0, sizeof(s2RoomShot));
  for (int k = 0; k < 1 + S2_DOCS + S2_PASSWORDS; k++)
    for (int tries = 0; tries < 2000; tries++) {
      bool pass = k > S2_DOCS;
      int s = k == 0 ? 0 : pass ? rnd(3) : rnd(2), f = rnd(S2_LEVELS - s), idx = s2Room[s][f], kind = s2Kind[idx];
      int n = s2ItemCount(kind);
      if (n <= 0) continue;
      int i = rnd(n);
      if (s2Hold[idx][i]) continue;
      if ((kind == LK_KITCHEN && i == s2KnifeItem) || (kind == LK_SECURITY && i == s2GunItem) || (kind == LK_MEDICAL && i == s2CardItem)) continue;
      if (kind == LK_SERVER && i >= 2 && i <= 5) continue;     // (the server room's terminals: not searched)
      if ((kind == LK_ARMORY || kind == LK_ARMORY2) && i == 0) continue;   // (the armories' cases: the guns)
      if (kind == LK_HAZARD && i == s2HazCard) continue;        // (hazardous storage: the level 4 card)
      s2Hold[idx][i] = k == 0 ? S2H_VEST : pass ? S2H_PASS : S2H_DOC;
      break;
    }
  for (int k = 0; k < S2_PASSWORDS; k++) {   // e.g. QX7-R41K
    static const char al[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    char *c = s2PassCode[k];
    c[0] = 'Q';
    for (int j = 1; j < 8; j++) c[j] = al[rnd(32)];
    c[3] = '-';
    c[8] = 0;
  }
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
static float drWakeT = 0;              // a drone just called in (gunfire, everyone dead): it hovers this long before it scans
#define DRONE_WAKE_SECONDS 5.0f
// The clone lab's hot gas: a duct under the deck, 5 vents; they blast down 2 at a time in a 3-step sequence
#define GAS_VENTS 5
#define GAS_WARN 0.9f                  // (a vent's lamp flashes this long first)
#define GAS_BLAST 1.3f
#define GAS_GAP 0.5f
#define GAS_PAUSE 1.4f                 // (after the third step, before it starts again)
#define GAS_HURT 26                    // (a full bar to under half; a second hit and he's done)
static const int16_t gasX[GAS_VENTS] = { 309, 529, 749, 969, 1189 };
static const uint8_t gasSeq[3][2] = { { 0, 2 }, { 1, 3 }, { 2, 4 } };
static const int16_t holeGasX[2] = { 475, 855 };        // Quantum Lab 2: its two holes' middles...
static const uint8_t holeGasSeq[3] = { 0, 0, 1 };       // ...left, left, right
static float gasT = 0;
static int gasHitStep = -1;            // (one hit a blast)
#define GAS_STEP (GAS_WARN + GAS_BLAST + GAS_GAP)
#define GAS_CYCLE (3 * GAS_STEP + GAS_PAUSE)
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
#define MISSILE_BONUS 5000                   // stage 2's end: each missile stopped (a server room) or destroyed (self destruct)
static long bonusPts[BONUS_LINES];
static long bonusTotal = 0;
// The high scores
struct HiEntry { char name[4]; int32_t score; int32_t secs; uint32_t deaths; };   // (deaths: the nuclear toll that game)
static const HiEntry hiDefault[HI_COUNT] = { { "ACE", 25000, 850, 0 }, { "VOS", 20000, 785, 2400000 }, { "MAX", 15000, 700, 5100000 },
                                             { "ZED", 10000, 560, 9800000 }, { "KAT", 5000, 405, 14300000 } };
static HiEntry hiTable[HI_COUNT] = { { "ACE", 25000, 850, 0 }, { "VOS", 20000, 785, 2400000 }, { "MAX", 15000, 700, 5100000 },
                                     { "ZED", 10000, 560, 9800000 }, { "KAT", 5000, 405, 14300000 } };
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
static bool hiResetConfirm = false;    // RESET HIGH SCORES pressed once: X again
static float hiResetDoneT = 0;
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
                     SFX_POWERUP, SFX_CLICK, SFX_DENIED, SFX_KEY, SFX_SHUTDOWN, SFX_WARN, SFX_LOOPPOWER, SFX_DOORS, SFX_TICK24,
                     SFX_KLAXON, SFX_IGNITE, SFX_BIGROAR, SFX_ROAR, SFX_TYPE,   // (those five: NUCLEAR MISSION's launch)
                     SFX_HISS,     // (the clone lab's hot gas)
                     SFX_FALL, SFX_BOOM,     // (a landing: the warhead falling, the detonation)
                     SFX_RAY,                // (the QX-9)
                     SFX_HEART, SFX_HPALERT,   // (low health: a heartbeat, the warning)
                     SFX_FLAME };              // (the FX-5: a roaring whoosh)
#define SFX_TONE 64                          // (64..95: the CODE ROOM's notes)
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

static bool sfxMute = false;                 // (the title's cut scenes: silent, under its tune)
static void sfxPlay(uint8_t sfx) {           // (the game's side of the queue)
  if (sfxMute) return;
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
  if (sfx >= SFX_TONE && sfx < SFX_TONE + 32) {   // the CODE ROOM's squares: a note each (32 semitones up from middle C)
    float f = 262.0f * powf(2.0f, (sfx - SFX_TONE) / 12.0f);
    startVoice(WAVE_SQUARE, f, f, 0, 430, 0.07f);
    startVoice(WAVE_TRIANGLE, f * 0.5f, f * 0.5f, 0, 430, 0.12f);
    return;
  }
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
    case SFX_IGNITE:                           // NUCLEAR MISSION's launch: ignition...
      startVoice(WAVE_NOISE, 5000, 400, 0, 900, 0.45f);
      startVoice(WAVE_SQUARE, 80, 40, 0, 900, 0.30f);
      startVoice(WAVE_NOISE, 1500, 300, 150, 1200, 0.30f);
      break;
    case SFX_BIGROAR:                          // ...the engine close up (repeated)...
      startVoice(WAVE_NOISE, 700, 600, 0, 330, 0.22f);
      startVoice(WAVE_NOISE, 220, 200, 0, 330, 0.25f);
      startVoice(WAVE_SQUARE, 45, 42, 0, 330, 0.12f);
      break;
    case SFX_TYPE:                             // ...the words typing on...
      startVoice(WAVE_SQUARE, 1800, 1800, 0, 12, 0.08f);
      break;
    case SFX_KLAXON:                           // ...the klaxon before ignition...
      startVoice(WAVE_SQUARE, 440, 440, 0, 380, 0.12f);
      startVoice(WAVE_SQUARE, 330, 330, 420, 380, 0.12f);
      break;
    case SFX_ROAR:                             // ...and the engine further off
      startVoice(WAVE_NOISE, 900, 700, 0, 320, 0.10f);
      startVoice(WAVE_NOISE, 300, 260, 0, 320, 0.12f);
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
    case SFX_FLAME:                            // the FX-5: a roaring whoosh (again and again while it's held)
      startVoice(WAVE_NOISE, 1400, 900, 0, 260, 0.20f);
      startVoice(WAVE_NOISE, 300, 200, 0, 260, 0.16f);
      break;
    case SFX_HEART:                            // a heartbeat: two low thumps
      startVoice(WAVE_TRIANGLE, 90, 40, 0, 110, 0.55f);
      startVoice(WAVE_TRIANGLE, 80, 36, 170, 120, 0.40f);
      break;
    case SFX_HPALERT:                          // health down to another 10%: two falling warning tones
      startVoice(WAVE_SQUARE, 880, 660, 0, 160, 0.14f);
      startVoice(WAVE_SQUARE, 660, 440, 190, 220, 0.14f);
      break;
    case SFX_RAY:                              // the QX-9: a falling, warbling zap
      startVoice(WAVE_SQUARE, 2200, 300, 0, 240, 0.12f);
      startVoice(WAVE_TRIANGLE, 900, 1500, 0, 240, 0.14f);
      break;
    case SFX_FALL:                             // a warhead falling: a long whistle down
      startVoice(WAVE_TRIANGLE, 2400, 500, 0, 1600, 0.16f);
      startVoice(WAVE_NOISE, 3000, 1500, 0, 1600, 0.04f);
      break;
    case SFX_BOOM:                             // the detonation: a crack, then a deep long rumble
      startVoice(WAVE_NOISE, 9000, 2000, 0, 250, 0.40f);
      startVoice(WAVE_NOISE, 400, 60, 0, 2600, 0.45f);
      startVoice(WAVE_SQUARE, 50, 22, 0, 2600, 0.22f);
      startVoice(WAVE_TRIANGLE, 80, 30, 0, 2000, 0.30f);
      break;
    case SFX_HISS:                             // hot gas blasting out of a vent
      startVoice(WAVE_NOISE, 7000, 3000, 0, 1100, 0.16f);
      startVoice(WAVE_NOISE, 900, 500, 0, 1100, 0.10f);
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
// lift B's rooms
#define CLONE_DECK_Y   440
static const LabItem cloneItems[] = {
  { 340, ROOM_FLOOR_Y, 170, 110 }, { 512, ROOM_FLOOR_Y, 60, 140 }, { 600, ROOM_FLOOR_Y, 110, 145 }, { 688, ROOM_FLOOR_Y, 60, 140 },   // the bench, DNA, the machine, the body scan
  { 820, ROOM_FLOOR_Y, 120, 140 }, { 975, ROOM_FLOOR_Y, 160, 90 }, { 1150, ROOM_FLOOR_Y, 70, 200 },                               // the cylinders, the gurney, the robot arm
  { 330, CLONE_DECK_Y, 120, 180 }, { 520, CLONE_DECK_Y, 120, 180 }, { 710, CLONE_DECK_Y, 120, 180 },                               // the five tanks
  { 900, CLONE_DECK_Y, 120, 180 }, { 1090, CLONE_DECK_Y, 120, 180 } };
static const LabItem soldierItems[] = {
  { 170, ROOM_FLOOR_Y, 84, 200 }, { 280, ROOM_FLOOR_Y, 100, 200 }, { 405, ROOM_FLOOR_Y, 90, 338 }, { 505, ROOM_FLOOR_Y, 50, 120 },   // the serum fridge, the desk, a side tank, a console
  { 640, ROOM_FLOOR_Y, 180, 380 }, { 775, ROOM_FLOOR_Y, 50, 120 }, { 875, ROOM_FLOOR_Y, 90, 338 }, { 995, ROOM_FLOOR_Y, 84, 160 },  // the big tank, a console, a side tank, the cabinet
  { 1110, ROOM_FLOOR_Y, 76, 210 } };                                                                                                 // the armour stand
static const LabItem serverItems[] = {
  { 180, ROOM_FLOOR_Y, 100, 196 }, { 290, ROOM_FLOOR_Y, 100, 196 },                                                         // two cupboards on the left
  { 420, ROOM_FLOOR_Y, 110, 150 }, { 567, ROOM_FLOOR_Y, 110, 150 }, { 713, ROOM_FLOOR_Y, 110, 150 }, { 860, ROOM_FLOOR_Y, 110, 150 },   // the four terminals
  { 990, ROOM_FLOOR_Y, 100, 196 }, { 1100, ROOM_FLOOR_Y, 100, 196 } };                                                      // two on the right
static const LabItem qlab1Items[] = {
  { 180, ROOM_FLOOR_Y, 60, 150 }, { 280, ROOM_FLOOR_Y, 80, 142 }, { 405, ROOM_FLOOR_Y, 110, 90 }, { 540, ROOM_FLOOR_Y, 120, 140 },   // the fridge, the cages, a bench, the terminal
  { 857, ROOM_FLOOR_Y, 134, 150 }, { 1035, ROOM_FLOOR_Y, 110, 116 }, { 1157, ROOM_FLOOR_Y, 54, 44 },                                // the glove box, the microscope, the centrifuge
  { 835, 440, 90, 160 }, { 950, 440, 60, 70 }, { 1050, 440, 80, 84 }, { 1140, 440, 40, 102 }, { 1215, 440, 50, 120 } };          // the deck: the silver fridge, an incubator, isolation, LN2, filing
static const LabItem qlab2Items[] = {
  { 180, ROOM_FLOOR_Y, 80, 150 }, { 275, ROOM_FLOOR_Y, 70, 60 }, { 369, ROOM_FLOOR_Y, 86, 110 },                                    // the lockers, a containment box, the drums
  { 627, ROOM_FLOOR_Y, 134, 150 }, { 755, ROOM_FLOOR_Y, 40, 102 },                                                                  // the glove box, LN2
  { 990, ROOM_FLOOR_Y, 120, 140 }, { 1165, ROOM_FLOOR_Y, 110, 90 } };                                                               // the terminal, a bench

// ---- lift C: the EXPERIMENTAL ARMORY (the prototype gun under glass, blue light) and the TEST CHAMBER
static const LabItem armoryItems[] = {
  { 640, ROOM_FLOOR_Y, 200, 260 },                                                            // the glass case (the gun)
  { 190, ROOM_FLOOR_Y, 110, 220 }, { 340, ROOM_FLOOR_Y, 140, 110 }, { 470, ROOM_FLOOR_Y, 80, 160 },   // the rack, the workbench, the cell charger
  { 830, ROOM_FLOOR_Y, 100, 110 }, { 945, ROOM_FLOOR_Y, 90, 110 }, { 1080, ROOM_FLOOR_Y, 140, 100 } }; // the gel table, ammo crates, the firing booth
static const LabItem testItems[] = {
  { 230, ROOM_FLOOR_Y, 140, 70 }, { 1000, ROOM_FLOOR_Y, 50, 130 }, { 1100, ROOM_FLOOR_Y, 80, 190 },   // a bench, the water cooler, a locker
  { 342, ROOM_FLOOR_Y, 50, 80 }, { 480, ROOM_FLOOR_Y, 60, 160 }, { 655, ROOM_FLOOR_Y, 170, 90 }, { 820, ROOM_FLOOR_Y, 70, 110 } };   // a laundry bin, the scales, a gurney, a trolley
#define ARMORY_CASE_X0 548
#define ARMORY_CASE_X1 732
#define ARMORY_CASE_Y0 420
#define ARMORY_CASE_Y1 588

// The prototype: a long chrome body, blue coils, a dish at the muzzle (facing right; sc 1 ~ 220 long)
static void drawXGun(float cx, float cy, float sc, float glow) {
  auto P = [&](float x, float y, float &ox, float &oy) { ox = cx + x * sc; oy = cy + y * sc; };
  float xs[8], ys[8];
  const float body[8][2] = { { -110, -14 }, { 40, -20 }, { 70, -12 }, { 70, 12 }, { 10, 16 }, { -30, 16 }, { -60, 10 }, { -110, 8 } };
  for (int i = 0; i < 8; i++) P(body[i][0], body[i][1], xs[i], ys[i]);
  fillPolygonF(xs, ys, 8, rgb565(170, 180, 196));
  const float top[4][2] = { { -100, -14 }, { 40, -20 }, { 50, -16 }, { -100, -8 } };
  for (int i = 0; i < 4; i++) P(top[i][0], top[i][1], xs[i], ys[i]);
  fillPolygonF(xs, ys, 4, rgb565(236, 242, 250));
  const float grip[4][2] = { { -40, 14 }, { -14, 14 }, { -26, 62 }, { -54, 62 } };
  for (int i = 0; i < 4; i++) P(grip[i][0], grip[i][1], xs[i], ys[i]);
  fillPolygonF(xs, ys, 4, rgb565(40, 44, 56));
  const float stock[4][2] = { { -110, -12 }, { -150, -4 }, { -150, 30 }, { -110, 8 } };
  for (int i = 0; i < 4; i++) P(stock[i][0], stock[i][1], xs[i], ys[i]);
  fillPolygonF(xs, ys, 4, rgb565(50, 56, 70));
  for (int k = 0; k < 4; k++) {                          // the coils, glowing
    float x0, y0, x1, y1;
    P(-10 + k * 18, -24, x0, y0);
    P(-2 + k * 18, 20, x1, y1);
    fillRectF(x0, y0, x1, y1, rgb565((int)(60 + 60 * glow), (int)(150 + 80 * glow), 255));
  }
  float bx0, by0, bx1, by1;
  P(70, -6, bx0, by0);
  P(110, 6, bx1, by1);
  fillRectF(bx0, by0, bx1, by1, rgb565(110, 120, 136));  // the barrel
  float dx, dy;
  P(116, 0, dx, dy);
  fillCircle((int)dx, (int)dy, (int)(16 * sc), rgb565(60, 70, 90));   // the emitter dish
  fillCircle((int)dx, (int)dy, (int)(9 * sc), rgb565(120, 210, 255));
  glowDisc(dx, dy, 30 * sc, 90, 190, 255, 0.35f * glow);
  float sx, sy;
  P(-80, -2, sx, sy);
  drawVectorText("QX-9", sx, sy, 1.2f * sc, rgb565(40, 80, 160), true);
}

static void armoryWall() {   // dark gunmetal panels
  for (int y = CEIL_Y + 61; y < ROOM_FLOOR_Y; y += 4) {
    int v = 40 - (y - CEIL_Y) * 10 / (ROOM_FLOOR_Y - CEIL_Y);
    fillRect(0, y, SCREEN_W - 1, y + 3, rgb565(v, v + 4, v + 14));
  }
  for (int x = 0; x < SCREEN_W; x += 160) {
    fillRect(x, CEIL_Y + 61, x + 1, ROOM_FLOOR_Y - 1, rgb565(18, 20, 26));
    fillRect(x + 2, CEIL_Y + 61, x + 2, ROOM_FLOOR_Y - 1, rgb565(56, 62, 76));
    for (int y = CEIL_Y + 80; y < ROOM_FLOOR_Y; y += 120) { fillCircle(x + 10, y, 2, rgb565(70, 76, 90)); fillCircle(x + 150, y, 2, rgb565(70, 76, 90)); }
  }
  fillRect(0, 380, SCREEN_W - 1, 381, rgb565(18, 20, 26));
  fillRect(0, 382, SCREEN_W - 1, 382, rgb565(56, 62, 76));
}

static void paintArmory() {
  // the sign
  fillRect(380, 140, 900, 200, rgb565(8, 12, 24));
  drawRectOutline(380, 140, 900, 200, rgb565(70, 150, 255), 3);
  drawVectorText("EXPERIMENTAL ARMORY", 640, 170, 3.4f, rgb565(140, 200, 255), true);
  // the spotlight's housing over the case
  fillRect(610, CEIL_Y + 58, 670, CEIL_Y + 80, rgb565(60, 66, 80));
  fillRect(620, CEIL_Y + 80, 660, CEIL_Y + 88, rgb565(150, 210, 255));
  // the plinth and the case's frame (the glass and the gun are drawn over it, lit)
  fillRect(540, 600, 740, ROOM_FLOOR_Y - 1, rgb565(30, 34, 44));
  fillRect(540, 600, 740, 604, rgb565(120, 130, 150));
  fillRect(530, 588, 750, 600, rgb565(170, 180, 196));
  fillRect(530, 588, 750, 590, rgb565(230, 236, 246));
  fillRect(560, 636, 720, 660, rgb565(14, 16, 22));
  drawVectorText("PROTOTYPE QX-9", 640, 648, 1.4f, rgb565(120, 190, 255), true);
  fillRect(ARMORY_CASE_X0 - 6, ARMORY_CASE_Y0 - 8, ARMORY_CASE_X1 + 6, ARMORY_CASE_Y0, rgb565(170, 180, 196));   // its lid
  fillRect(ARMORY_CASE_X0 - 6, ARMORY_CASE_Y0, ARMORY_CASE_X0, ARMORY_CASE_Y1, rgb565(150, 160, 176));
  fillRect(ARMORY_CASE_X1, ARMORY_CASE_Y0, ARMORY_CASE_X1 + 6, ARMORY_CASE_Y1, rgb565(150, 160, 176));
  // the rack of prototypes on the left
  {
    int x0 = 135, x1 = 245, top = ROOM_FLOOR_Y - 220;
    fillRect(x0, top, x1, ROOM_FLOOR_Y - 1, rgb565(50, 56, 68));
    drawRectOutline(x0, top, x1, ROOM_FLOOR_Y - 1, rgb565(90, 98, 116), 2);
    for (int k = 0; k < 3; k++) {
      int y = top + 40 + k * 56;
      fillRect(x0 + 6, y + 20, x1 - 6, y + 24, rgb565(90, 98, 116));          // its pegs' bar
      int gx[6] = { x0 + 14, x1 - 20, x1 - 10, x1 - 14, x0 + 40, x0 + 14 }, gy[6] = { y, y - 4, y + 2, y + 10, y + 12, y + 12 };
      fillPolygon(gx, gy, 6, rgb565(30, 32, 40));
      fillRect(x0 + 50, y + 2, x0 + 60, y + 9, k == 1 ? rgb565(255, 140, 40) : rgb565(80, 170, 255));
      fillRect(x0 + 38, y + 10, x0 + 48, y + 26, rgb565(30, 32, 40));
    }
    fillRect(x0 + 10, top + 6, x1 - 10, top + 20, rgb565(14, 16, 22));
    drawVectorText("PROTOTYPES", (x0 + x1) / 2, top + 13, 1.0f, rgb565(150, 200, 255), false);
  }
  // the workbench: a vice, parts, a laptop with a schematic
  {
    int x0 = 270, x1 = 410, top = ROOM_FLOOR_Y - 90;
    fillRect(x0, top, x1, top + 8, rgb565(120, 96, 70));
    fillRect(x0 + 6, top + 8, x0 + 12, ROOM_FLOOR_Y - 1, rgb565(70, 74, 86));
    fillRect(x1 - 12, top + 8, x1 - 6, ROOM_FLOOR_Y - 1, rgb565(70, 74, 86));
    fillRect(x0 + 10, top - 22, x0 + 40, top, rgb565(60, 90, 140));          // the vice
    fillRect(x0 + 4, top - 30, x0 + 46, top - 22, rgb565(80, 110, 160));
    fillRect(x0 + 52, top - 6, x0 + 90, top - 2, rgb565(170, 176, 186));     // a barrel, springs
    for (int k = 0; k < 4; k++) drawCircle(x0 + 58 + k * 8, top - 12, 4, rgb565(200, 200, 210));
    int lx[4] = { x1 - 60, x1 - 14, x1 - 20, x1 - 54 }, ly[4] = { top, top, top - 40, top - 40 };
    fillPolygon(lx, ly, 4, rgb565(30, 34, 44));                              // the laptop
    fillRect(x1 - 52, top - 36, x1 - 22, top - 6, rgb565(10, 30, 60));
    drawLine(x1 - 48, top - 20, x1 - 26, top - 20, rgb565(90, 190, 255), false);
    drawLine(x1 - 40, top - 32, x1 - 40, top - 10, rgb565(90, 190, 255), false);
  }
  // the energy cell charger: six cells glowing in their slots
  {
    int x0 = 432, x1 = 508, top = ROOM_FLOOR_Y - 160;
    fillRect(x0, top, x1, ROOM_FLOOR_Y - 1, rgb565(44, 48, 60));
    drawRectOutline(x0, top, x1, ROOM_FLOOR_Y - 1, rgb565(90, 98, 116), 2);
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < 2; c++) {
        int cx = x0 + 20 + c * 36, cy = top + 26 + r * 40;
        fillRect(cx - 10, cy - 14, cx + 10, cy + 14, rgb565(14, 16, 22));
        fillRect(cx - 6, cy - 12, cx + 6, cy + 12, rgb565(80, 220, 255));
        fillRect(cx - 3, cy - 16, cx + 3, cy - 12, rgb565(170, 176, 186));
      }
    drawVectorText("CELLS", (x0 + x1) / 2, ROOM_FLOOR_Y - 22, 1.1f, rgb565(150, 200, 255), false);
  }
  // the gel table: a block of ballistic gel with a track through it
  {
    int x0 = 780, x1 = 880, top = ROOM_FLOOR_Y - 70;
    fillRect(x0, top, x1, top + 6, rgb565(150, 156, 166));
    fillRect(x0 + 6, top + 6, x0 + 10, ROOM_FLOOR_Y - 1, rgb565(90, 96, 106));
    fillRect(x1 - 10, top + 6, x1 - 6, ROOM_FLOOR_Y - 1, rgb565(90, 96, 106));
    fillRect(x0 + 14, top - 40, x1 - 14, top, rgb565(230, 200, 110));
    fillRect(x0 + 14, top - 40, x0 + 22, top, rgb565(250, 226, 150));
    drawLine(x0 + 16, top - 22, x1 - 30, top - 18, rgb565(160, 110, 50), true);
    drawVectorText("GEL", (x0 + x1) / 2, top - 52, 1.1f, rgb565(150, 200, 255), false);
  }
  // ammo crates, stencilled
  for (int k = 0; k < 3; k++) {
    int x0 = 902 + (k == 2 ? 20 : k * 46), y1 = ROOM_FLOOR_Y - (k == 2 ? 52 : 0), y0 = y1 - 52;
    fillRect(x0, y0, x0 + 42, y1 - 1, rgb565(80, 92, 56));
    drawRectOutline(x0, y0, x0 + 42, y1 - 1, rgb565(50, 58, 36), 2);
    drawVectorText("AMMO", x0 + 21, (y0 + y1) / 2, 1.0f, rgb565(230, 200, 60), false);
  }
  // the firing booth: a target on the wall, a counter, a red laser emitter
  {
    int x0 = 1010, x1 = 1150;
    int tx = 1080, ty = 460;
    fillRect(tx - 40, ty - 70, tx + 40, ty + 70, rgb565(236, 232, 220));     // the paper target, a silhouette on it
    fillCircle(tx, ty - 38, 16, rgb565(30, 30, 34));
    int bx[4] = { tx - 30, tx + 30, tx + 34, tx - 34 }, by[4] = { ty - 18, ty - 18, ty + 66, ty + 66 };
    fillPolygon(bx, by, 4, rgb565(30, 30, 34));
    drawCircle(tx, ty + 10, 18, rgb565(236, 232, 220));
    drawCircle(tx, ty + 10, 34, rgb565(236, 232, 220));
    for (int k = 0; k < 5; k++) fillCircle(tx - 14 + (k * 13) % 30, ty - 4 + (k * 17) % 34, 3, rgb565(240, 240, 240));
    fillRect(x0, ROOM_FLOOR_Y - 100, x1, ROOM_FLOOR_Y - 92, rgb565(120, 130, 150));   // the counter
    fillRect(x0 + 6, ROOM_FLOOR_Y - 92, x1 - 6, ROOM_FLOOR_Y - 1, rgb565(44, 48, 60));
    fillRect(x0 + 6, ROOM_FLOOR_Y - 60, x1 - 6, ROOM_FLOOR_Y - 56, rgb565(240, 196, 40));
    fillRect(x0 + 10, ROOM_FLOOR_Y - 118, x0 + 40, ROOM_FLOOR_Y - 100, rgb565(30, 34, 44));   // the emitter
    fillCircle(x0 + 40, ROOM_FLOOR_Y - 109, 4, rgb565(255, 40, 40));
  }
}

static void paintTestRoom() {
  fillRect(380, 140, 900, 200, rgb565(240, 196, 40));                        // the sign
  drawRectOutline(380, 140, 900, 200, rgb565(20, 20, 22), 3);
  drawVectorText("TEST CHAMBER", 640, 170, 3.6f, rgb565(20, 20, 22), true);
  fillRect(500, 240, 780, 360, rgb565(70, 76, 86));                          // the observation window
  fillRect(510, 250, 770, 350, rgb565(20, 36, 54));
  drawLine(530, 340, 600, 260, rgb565(60, 90, 120), false);
  drawLine(560, 340, 630, 260, rgb565(60, 90, 120), false);
  drawVectorText("OBSERVATION", 640, 376, 1.4f, rgb565(90, 100, 120), false);
  for (int hc = 0; hc < 2; hc++) {                                           // height charts on the wall
    int x = hc ? 880 : 380, top = ROOM_FLOOR_Y - 400;
    fillRect(x, top, x + 34, ROOM_FLOOR_Y - 1, rgb565(240, 242, 246));
    drawRectOutline(x, top, x + 34, ROOM_FLOOR_Y - 1, rgb565(150, 156, 166), 1);
    for (int k = 0; k <= 20; k++) {
      int y = ROOM_FLOOR_Y - 1 - k * 20;
      fillRect(x, y, x + (k % 5 == 0 ? 20 : 10), y, rgb565(40, 40, 46));
      if (k % 5 == 0 && k) {
        char b[8];
        snprintf(b, sizeof(b), "%d", k / 5);
        drawVectorText(b, x + 27, y, 1.0f, rgb565(40, 40, 46), false);
      }
    }
  }
  fillRect(140, 300, 300, 360, rgb565(200, 40, 40));                         // a warning
  drawVectorText("TEST SUBJECTS", 220, 320, 1.4f, rgb565(250, 250, 250), true);
  drawVectorText("ONLY", 220, 344, 1.4f, rgb565(250, 250, 250), true);
  // the bench, the water cooler, a locker
  fillRect(160, ROOM_FLOOR_Y - 44, 300, ROOM_FLOOR_Y - 36, rgb565(140, 110, 80));
  fillRect(170, ROOM_FLOOR_Y - 36, 176, ROOM_FLOOR_Y - 1, rgb565(90, 96, 106));
  fillRect(284, ROOM_FLOOR_Y - 36, 290, ROOM_FLOOR_Y - 1, rgb565(90, 96, 106));
  fillRect(978, ROOM_FLOOR_Y - 80, 1022, ROOM_FLOOR_Y - 1, rgb565(220, 224, 230));
  fillRect(984, ROOM_FLOOR_Y - 130, 1016, ROOM_FLOOR_Y - 80, rgb565(150, 200, 240));
  fillRect(984, ROOM_FLOOR_Y - 134, 1016, ROOM_FLOOR_Y - 128, rgb565(70, 110, 170));
  fillRect(994, ROOM_FLOOR_Y - 70, 1006, ROOM_FLOOR_Y - 64, rgb565(60, 120, 200));
  fillRect(1060, ROOM_FLOOR_Y - 190, 1140, ROOM_FLOOR_Y - 1, rgb565(150, 160, 176));
  fillRect(1099, ROOM_FLOOR_Y - 186, 1101, ROOM_FLOOR_Y - 4, rgb565(90, 96, 108));
  for (int y = ROOM_FLOOR_Y - 176; y < ROOM_FLOOR_Y - 150; y += 6) { fillRect(1066, y, 1094, y + 2, rgb565(90, 96, 108)); fillRect(1106, y, 1134, y + 2, rgb565(90, 96, 108)); }
  // a laundry bin of orange jumpsuits
  fillRect(318, ROOM_FLOOR_Y - 70, 366, ROOM_FLOOR_Y - 6, rgb565(110, 120, 136));
  fillRect(318, ROOM_FLOOR_Y - 70, 366, ROOM_FLOOR_Y - 64, rgb565(150, 160, 176));
  fillRect(322, ROOM_FLOOR_Y - 80, 346, ROOM_FLOOR_Y - 68, rgb565(240, 130, 40));
  fillRect(340, ROOM_FLOOR_Y - 84, 362, ROOM_FLOOR_Y - 70, rgb565(220, 110, 30));
  fillCircle(324, ROOM_FLOOR_Y - 4, 4, rgb565(40, 40, 46));
  fillCircle(360, ROOM_FLOOR_Y - 4, 4, rgb565(40, 40, 46));
  drawVectorText("SUBJECTS", 342, ROOM_FLOOR_Y - 40, 0.9f, rgb565(230, 234, 240), false);
  // the scales: a platform, a column, the sliding weights
  fillRect(452, ROOM_FLOOR_Y - 14, 508, ROOM_FLOOR_Y - 1, rgb565(80, 86, 96));
  fillRect(496, ROOM_FLOOR_Y - 150, 504, ROOM_FLOOR_Y - 14, rgb565(170, 176, 186));
  fillRect(462, ROOM_FLOOR_Y - 152, 512, ROOM_FLOOR_Y - 144, rgb565(200, 204, 212));
  fillRect(470, ROOM_FLOOR_Y - 156, 478, ROOM_FLOOR_Y - 140, rgb565(60, 64, 72));
  fillRect(492, ROOM_FLOOR_Y - 120, 520, ROOM_FLOOR_Y - 104, rgb565(230, 234, 240));
  fillRect(496, ROOM_FLOOR_Y - 116, 516, ROOM_FLOOR_Y - 108, rgb565(30, 40, 30));
  // a gurney: a padded top, a pillow, the frame on wheels, a strap
  fillRect(574, ROOM_FLOOR_Y - 70, 736, ROOM_FLOOR_Y - 58, rgb565(120, 170, 190));
  fillRect(574, ROOM_FLOOR_Y - 76, 612, ROOM_FLOOR_Y - 70, rgb565(240, 242, 246));
  fillRect(660, ROOM_FLOOR_Y - 72, 672, ROOM_FLOOR_Y - 56, rgb565(60, 50, 40));
  fillRect(586, ROOM_FLOOR_Y - 58, 592, ROOM_FLOOR_Y - 10, rgb565(150, 156, 166));
  fillRect(718, ROOM_FLOOR_Y - 58, 724, ROOM_FLOOR_Y - 10, rgb565(150, 156, 166));
  fillRect(580, ROOM_FLOOR_Y - 34, 730, ROOM_FLOOR_Y - 30, rgb565(150, 156, 166));
  fillCircle(589, ROOM_FLOOR_Y - 6, 6, rgb565(40, 40, 46));
  fillCircle(721, ROOM_FLOOR_Y - 6, 6, rgb565(40, 40, 46));
  // a trolley: two trays of syringes and sample jars
  fillRect(788, ROOM_FLOOR_Y - 104, 852, ROOM_FLOOR_Y - 98, rgb565(190, 196, 206));
  fillRect(788, ROOM_FLOOR_Y - 54, 852, ROOM_FLOOR_Y - 48, rgb565(190, 196, 206));
  fillRect(790, ROOM_FLOOR_Y - 104, 794, ROOM_FLOOR_Y - 10, rgb565(150, 156, 166));
  fillRect(846, ROOM_FLOOR_Y - 104, 850, ROOM_FLOOR_Y - 10, rgb565(150, 156, 166));
  for (int k = 0; k < 4; k++) fillRect(798 + k * 12, ROOM_FLOOR_Y - 120, 804 + k * 12, ROOM_FLOOR_Y - 104, k & 1 ? rgb565(240, 130, 40) : rgb565(120, 200, 140));
  fillRect(800, ROOM_FLOOR_Y - 62, 840, ROOM_FLOOR_Y - 56, rgb565(230, 234, 240));
  fillCircle(794, ROOM_FLOOR_Y - 5, 5, rgb565(40, 40, 46));
  fillCircle(846, ROOM_FLOOR_Y - 5, 5, rgb565(40, 40, 46));
}


// ---- lift C: HAZARDOUS STORAGE. Drums of green waste on a shelf up the wall, three of them leaking - a drip
// from each in turn - and a puddle of it on the floor to jump. The fumes eat at him steadily while he's in
// here: from full health, about long enough to walk in, search three things and start a fourth. The ACCESS CARD LEVEL 4 is in one of its 6 things.
static const LabItem hazItems[] = {
  { 180, ROOM_FLOOR_Y, 90, 200 }, { 310, ROOM_FLOOR_Y, 110, 150 }, { 455, ROOM_FLOOR_Y, 120, 110 },   // the hazmat locker, the chemical cabinet, barrels on a pallet
  { 830, ROOM_FLOOR_Y, 110, 100 }, { 965, ROOM_FLOOR_Y, 100, 120 }, { 1105, ROOM_FLOOR_Y, 100, 210 } };  // the lead container, the spill cart, the decon shower
#define HZ_SHELF_Y 300                       // the shelf's top (the drums stand on it)
#define HZ_PUDDLE_X 640
#define HZ_PUDDLE_W 92                       // (jumpable)
#define HZ_PUDDLE_HURT 26                    // 51% of a full bar
#define HZ_DRIP_HURT 13                      // 26%
#define HZ_AIR_RATE 2.0f                     // health a second, just for being in here (a full bar in 25 seconds)
#define HZ_DRIP_EVERY 0.9f                   // (the leaks in turn: left, middle, right, a pause)
#define HZ_DRIP_CYCLE 3.3f
#define HZ_DRIP_GROW 0.5f                    // (a drop swelling at the leak before it falls)
static const int16_t hzLeakX[3] = { 400, 640, 880 };
static void hurt(int amount);
static void nkBio(int x, int y, int r);
static float hzAcc = 0, hzPuddleCool = 0, hzLastX = 0;
static int hzDripHit = -1;

static void paintHazStorage() {
  fillRect(380, 130, 900, 190, rgb565(240, 196, 40));                        // the sign
  drawRectOutline(380, 130, 900, 190, rgb565(20, 20, 22), 3);
  drawVectorText("HAZARDOUS STORAGE", 640, 160, 3.2f, rgb565(20, 20, 22), true);
  nkBio(340, 160, 22);
  nkBio(940, 160, 22);
  // the shelf, its brackets, the drums on it
  fillRect(120, HZ_SHELF_Y, 1160, HZ_SHELF_Y + 10, rgb565(120, 126, 136));
  fillRect(120, HZ_SHELF_Y, 1160, HZ_SHELF_Y + 2, rgb565(190, 196, 206));
  for (int x = 150; x < 1160; x += 200) {
    int bx[3] = { x, x + 12, x }, by[3] = { HZ_SHELF_Y + 10, HZ_SHELF_Y + 10, HZ_SHELF_Y + 50 };
    fillPolygon(bx, by, 3, rgb565(100, 106, 116));
  }
  for (int d = 0; d < 9; d++) {
    int cx = 160 + d * 120, top = HZ_SHELF_Y - 92;
    bool leak = cx == 400 || cx == 640 || cx == 880;
    fillRect(cx - 34, top, cx + 34, HZ_SHELF_Y - 1, rgb565(40, 120, 50));
    fillRect(cx - 34, top, cx - 24, HZ_SHELF_Y - 1, rgb565(70, 160, 80));
    fillRect(cx + 24, top, cx + 34, HZ_SHELF_Y - 1, rgb565(26, 86, 34));
    for (int r = 0; r < 2; r++) fillRect(cx - 34, top + 24 + r * 40, cx + 34, top + 28 + r * 40, rgb565(26, 80, 32));   // its hoops
    fillRect(cx - 30, top - 4, cx + 30, top, rgb565(30, 90, 38));
    int tx[3] = { cx, cx + 14, cx - 14 }, ty[3] = { top + 34, top + 58, top + 58 };   // a hazard triangle on it
    fillPolygon(tx, ty, 3, rgb565(240, 200, 40));
    drawVectorText("!", (float)cx, top + 50.0f, 1.2f, rgb565(20, 20, 22), true);
    if (leak) {                                                              // split, its green running down it and off the shelf
      drawLine(cx + 8, top + 64, cx + 14, top + 80, rgb565(20, 30, 20), true);
      fillRect(cx + 10, top + 70, cx + 16, HZ_SHELF_Y + 10, rgb565(140, 255, 90));
      fillRect(cx + 9, HZ_SHELF_Y + 10, cx + 17, HZ_SHELF_Y + 16, rgb565(140, 255, 90));
    }
  }
  // the floor's things
  {                                                                          // the hazmat locker: a yellow suit behind the glass
    int x0 = 135, x1 = 225, top = ROOM_FLOOR_Y - 200;
    fillRect(x0, top, x1, ROOM_FLOOR_Y - 1, rgb565(150, 160, 176));
    fillRect(x0 + 10, top + 14, x1 - 10, ROOM_FLOOR_Y - 30, rgb565(60, 80, 96));
    fillCircle((x0 + x1) / 2, top + 50, 18, rgb565(240, 210, 40));
    fillRect((x0 + x1) / 2 - 10, top + 44, (x0 + x1) / 2 + 10, top + 56, rgb565(60, 100, 140));
    fillRect(x0 + 22, top + 68, x1 - 22, top + 150, rgb565(240, 210, 40));
    drawVectorText("HAZMAT", (x0 + x1) / 2, ROOM_FLOOR_Y - 16, 1.1f, rgb565(20, 20, 22), true);
  }
  {                                                                          // the chemical cabinet
    int x0 = 255, x1 = 365, top = ROOM_FLOOR_Y - 150;
    fillRect(x0, top, x1, ROOM_FLOOR_Y - 1, rgb565(240, 200, 40));
    fillRect((x0 + x1) / 2 - 1, top + 6, (x0 + x1) / 2 + 1, ROOM_FLOOR_Y - 6, rgb565(150, 120, 20));
    fillRect(x0 + 10, top + 14, x1 - 10, top + 40, rgb565(200, 40, 30));
    drawVectorText("FLAMMABLE", (x0 + x1) / 2, top + 27, 1.0f, rgb565(250, 250, 250), true);
    fillRect((x0 + x1) / 2 - 8, top + 70, (x0 + x1) / 2 - 4, top + 96, rgb565(60, 60, 66));
    fillRect((x0 + x1) / 2 + 4, top + 70, (x0 + x1) / 2 + 8, top + 96, rgb565(60, 60, 66));
  }
  for (int k = 0; k < 4; k++) {                                              // barrels on a pallet
    int bx = 410 + (k % 2) * 44 + (k / 2) * 6, by1 = ROOM_FLOOR_Y - 14 - (k / 2) * 0;
    if (k >= 2) continue;
    fillRect(bx - 18, by1 - 84, bx + 18, by1, k ? rgb565(60, 120, 180) : rgb565(220, 120, 40));
    fillRect(bx - 18, by1 - 60, bx + 18, by1 - 56, rgb565(30, 30, 34));
    fillRect(bx - 18, by1 - 30, bx + 18, by1 - 26, rgb565(30, 30, 34));
  }
  fillRect(395, ROOM_FLOOR_Y - 14, 515, ROOM_FLOOR_Y - 1, rgb565(150, 116, 70));
  fillRect(395, ROOM_FLOOR_Y - 14, 515, ROOM_FLOOR_Y - 11, rgb565(190, 150, 90));
  fillRect(482, ROOM_FLOOR_Y - 64, 512, ROOM_FLOOR_Y - 14, rgb565(220, 220, 224));   // (a small white drum, a skull)
  fillCircle(497, ROOM_FLOOR_Y - 42, 7, rgb565(30, 30, 34));
  {                                                                          // the lead-lined container
    int x0 = 775, x1 = 885, top = ROOM_FLOOR_Y - 100;
    fillRect(x0, top, x1, ROOM_FLOOR_Y - 1, rgb565(80, 86, 96));
    fillRect(x0, top, x1, top + 8, rgb565(130, 136, 146));
    drawVectorText("LEAD", (x0 + x1) / 2, top + 40, 1.6f, rgb565(200, 206, 216), true);
    int rx = (x0 + x1) / 2, ry = top + 72;                                   // a radiation sign
    fillCircle(rx, ry, 14, rgb565(240, 200, 40));
    for (int k = 0; k < 3; k++) {
      float a = -PI / 2 + k * 2 * PI / 3;
      int px[3] = { rx, rx + (int)(13 * cosf(a - 0.5f)), rx + (int)(13 * cosf(a + 0.5f)) }, py[3] = { ry, ry + (int)(13 * sinf(a - 0.5f)), ry + (int)(13 * sinf(a + 0.5f)) };
      fillPolygon(px, py, 3, rgb565(20, 20, 22));
    }
    fillCircle(rx, ry, 3, rgb565(240, 200, 40));
  }
  {                                                                          // the spill cart: absorbent pads, a bucket, a mop
    int x0 = 918, x1 = 1012, top = ROOM_FLOOR_Y - 90;
    fillRect(x0, top, x1, top + 6, rgb565(200, 40, 30));
    fillRect(x0, top + 50, x1, top + 56, rgb565(200, 40, 30));
    fillRect(x0 + 4, top, x0 + 8, ROOM_FLOOR_Y - 10, rgb565(120, 126, 136));
    fillRect(x1 - 8, top, x1 - 4, ROOM_FLOOR_Y - 10, rgb565(120, 126, 136));
    for (int k = 0; k < 3; k++) fillRect(x0 + 14, top - 10 - k * 9, x1 - 30, top - 2 - k * 9, rgb565(236, 236, 230));
    fillRect(x0 + 20, top + 20, x0 + 50, top + 50, rgb565(240, 200, 40));
    drawLine(x1 - 14, top - 70, x1 - 20, ROOM_FLOOR_Y - 8, rgb565(160, 120, 70), true);
    drawVectorText("SPILL KIT", (x0 + x1) / 2, top + 30, 1.0f, rgb565(250, 250, 250), true);
    fillCircle(x0 + 10, ROOM_FLOOR_Y - 6, 6, rgb565(30, 30, 34));
    fillCircle(x1 - 10, ROOM_FLOOR_Y - 6, 6, rgb565(30, 30, 34));
  }
  {                                                                          // the decontamination shower
    int x0 = 1055, x1 = 1155, top = ROOM_FLOOR_Y - 210;
    fillRect(x0, top, x0 + 6, ROOM_FLOOR_Y - 1, rgb565(150, 156, 166));
    fillRect(x1 - 6, top, x1, ROOM_FLOOR_Y - 1, rgb565(150, 156, 166));
    fillRect(x0, top, x1, top + 8, rgb565(150, 156, 166));
    fillRect(x0 + 6, top + 8, x1 - 6, ROOM_FLOOR_Y - 1, rgb565(200, 226, 236));
    fillRect((x0 + x1) / 2 - 3, top + 8, (x0 + x1) / 2 + 3, top + 30, rgb565(120, 126, 136));
    fillRect((x0 + x1) / 2 - 16, top + 30, (x0 + x1) / 2 + 16, top + 36, rgb565(120, 126, 136));
    fillRect(x0 + 6, top + 50, x1 - 6, top + 70, rgb565(60, 170, 80));
    drawVectorText("DECON", (x0 + x1) / 2, top + 60, 1.4f, rgb565(250, 250, 250), true);
    fillRect(x1 - 26, top + 90, x1 - 14, top + 120, rgb565(30, 120, 50));    // its pull handle
  }
  // stains on the wall under the leaks
  for (int k = 0; k < 3; k++)
    for (int y = HZ_SHELF_Y + 24; y < ROOM_FLOOR_Y - 20; y += 22)
      fillRect(hzLeakX[k] + 11, y, hzLeakX[k] + 15, y + 8, rgb565(196, 222, 186));
}

static void paintHazPuddle() {   // (after the floor: the green on it)
  int x0 = HZ_PUDDLE_X - HZ_PUDDLE_W / 2, x1 = HZ_PUDDLE_X + HZ_PUDDLE_W / 2;
  int px[8] = { x0 - 6, x0 + 10, x0 + 40, x1 - 20, x1 + 6, x1 - 4, x0 + 50, x0 + 4 };
  int py[8] = { ROOM_FLOOR_Y + 2, ROOM_FLOOR_Y - 4, ROOM_FLOOR_Y - 5, ROOM_FLOOR_Y - 4, ROOM_FLOOR_Y + 2, ROOM_FLOOR_Y + 8, ROOM_FLOOR_Y + 10, ROOM_FLOOR_Y + 8 };
  fillPolygon(px, py, 8, rgb565(90, 220, 60));
  fillRect(x0 + 14, ROOM_FLOOR_Y - 2, x0 + 40, ROOM_FLOOR_Y, rgb565(200, 255, 160));
}

// Each frame in there: the air, the searching, the drips, the puddle
static void updateHazStorage(float dt) {
  if (agState == ST_KO || gameOver != END_NONE) return;
  float rate = HZ_AIR_RATE;
  hzAcc += rate * dt;
  int whole = (int)hzAcc;
  if (whole > 0) {
    hzAcc -= whole;
    health -= whole;
    if (health <= 0) { health = 0; hurtFlashT = 1.0f; endGame(END_HEALTH); return; }
    if (health <= HEALTH_MAX / 4 && msgT <= 0) showMsg("TOXIC FUMES - GET OUT AND FIND FIRST AID", 2.5f);
  }
  // the drips: each leak's drop, swelling then falling (they can be in the air together)
  float c = fmodf(clockT, HZ_DRIP_CYCLE);
  for (int k = 0; k < 3; k++) {
    float u = c - k * HZ_DRIP_EVERY - HZ_DRIP_GROW;
    if (u > 0 && u < 1.0f) {
      float y = HZ_SHELF_Y + 16 + 0.5f * 1500 * u * u, x = hzLeakX[k] + 13;
      int id = (int)(clockT / HZ_DRIP_CYCLE) * 3 + k;
      float top = agY - 130 * myScale;
      if (hzDripHit != id && y < ROOM_FLOOR_Y && y > top && y < agY && fabsf(agX - x) < 24) {
        hzDripHit = id;
        hurt(HZ_DRIP_HURT);
        sfxPlay(SFX_SCREAM);
        showMsg("TOXIC DRIP!", 1.4f);
      }
    }
  }
  // the puddle: on the floor in it and it burns - and back out the way he came
  if (hzPuddleCool > 0) hzPuddleCool -= dt;
  bool inIt = agState != ST_JUMP && fabsf(agY - ROOM_FLOOR_Y) < 1 && fabsf(agX - HZ_PUDDLE_X) < HZ_PUDDLE_W / 2.0f;   // (over it in a jump: fine)
  if (inIt && hzPuddleCool <= 0 && gameOver == END_NONE) {
    hzPuddleCool = 1.0f;
    hurt(HZ_PUDDLE_HURT);
    sfxPlay(SFX_SCREAM);
    showMsg("BURNT BY TOXIC WASTE!", 1.6f);
    agX = hzLastX < HZ_PUDDLE_X ? HZ_PUDDLE_X - HZ_PUDDLE_W / 2.0f - 30 : HZ_PUDDLE_X + HZ_PUDDLE_W / 2.0f + 30;
  }
  if (!inIt) hzLastX = agX;
}

static void drawHazStorage() {   // (in front: the drops, the puddle bubbling, the fumes)
  float c = fmodf(clockT, HZ_DRIP_CYCLE);
  for (int k = 0; k < 3; k++) {
    float into = c - k * HZ_DRIP_EVERY, x = hzLeakX[k] + 13;
    if (into < 0 || into > HZ_DRIP_GROW + 1.0f) continue;
    if (into < HZ_DRIP_GROW) {                                              // swelling at the leak
      float r = 3 + 5 * into / HZ_DRIP_GROW;
      fillCircle((int)x, HZ_SHELF_Y + 16 + (int)r, (int)r, rgb565(140, 255, 90));
    } else {
      float u = into - HZ_DRIP_GROW, y = HZ_SHELF_Y + 16 + 0.5f * 1500 * u * u;
      if (y < ROOM_FLOOR_Y) {
        glowDisc(x, y, 18, 120, 255, 80, 0.4f);
        fillCircle((int)x, (int)y, 7, rgb565(140, 255, 90));
        fillRect((int)x - 3, (int)y - 16, (int)x + 3, (int)y - 4, rgb565(140, 255, 90));
      } else if (y < ROOM_FLOOR_Y + 300) {                                  // a splash
        float s2 = fminf(1.0f, (y - ROOM_FLOOR_Y) / 300);
        for (int j = -2; j <= 2; j++) fillCircle((int)(x + j * 10 * (0.4f + s2)), ROOM_FLOOR_Y - 4 - (int)(10 * (1 - s2) * (2 - abs(j))), 3, rgb565(140, 255, 90));
      }
    }
  }
  for (int b = 0; b < 4; b++) {                                             // the puddle bubbling
    float u = fmodf(clockT * 0.8f + b * 0.27f, 1.0f);
    drawCircle(HZ_PUDDLE_X - 30 + b * 20, ROOM_FLOOR_Y - 2 - (int)(6 * u), 2 + (int)(3 * u), rgb565(200, 255, 160));
  }
  glowDisc(HZ_PUDDLE_X, ROOM_FLOOR_Y - 20, 90, 120, 255, 80, 0.18f + 0.05f * sinf(clockT * 3));
  for (int f = 0; f < 5; f++) {                                             // fumes drifting up
    float u = fmodf(clockT * 0.12f + f * 0.2f, 1.0f);
    glowDisc(200 + f * 220 + 40 * sinf(clockT * 0.5f + f), ROOM_FLOOR_Y - u * 400, 70, 120, 220, 90, 0.10f * (1 - u));
  }
}

// ---- lift E's rooms: what's in them to use and search
#define SIM_COLS 8                           // the CODE ROOM's board: 8 x 4 squares, three quarters of the room
#define SIM_ROWS 4
#define SIM_X0 40
#define SIM_Y0 128
#define SIM_CW 110
#define SIM_CH 98
#define SIM_TERM_X 480                       // (its terminal: under the board's middle)
#define SIM_ROUNDS 3                         // (3 squares, then 4, then 5)
#define SIM_ON 0.55f
#define SIM_GAP 0.22f
#define MD_X0 944                            // the missiles' display, right of the board
#define MD_X1 1228
#define MD_Y0 120
#define MD_Y1 640
#define DS_CONSOLE_X 640
static const LabItem simonItems[] = { { SIM_TERM_X, ROOM_FLOOR_Y, 130, 140 } };   // (the terminal: not searched - played)
static const LabItem destructItems[] = {
  { DS_CONSOLE_X, ROOM_FLOOR_Y, 200, 150 },                                      // the self destruct console (used, not searched)
  { 170, ROOM_FLOOR_Y, 70, 130 }, { 330, ROOM_FLOOR_Y, 96, 196 }, { 930, ROOM_FLOOR_Y, 150, 120 }, { 1110, ROOM_FLOOR_Y, 70, 130 } };
static int s2ItemCount(int kind) {   // how many things a stage 2 room has to search
  switch (kind) {
    case LK_KITCHEN: return 8;
    case LK_CLEANING: return 16;
    case LK_MEDICAL: return 9;
    case LK_SECURITY: return 6;
    case LK_ADMIN: return 8;
    case LK_STORE: return 16;
    case LK_CLONE: return (int)(sizeof(cloneItems) / sizeof(cloneItems[0]));
    case LK_SOLDIER: return (int)(sizeof(soldierItems) / sizeof(soldierItems[0]));
    case LK_SERVER: return (int)(sizeof(serverItems) / sizeof(serverItems[0]));
    case LK_QLAB1: return (int)(sizeof(qlab1Items) / sizeof(qlab1Items[0]));
    case LK_QLAB2: return (int)(sizeof(qlab2Items) / sizeof(qlab2Items[0]));
    case LK_ARMORY: return (int)(sizeof(armoryItems) / sizeof(armoryItems[0]));
    case LK_TEST: return (int)(sizeof(testItems) / sizeof(testItems[0]));
    case LK_HAZARD: return (int)(sizeof(hazItems) / sizeof(hazItems[0]));
    case LK_ARMORY2: return (int)(sizeof(armoryItems) / sizeof(armoryItems[0]));
    case LK_SIMON: return 1;
    case LK_DESTRUCT: return (int)(sizeof(destructItems) / sizeof(destructItems[0]));
  }
  return 0;
}
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
  if (s2Kind[idx] == LK_CLONE) for (const LabItem &it : cloneItems) add(it);
  if (s2Kind[idx] == LK_SOLDIER) for (const LabItem &it : soldierItems) add(it);
  if (s2Kind[idx] == LK_SERVER) for (const LabItem &it : serverItems) add(it);
  if (s2Kind[idx] == LK_QLAB1) for (const LabItem &it : qlab1Items) add(it);
  if (s2Kind[idx] == LK_QLAB2) for (const LabItem &it : qlab2Items) add(it);
  if (s2Kind[idx] == LK_ARMORY || s2Kind[idx] == LK_ARMORY2) for (const LabItem &it : armoryItems) add(it);
  if (s2Kind[idx] == LK_TEST) for (const LabItem &it : testItems) add(it);
  if (s2Kind[idx] == LK_HAZARD) for (const LabItem &it : hazItems) add(it);
  if (s2Kind[idx] == LK_SIMON) for (const LabItem &it : simonItems) add(it);
  if (s2Kind[idx] == LK_DESTRUCT) for (const LabItem &it : destructItems) add(it);
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
static const WideRoom &roomTemplate(int idx);
static float cameraX(const RoomInfo &ri, bool right) {   // in from the corner, clear of any lift shaft
  const WideRoom &t = roomTemplate((int)(&ri - rooms));
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
  if (ri.hazard == HZ_CAMERA || ri.hazard == HZ_CAMDRONE) {
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
static void paintDeckSlab(int y, int x0 = 224, int x1 = SCREEN_W - 1) {
  fillRect(x0, y, x1, y + PLAT_T - 1, rgb565(196, 200, 208));
  fillRect(x0, y, x1, y + 2, rgb565(244, 246, 250));
  fillRect(x0, y + PLAT_T - 3, x1, y + PLAT_T - 1, rgb565(150, 156, 166));
  for (int x = x0 + 76; x < x1 - 90; x += 230) fillRect(x, y + PLAT_T + 1, x + 90, y + PLAT_T + 4, rgb565(250, 252, 255));   // lights under it
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
  paintDeckSlab(STORE_MID_Y, 0, 1059);
  paintBox(45, STORE_MID_Y, 64, 56, 1);
  paintBox(110, STORE_MID_Y, 64, 90, 0);
  paintSecurityLockers(218, STORE_MID_Y);
  paintRack(345, 495, STORE_MID_Y, 0);
  paintCrates(530, 140, STORE_MID_Y, 0);
  paintCupboardTall(760, STORE_MID_Y);
  paintBox(850, STORE_MID_Y, 70, 70, 2);
  paintBox(922, STORE_MID_Y, 66, 110, 1);
  // the top
  paintDeckSlab(STORE_TOP_Y, 0, 1059);
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

// ---- lift B's rooms: the clone lab and the super soldier lab (NUCLEAR MISSION's secret lab, wider), the server room
// NUCLEAR MISSION's pieces are drawn through a scale and a shift: its point (x, y) lands at (nmOX + x*nmS, nmOY + y*nmS)
static float nmS = 1, nmOX = 0, nmOY = 0;
static void nmAt(float s, float nx, float ny, float qx, float qy) { nmS = s; nmOX = qx - nx * s; nmOY = qy - ny * s; }
static int nX(float x) { return (int)lroundf(nmOX + x * nmS); }
static int nY(float y) { return (int)lroundf(nmOY + y * nmS); }
static void nRect(int x0, int y0, int x1, int y1, uint16_t c) { fillRect(nX((float)x0), nY((float)y0), nX(x1 + 1.0f) - 1, nY(y1 + 1.0f) - 1, c); }
static void nSpan(int y, int x0, int x1, uint16_t c) { nRect(x0, y, x1, y, c); }
static void nCirc(int cx, int cy, int r, uint16_t c) { fillCircle(nX((float)cx), nY((float)cy), r * nmS < 1 ? 1 : (int)lroundf(r * nmS), c); }
static void nRing(int cx, int cy, int r, uint16_t c) { drawCircle(nX((float)cx), nY((float)cy), (int)lroundf(r * nmS), c); }
static void nPoly(const int *xs, const int *ys, int n, uint16_t c) {
  float fx[12], fy[12];
  for (int i = 0; i < n && i < 12; i++) { fx[i] = nmOX + xs[i] * nmS; fy[i] = nmOY + ys[i] * nmS; }
  fillPolygonF(fx, fy, n, c);
}
static void nLine(int x0, int y0, int x1, int y1, uint16_t c, bool thick) {
  drawLineF(nmOX + x0 * nmS, nmOY + y0 * nmS, nmOX + x1 * nmS, nmOY + y1 * nmS, c, (thick ? 2.0f : 1.0f) * nmS);
}
static void nText(const char *t, int x, int y, float size, uint16_t c, bool thick) { drawVectorText(t, nmOX + x * nmS, nmOY + y * nmS, size * nmS, c, thick); }
static void nOutline(int x0, int y0, int x1, int y1, uint16_t c, int w) { drawRectOutline(nX((float)x0), nY((float)y0), nX((float)x1), nY((float)y1), c, (int)lroundf(w * nmS)); }

static void nBioSign(int x, int y, int r) {   // a yellow triangle with the trefoil in it
  int tx[3] = { x, x + r, x - r }, ty[3] = { y - r, y + r * 3 / 4, y + r * 3 / 4 };
  nPoly(tx, ty, 3, rgb565(240, 200, 40));
  int cy = y + r / 6, k = r / 4;
  for (int i = 0; i < 3; i++) {
    float a = -PI / 2 + i * 2 * PI / 3;
    nCirc(x + (int)(cosf(a) * k), cy + (int)(sinf(a) * k), k, rgb565(20, 20, 22));
  }
  nCirc(x, cy, k / 2 + 1, rgb565(240, 200, 40));
}

// A colour seen through the tank's yellow-green fluid
static uint16_t inTank(int r, int g, int b) { return rgb565(r + (160 - r) * 3 / 10, g + (205 - g) * 3 / 10, b + (60 - b) * 3 / 10); }

// One clone floating in a tank, eyes shut: a man in a dark suit, white shirt and tie (no one in
// particular). top: the top of his head; ~100 tall. variant moves the arms a little.
static void nClone(int cx, int top, int variant) {
  uint16_t skin = inTank(222, 170, 130), skinD = inTank(180, 128, 96), hair = inTank(96, 66, 40), hairD = inTank(64, 42, 26);
  uint16_t suit = inTank(40, 46, 66), suitD = inTank(26, 30, 44), shirt = inTank(236, 238, 240), tie = inTank(40, 80, 150);
  uint16_t shoe = inTank(24, 22, 22), line = inTank(60, 40, 30);
  if (variant >= 3) { hair = inTank(40, 34, 30); tie = inTank(150, 40, 40); }   // (the extra ones: dark hair, a red tie)
  int f = top, v = variant % 3;
  nRect(cx - 13, f + 62, cx - 2, f + 95, suit);
  nRect(cx + 2, f + 62, cx + 13, f + 95, suit);
  nRect(cx - 2, f + 64, cx - 1, f + 95, suitD);
  nRect(cx + 11, f + 66, cx + 13, f + 95, suitD);
  nRect(cx - 15, f + 95, cx - 1, f + 100, shoe);
  nRect(cx + 1, f + 95, cx + 15, f + 100, shoe);
  int sp = 2 + v * 2;
  int lx[4] = { cx - 22, cx - 14, cx - 15 - sp, cx - 24 - sp }, ly[4] = { f + 30, f + 32, f + 66, f + 64 };
  int rx[4] = { cx + 22, cx + 14, cx + 15 + sp, cx + 24 + sp }, ry[4] = { f + 30, f + 32, f + 66, f + 64 };
  nPoly(lx, ly, 4, suit);
  nPoly(rx, ry, 4, suit);
  nRect(cx - 23 - sp, f + 63, cx - 16 - sp, f + 65, shirt);
  nRect(cx + 16 + sp, f + 63, cx + 23 + sp, f + 65, shirt);
  nCirc(cx - 20 - sp, f + 70, 4, skin);
  nCirc(cx + 20 + sp, f + 70, 4, skin);
  int jx[6] = { cx - 22, cx + 22, cx + 18, cx + 16, cx - 16, cx - 18 }, jy[6] = { f + 28, f + 28, f + 50, f + 64, f + 64, f + 50 };
  nPoly(jx, jy, 6, suit);
  nRect(cx - 22, f + 28, cx - 18, f + 50, suitD);
  int sx[3] = { cx - 7, cx + 7, cx }, sy[3] = { f + 26, f + 26, f + 48 };
  nPoly(sx, sy, 3, shirt);
  int tx[5] = { cx - 2, cx + 2, cx + 3, cx, cx - 3 }, ty[5] = { f + 28, f + 28, f + 44, f + 48, f + 44 };
  nPoly(tx, ty, 5, tie);
  nRect(cx - 2, f + 26, cx + 2, f + 29, tie);
  nLine(cx - 7, f + 27, cx - 1, f + 50, suitD, false);
  nLine(cx + 7, f + 27, cx + 1, f + 50, suitD, false);
  nLine(cx - 7, f + 27, cx - 11, f + 36, suitD, false);
  nLine(cx + 7, f + 27, cx + 11, f + 36, suitD, false);
  nCirc(cx + 2, f + 54, 1, suitD);
  nCirc(cx + 2, f + 59, 1, suitD);
  nRect(cx + 9, f + 36, cx + 14, f + 37, shirt);
  nRect(cx - 4, f + 20, cx + 4, f + 27, skinD);
  nCirc(cx, f + 10, 10, hair);
  nCirc(cx - 9, f + 13, 2, skin);
  nCirc(cx + 9, f + 13, 2, skin);
  nCirc(cx, f + 13, 8, skin);
  nRect(cx - 8, f + 13, cx + 8, f + 18, skin);
  nCirc(cx, f + 18, 6, skin);
  int px[4] = { cx - 10, cx + 9, cx + 7, cx - 9 }, py[4] = { f + 2, f + 2, f + 7, f + 8 };
  nPoly(px, py, 4, hair);
  nLine(cx - 3, f + 1, cx - 5, f + 6, hairD, false);
  nRect(cx - 6, f + 10, cx - 2, f + 10, hairD);
  nRect(cx + 2, f + 10, cx + 6, f + 10, hairD);
  nRect(cx - 6, f + 13, cx - 2, f + 13, line);
  nRect(cx + 2, f + 13, cx + 6, f + 13, line);
  nRect(cx, f + 14, cx + 1, f + 18, skinD);
  nRect(cx - 3, f + 21, cx + 3, f + 21, line);
}

// ---- the EXPERIMENTAL CLONE LAB: five cloning tanks up on the deck, the machines that feed them below
#define CLONE_TANKS    5
#define CLONE_TANK_S   1.25f
static const int16_t cloneTankX[CLONE_TANKS] = { 330, 520, 710, 900, 1090 };

static void paintCloneTank(int qx, int k) {   // its base on the deck (NUCLEAR MISSION's, a quarter bigger)
  nmAt(CLONE_TANK_S, 0, 400, (float)qx, CLONE_DECK_Y);
  int cx = 0, x0 = cx - 42, x1 = cx + 42;
  fillRect(qx - 8, CEIL_Y + 58, qx + 8, nY(256), rgb565(110, 116, 126));   // the feed from the ceiling
  fillRect(qx - 8, CEIL_Y + 58, qx - 5, nY(256), rgb565(150, 156, 166));
  nRect(x0 - 6, 256, x1 + 6, 270, rgb565(150, 156, 166));
  nRect(x0 - 6, 256, x1 + 6, 258, rgb565(200, 206, 214));
  for (int y = 270; y < 384; y++) {
    int g = (y - 270) * 40 / 114;
    nSpan(y, x0, x1, rgb565(150 + g / 2, 190 + g / 2, 40 + g / 3));
  }
  nClone(cx, 278, k);
  nLine(cx + 8, 278, cx + 14, 270, rgb565(70, 90, 60), false);
  for (int y = 270; y < 384; y += 2) nSpan(y, x0, x0 + 6, rgb565(206, 236, 140));
  nLine(x1 - 14, 380, x1 - 4, 280, rgb565(220, 240, 170), false);
  nRect(x0 - 4, 270, x0 - 1, 384, rgb565(170, 176, 186));
  nRect(x1 + 1, 270, x1 + 4, 384, rgb565(170, 176, 186));
  nRect(x0 - 8, 384, x1 + 8, 399, rgb565(80, 86, 96));
  nRect(x0 - 8, 384, x1 + 8, 386, rgb565(130, 138, 150));
  char b[12];
  snprintf(b, sizeof(b), "SUBJECT %02d", k + 1);
  nRect(cx - 34, 388, cx + 34, 397, rgb565(20, 24, 30));
  nText(b, cx, 392, 0.9f, rgb565(240, 200, 60), true);
}

static void paintCloneLab() {
  // the sign up top, between the tanks' feeds
  nmAt(1.2f, 360, 92, 640, 128);
  for (int k = 0; k < CLONE_TANKS; k++) paintCloneTank(cloneTankX[k], k);
  nmAt(1.2f, 360, 92, 640, 128);
  nRect(150, 92, 570, 150, rgb565(10, 16, 12));
  nOutline(150, 92, 570, 150, rgb565(60, 220, 120), 2);
  nText("EXPERIMENTAL", 360, 110, 2.6f, rgb565(60, 220, 120), true);
  nText("CLONE LAB", 360, 136, 2.6f, rgb565(230, 240, 235), true);
  nBioSign(22, 121, 22);     // (out past the tanks' feeds)
  nBioSign(814, 121, 22);
  // the deck: its slab, a rail in front of the tanks, a yellow and black edge
  paintDeckSlab(CLONE_DECK_Y);
  for (int x : { 224, 425, 615, 805, 995, 1185, SCREEN_W - 20 }) fillRect(x, CLONE_DECK_Y - 62, x + 3, CLONE_DECK_Y - 1, rgb565(150, 156, 166));   // a rail in front (posts between the tanks, a top rail)
  fillRect(224, CLONE_DECK_Y - 64, SCREEN_W - 17, CLONE_DECK_Y - 59, rgb565(222, 226, 232));
  // under the deck: the hot gas duct, a nozzle over each vent (and its lamp, lit by drawGas)
  {
    int y0 = CLONE_DECK_Y + PLAT_T + 2, y1 = y0 + 24;
    static const uint8_t shade[24] = { 120, 150, 176, 196, 210, 218, 222, 220, 214, 206, 196, 186, 176, 166, 156, 146, 136, 126, 116, 106, 96, 86, 76, 66 };
    for (int y = y0; y < y1; y++) { int v = shade[y - y0]; fillRect(224, y, SCREEN_W - 17, y, rgb565(v - 10, v - 4, v + 4)); }
    for (int x = 270; x < SCREEN_W - 30; x += 110) {                   // its flanges, bolted
      fillRect(x, y0 - 3, x + 8, y1 + 2, rgb565(130, 136, 146));
      fillRect(x, y0 - 3, x + 2, y1 + 2, rgb565(200, 206, 214));
      fillCircle(x + 4, y0 + 2, 2, rgb565(70, 74, 82));
      fillCircle(x + 4, y1 - 3, 2, rgb565(70, 74, 82));
    }
    fillRect(224, y0, 232, y1, rgb565(90, 96, 106));                    // (its end cap)
    drawVectorText("HOT GAS", 640, y0 + 12, 1.2f, rgb565(200, 60, 40), true);
    for (int v = 0; v < GAS_VENTS; v++) {                                // the nozzles
      int x = gasX[v];
      int px[4] = { x - 16, x + 16, x + 10, x - 10 }, py[4] = { y1, y1, y1 + 18, y1 + 18 };
      fillPolygon(px, py, 4, rgb565(110, 116, 126));
      fillRect(x - 10, y1 + 16, x + 10, y1 + 18, rgb565(40, 40, 44));
      fillRect(x + 14, y1, x + 28, y1 + 10, rgb565(70, 74, 82));          // (the lamp's housing)
      for (int y = y1 + 22; y < ROOM_FLOOR_Y - 2; y += 18)                // scorched streaks down the wall
        fillRect(x - 3, y, x + 3, y + 8, rgb565(214, 210, 200));
    }
  }
  // the bench: a test tube rack (NUCLEAR MISSION's, a fifth bigger)
  nmAt(1.2f, 98, 560, 340, ROOM_FLOOR_Y);
  nRect(28, 500, 168, 506, rgb565(170, 176, 186));
  nRect(34, 507, 40, 559, rgb565(110, 116, 126));
  nRect(156, 507, 162, 559, rgb565(110, 116, 126));
  nRect(38, 478, 100, 484, rgb565(200, 204, 212));
  nRect(38, 492, 100, 499, rgb565(200, 204, 212));
  const uint16_t fluid[5] = { rgb565(120, 230, 70), rgb565(230, 60, 60), rgb565(80, 170, 255), rgb565(120, 230, 70), rgb565(240, 200, 60) };
  for (int k = 0; k < 5; k++) {
    int tx = 44 + k * 12;
    nRect(tx, 466, tx + 6, 498, rgb565(210, 228, 236));
    nRect(tx + 1, 482, tx + 5, 497, fluid[k]);
    nCirc(tx + 3, 497, 3, fluid[k]);
  }
  for (int k = 0; k < 3; k++) {                                        // (more: flasks at the other end)
    int fx = 120 + k * 16;
    int xs[4] = { fx - 2, fx + 2, fx + 7, fx - 7 }, ys[4] = { 476, 476, 499, 499 };
    nPoly(xs, ys, 4, rgb565(210, 228, 236));
    nRect(fx - 6, 490, fx + 6, 498, fluid[(k + 2) % 5]);
  }
  // the machine in the middle: a green tank, a biohazard sign, two monitors
  nmAt(1.2f, 270, 560, 600, ROOM_FLOOR_Y);
  int mx = 270;
  nRect(mx - 46, 534, mx + 46, 559, rgb565(80, 86, 96));
  nRect(mx - 40, 440, mx + 40, 534, rgb565(120, 128, 140));
  nRect(mx - 40, 440, mx + 40, 446, rgb565(170, 176, 186));
  nRect(mx - 30, 452, mx + 30, 510, rgb565(30, 120, 40));
  for (int y = 452; y < 510; y++) nSpan(y, mx - 28, mx - 18, rgb565(80, 200, 90));
  nBioSign(mx, 526, 12);
  fillRect(596, CLONE_DECK_Y + PLAT_T, 604, nY(440), rgb565(90, 96, 106));   // its feed pipe, up to the deck
  nRect(174, 444, 220, 486, rgb565(30, 34, 40));
  nRect(178, 448, 216, 482, rgb565(6, 20, 14));
  for (int k = 0; k < 16; k++) {
    float a = k * 0.55f;
    int yy = 450 + k * 2;
    nCirc(197 + (int)(12 * sinf(a)), yy, 1, rgb565(80, 240, 140));
    nCirc(197 - (int)(12 * sinf(a)), yy, 1, rgb565(80, 170, 255));
  }
  nRect(320, 444, 366, 486, rgb565(30, 34, 40));
  nRect(324, 448, 362, 482, rgb565(6, 20, 14));
  nCirc(343, 455, 4, rgb565(80, 240, 140));
  nRect(339, 460, 347, 472, rgb565(80, 240, 140));
  nRect(334, 461, 337, 471, rgb565(80, 240, 140));
  nRect(349, 461, 352, 471, rgb565(80, 240, 140));
  nRect(339, 473, 342, 481, rgb565(80, 240, 140));
  nRect(344, 473, 347, 481, rgb565(80, 240, 140));
  nRect(195, 487, 199, 559, rgb565(110, 116, 126));                   // (their stands)
  nRect(341, 487, 345, 559, rgb565(110, 116, 126));
  nRect(185, 554, 209, 559, rgb565(110, 116, 126));
  nRect(331, 554, 355, 559, rgb565(110, 116, 126));
  nText("DNA", 197, 438, 1.2f, rgb565(80, 240, 140), true);
  nText("SCAN", 343, 438, 1.2f, rgb565(80, 240, 140), true);
  // gas cylinders, biohazard on each
  nmAt(1.2f, 434, 560, 820, ROOM_FLOOR_Y);
  for (int k = 0; k < 3; k++) {
    int gx = 398 + k * 36;
    nRect(gx - 12, 466, gx + 12, 559, rgb565(52, 120, 70));
    nCirc(gx, 466, 12, rgb565(52, 120, 70));
    nRect(gx - 12, 470, gx - 8, 559, rgb565(90, 160, 110));
    nRect(gx - 4, 446, gx + 4, 456, rgb565(150, 156, 166));
    nRect(gx - 8, 446, gx + 8, 449, rgb565(190, 196, 206));
    nBioSign(gx, 506, 9);
  }
  // the gurney: a clone under a sheet, his feet out, a tag on his toe
  {
    int gx0 = 900, gx1 = 1050, top = ROOM_FLOOR_Y - 66;
    fillRect(gx0 + 10, top + 10, gx0 + 15, ROOM_FLOOR_Y - 10, rgb565(150, 156, 166));
    fillRect(gx1 - 15, top + 10, gx1 - 10, ROOM_FLOOR_Y - 10, rgb565(150, 156, 166));
    fillRect(gx0 + 10, ROOM_FLOOR_Y - 34, gx1 - 10, ROOM_FLOOR_Y - 30, rgb565(150, 156, 166));
    for (int x : { gx0 + 12, gx1 - 12 }) fillCircle(x, ROOM_FLOOR_Y - 6, 6, rgb565(40, 42, 48));
    fillRect(gx0, top, gx1, top + 10, rgb565(190, 196, 206));
    int sx[8] = { gx0 + 4, gx0 + 10, gx0 + 34, gx0 + 50, gx0 + 100, gx0 + 128, gx1 - 10, gx1 - 4 };
    int sy[8] = { top + 2, top - 20, top - 24, top - 16, top - 18, top - 14, top - 12, top + 2 };
    fillPolygon(sx, sy, 8, rgb565(236, 240, 244));                     // the sheet, his shape under it
    fillCircle(gx0 + 22, top - 14, 14, rgb565(236, 240, 244));         // (his head)
    drawLine(gx0 + 50, top - 14, gx0 + 100, top - 6, rgb565(200, 206, 214), false);
    fillRect(gx1 - 4, top - 14, gx1 + 6, top, rgb565(222, 170, 130));  // his feet
    fillRect(gx1 + 4, top - 10, gx1 + 14, top - 2, rgb565(240, 220, 120));
    drawVectorText("06", gx1 + 9, top - 6, 0.7f, rgb565(60, 50, 30), false);
  }
  // the robot arm (NUCLEAR MISSION's size: it reaches up under the deck)
  nmAt(1.0f, 640, 560, 1150, ROOM_FLOOR_Y);
  {
    int rx = 640;
    nRect(rx - 26, 542, rx + 26, 559, rgb565(60, 64, 72));
    nRect(rx - 12, 520, rx + 12, 542, rgb565(236, 140, 30));
    int ax[4] = { rx - 8, rx + 8, rx - 22, rx - 38 }, ay[4] = { 520, 520, 420, 420 };
    nPoly(ax, ay, 4, rgb565(236, 140, 30));
    nCirc(rx - 30, 420, 11, rgb565(60, 64, 72));
    int bx[4] = { rx - 36, rx - 24, rx + 14, rx + 4 }, by[4] = { 414, 426, 376, 368 };
    nPoly(bx, by, 4, rgb565(236, 140, 30));
    nCirc(rx + 10, 372, 8, rgb565(60, 64, 72));
    nRect(rx + 14, 362, rx + 30, 368, rgb565(170, 176, 186));
    nRect(rx + 14, 376, rx + 30, 382, rgb565(170, 176, 186));
  }
}

// ---- the SUPER SOLDIER LAB: the big tank in the middle, two side tanks, consoles, the desk and the syringe cabinet
#define SOLDIER_S 1.3f

static void nSideTank(int cx, int pic) {   // a green tank on a base, with a picture in it (0 DNA, 1 a molecule)
  nRect(cx - 34, 470, cx + 34, 559, rgb565(110, 116, 126));
  nRect(cx - 34, 470, cx + 34, 474, rgb565(150, 156, 166));
  nRect(cx - 24, 490, cx + 24, 530, rgb565(90, 96, 106));
  nRect(cx - 30, 312, cx + 30, 326, rgb565(150, 156, 166));
  nRect(cx - 8, 300, cx + 8, 312, rgb565(220, 170, 40));
  for (int y = 326; y < 470; y++) {
    int g = (y - 326) * 30 / 144;
    nSpan(y, cx - 26, cx + 26, rgb565(40 + g, 200 + g, 60));
  }
  nRect(cx - 26, 326, cx - 20, 469, rgb565(150, 250, 140));
  uint16_t ic = rgb565(170, 255, 150);
  if (pic == 0) {
    for (int k = 0; k < 14; k++) {
      float a = k * 0.6f;
      int y = 356 + k * 6;
      nCirc(cx + (int)(10 * sinf(a)), y, 2, ic);
      nCirc(cx - (int)(10 * sinf(a)), y, 2, ic);
      if (k % 2 == 0) nLine(cx + (int)(10 * sinf(a)), y, cx - (int)(10 * sinf(a)), y, ic, false);
    }
  } else {
    int px[6], py[6];
    for (int k = 0; k < 6; k++) { px[k] = cx + (int)(14 * cosf(k * 1.047f)); py[k] = 398 + (int)(14 * sinf(k * 1.047f)); }
    for (int k = 0; k < 6; k++) {
      nLine(px[k], py[k], px[(k + 1) % 6], py[(k + 1) % 6], ic, true);
      nCirc(px[k], py[k], 3, ic);
      int ox = cx + (int)(24 * cosf(k * 1.047f)), oy = 398 + (int)(24 * sinf(k * 1.047f));
      if (k % 2 == 0) { nLine(px[k], py[k], ox, oy, ic, false); nCirc(ox, oy, 3, ic); }
    }
  }
  for (int k = 0; k < 4; k++) nRing(cx - 12 + k * 8, 450 - k * 28, 2, rgb565(220, 255, 200));
}

static void nLabConsole(int cx, int pic) {   // a small console with a screen (0 a network, 1 DNA) and three buttons
  nRect(cx - 22, 440, cx + 22, 559, rgb565(130, 136, 146));
  nRect(cx - 22, 440, cx + 22, 443, rgb565(176, 182, 192));
  nRect(cx - 17, 448, cx + 17, 478, rgb565(10, 20, 36));
  uint16_t c = rgb565(80, 170, 255);
  if (pic == 0) {
    nCirc(cx - 8, 456, 3, c); nCirc(cx + 8, 456, 3, c); nCirc(cx - 8, 470, 3, c); nCirc(cx + 8, 470, 3, c);
    nLine(cx - 8, 456, cx + 8, 470, c, false); nLine(cx + 8, 456, cx - 8, 470, c, false);
  } else {
    for (int k = 0; k < 6; k++) { int y = 452 + k * 4; int w = (int)(8 * sinf(k * 0.9f)); nRect(cx - abs(w), y, cx + abs(w), y + 1, c); }
  }
  nRect(cx - 14, 486, cx - 8, 492, rgb565(240, 200, 40));
  nRect(cx - 3, 486, cx + 3, 492, rgb565(220, 50, 40));
  nRect(cx + 8, 486, cx + 14, 492, rgb565(220, 50, 40));
}

static void paintSoldierLab() {
  // red lamps high on the walls
  for (int lx : { 40, SCREEN_W - 62 }) {
    fillRect(lx, 330, lx + 22, 342, rgb565(90, 20, 20));
    fillRect(lx + 2, 332, lx + 20, 340, rgb565(240, 40, 40));
  }
  // pipes behind, down from the ceiling (and two more for the wider room)
  for (int px : { 479, 531, 749, 801, 1048, 232 }) {
    fillRect(px - 8, CEIL_Y + 58, px + 8, ROOM_FLOOR_Y - 1, rgb565(150, 156, 166));
    fillRect(px - 8, CEIL_Y + 58, px - 4, ROOM_FLOOR_Y - 1, rgb565(196, 202, 210));
    fillRect(px + 5, CEIL_Y + 58, px + 8, ROOM_FLOOR_Y - 1, rgb565(110, 116, 126));
    fillRect(px - 10, 440, px + 10, 448, rgb565(200, 160, 40));
  }
  // vitals screens on the wall over the consoles (their traces move)
  for (int sx : { 505, 775 }) {
    fillRect(sx - 52, 408, sx + 52, 486, rgb565(60, 64, 72));
    fillRect(sx - 48, 412, sx + 48, 482, rgb565(6, 16, 12));
    drawVectorText("VITALS", sx, 420, 1.1f, rgb565(80, 240, 140), false);
  }
  // the side tanks (a third bigger) and consoles
  nmAt(SOLDIER_S, 0, 560, 405, ROOM_FLOOR_Y);
  nSideTank(0, 0);
  nmAt(SOLDIER_S, 0, 560, 875, ROOM_FLOOR_Y);
  nSideTank(0, 1);
  nmAt(1.0f, 0, 560, 505, ROOM_FLOOR_Y);
  nLabConsole(0, 0);
  nmAt(1.0f, 0, 560, 775, ROOM_FLOOR_Y);
  nLabConsole(0, 1);
  // the big tank: a striped platform, steps, a glowing cylinder with the soldier in it
  nmAt(SOLDIER_S, 0, 560, 640, ROOM_FLOOR_Y);
  {
    int cx = 0;
    nRect(cx - 78, 524, cx + 78, 559, rgb565(60, 64, 72));
    for (int x = cx - 78; x < cx + 78; x += 18) {
      nRect(x, 530, x + 17, 542, rgb565(20, 20, 22));
      int xs[4] = { x, x + 9, x + 14, x + 5 }, ys[4] = { 530, 530, 543, 543 };
      nPoly(xs, ys, 4, rgb565(236, 196, 40));
    }
    nRect(cx - 20, 544, cx + 20, 559, rgb565(90, 96, 106));
    nRect(cx - 62, 516, cx + 62, 524, rgb565(150, 156, 166));
    nRect(cx - 70, 266, cx + 70, 290, rgb565(120, 128, 140));
    nRect(cx - 70, 266, cx + 70, 270, rgb565(170, 176, 186));
    nRect(cx - 40, 286, cx + 40, 290, rgb565(220, 255, 170));
    fillRect(640 - 14, CEIL_Y + 58, 640 + 14, nY(266), rgb565(110, 116, 126));   // its feed from the ceiling
    for (int y = 290; y < 516; y++) {
      int g = (y - 290) * 40 / 226;
      nSpan(y, cx - 54, cx + 54, rgb565(150 + g / 2, 220 + g / 2, 60 + g / 3));
    }
    uint16_t sh = rgb565(70, 120, 40);
    nCirc(cx, 336, 13, sh);
    nRect(cx - 6, 346, cx + 6, 356, sh);
    int tx[6] = { cx - 34, cx + 34, cx + 26, cx + 16, cx - 16, cx - 26 }, ty[6] = { 356, 356, 392, 430, 430, 392 };
    nPoly(tx, ty, 6, sh);
    int la[4] = { cx - 34, cx - 24, cx - 30, cx - 44 }, lay[4] = { 358, 366, 432, 428 };
    int ra[4] = { cx + 34, cx + 24, cx + 30, cx + 44 }, ray[4] = { 358, 366, 432, 428 };
    nPoly(la, lay, 4, sh);
    nPoly(ra, ray, 4, sh);
    nCirc(cx - 36, 436, 6, sh);
    nCirc(cx + 36, 436, 6, sh);
    int lg[4] = { cx - 17, cx - 2, cx - 6, cx - 22 }, lgy[4] = { 428, 428, 506, 506 };
    int rg[4] = { cx + 17, cx + 2, cx + 6, cx + 22 }, rgy[4] = { 428, 428, 506, 506 };
    nPoly(lg, lgy, 4, sh);
    nPoly(rg, rgy, 4, sh);
    for (int y = 290; y < 516; y += 2) nSpan(y, cx - 54, cx - 46, rgb565(210, 250, 160));
    nRect(cx - 58, 290, cx - 55, 515, rgb565(170, 176, 186));
    nRect(cx + 55, 290, cx + 58, 515, rgb565(170, 176, 186));
    nRect(cx - 30, 500, cx + 30, 512, rgb565(20, 24, 30));
    nText("SUBJECT X", cx, 506, 0.9f, rgb565(240, 200, 60), true);
  }
  // the desk on the left: a body-scan monitor, test tubes, a syringe; a stool
  nmAt(1.0f, 0, 560, 280, ROOM_FLOOR_Y);
  {
    int dx = 0;
    nRect(dx - 44, 360, dx + 44, 420, rgb565(150, 156, 166));
    nRect(dx - 40, 364, dx + 40, 416, rgb565(10, 24, 44));
    nCirc(dx - 18, 376, 5, rgb565(90, 210, 255));
    nRect(dx - 23, 382, dx - 13, 400, rgb565(90, 210, 255));
    nRect(dx - 23, 400, dx - 19, 412, rgb565(90, 210, 255));
    nRect(dx - 17, 400, dx - 13, 412, rgb565(90, 210, 255));
    for (int k = 0; k < 4; k++) nRect(dx + 4 + k * 8, 408 - k * 6, dx + 9 + k * 8, 412, rgb565(90, 210, 255));
    nRect(dx - 4, 421, dx + 4, 469, rgb565(110, 116, 126));
    nRect(dx - 46, 470, dx + 46, 476, rgb565(170, 176, 186));
    nRect(dx - 40, 477, dx - 36, 559, rgb565(110, 116, 126));
    nRect(dx + 36, 477, dx + 40, 559, rgb565(110, 116, 126));
    for (int k = 0; k < 3; k++) {
      nRect(dx - 38 + k * 9, 448, dx - 33 + k * 9, 469, rgb565(210, 228, 236));
      nRect(dx - 37 + k * 9, 456, dx - 34 + k * 9, 468, rgb565(90, 230, 80));
    }
    nRect(dx - 6, 462, dx + 30, 467, rgb565(210, 228, 236));
    nRect(dx - 4, 463, dx + 22, 466, rgb565(90, 230, 80));
    nRect(dx + 30, 464, dx + 40, 465, rgb565(180, 186, 196));
    nRect(dx - 10, 461, dx - 6, 468, rgb565(220, 224, 230));
    nCirc(dx + 24, 512, 12, rgb565(30, 32, 36));
    nRect(dx + 22, 518, dx + 26, 548, rgb565(80, 86, 96));
    nRect(dx + 10, 548, dx + 38, 551, rgb565(80, 86, 96));
  }
  // the cabinet on the right, a rack of syringes over it
  nmAt(1.0f, 0, 560, 995, ROOM_FLOOR_Y);
  {
    int kx = 0;
    nRect(kx - 40, 480, kx + 40, 559, rgb565(150, 156, 166));
    nRect(kx - 40, 480, kx + 40, 486, rgb565(190, 196, 206));
    for (int r = 0; r < 2; r++)
      for (int c = 0; c < 2; c++) {
        int x0 = kx - 36 + c * 38, y0 = 492 + r * 30;
        nOutline(x0, y0, x0 + 34, y0 + 26, rgb565(110, 116, 126), 1);
        nRect(x0 + 12, y0 + 11, x0 + 22, y0 + 14, rgb565(90, 96, 106));
      }
    nRect(kx - 38, 400, kx + 38, 404, rgb565(170, 176, 186));
    nRect(kx - 38, 466, kx + 38, 470, rgb565(170, 176, 186));
    for (int k = 0; k < 4; k++) {
      int sx = kx - 30 + k * 19;
      nRect(sx, 404, sx + 8, 412, rgb565(110, 116, 126));
      nRect(sx + 1, 412, sx + 7, 452, rgb565(220, 236, 240));
      nRect(sx + 2, 430, sx + 6, 451, rgb565(90, 230, 80));
      nRect(sx + 3, 452, sx + 5, 466, rgb565(180, 186, 196));
    }
  }
  // (new, for the wider room) the serum fridge: a glass door, rows of green vials
  {
    int x0 = 128, x1 = 212, top = ROOM_FLOOR_Y - 200;
    fillRect(x0, top, x1, ROOM_FLOOR_Y - 1, rgb565(176, 182, 192));
    fillRect(x0, top, x1, top + 3, rgb565(220, 224, 230));
    fillRect(x0 + 8, top + 26, x1 - 8, ROOM_FLOOR_Y - 20, rgb565(150, 200, 214));
    for (int r = 0; r < 4; r++) {
      int sy = top + 64 + r * 38;
      fillRect(x0 + 8, sy, x1 - 8, sy + 2, rgb565(220, 230, 236));
      for (int k = 0; k < 6; k++) {
        fillRect(x0 + 14 + k * 10, sy - 20, x0 + 19 + k * 10, sy - 1, rgb565(210, 228, 236));
        fillRect(x0 + 15 + k * 10, sy - 12, x0 + 18 + k * 10, sy - 1, (k + r) % 4 ? rgb565(90, 230, 80) : rgb565(240, 200, 60));
      }
    }
    drawLine(x0 + 16, ROOM_FLOOR_Y - 30, x0 + 50, top + 34, rgb565(206, 236, 246), false);
    fillRect(x1 - 6, top + 70, x1 - 3, top + 120, rgb565(90, 96, 106));
    fillRect(x0 + 10, top + 6, x1 - 10, top + 20, rgb565(20, 24, 30));
    drawVectorText("SERUM", (x0 + x1) / 2, top + 13, 1.0f, rgb565(90, 230, 80), true);
  }
  // (new) the armour stand: a soldier's suit of armour on a stand, a helmet on top
  {
    int cx = 1110, fy = ROOM_FLOOR_Y;
    uint16_t plate = rgb565(70, 86, 60), plateL = rgb565(100, 120, 84), dark = rgb565(40, 46, 36);
    fillRect(cx - 34, fy - 10, cx + 34, fy - 1, rgb565(80, 86, 96));
    fillRect(cx - 4, fy - 80, cx + 4, fy - 10, rgb565(130, 136, 146));
    int bx[6] = { cx - 34, cx + 34, cx + 28, cx + 20, cx - 20, cx - 28 }, by[6] = { fy - 176, fy - 176, fy - 120, fy - 84, fy - 84, fy - 120 };
    fillPolygon(bx, by, 6, plate);
    fillRect(cx - 34, fy - 176, cx - 22, fy - 150, plateL);                   // the shoulder plates
    fillRect(cx + 22, fy - 176, cx + 34, fy - 150, plateL);
    for (int k = 0; k < 3; k++) fillRect(cx - 18, fy - 140 + k * 16, cx + 18, fy - 136 + k * 16, dark);
    fillCircle(cx, fy - 194, 16, plate);                                       // the helmet
    fillRect(cx - 16, fy - 194, cx + 16, fy - 182, plate);
    fillRect(cx - 12, fy - 196, cx + 12, fy - 188, rgb565(30, 60, 90));       // its visor
    fillRect(cx - 10, fy - 195, cx - 2, fy - 193, rgb565(110, 170, 220));
  }
  // the sign up top (over the pipes): SUPER SOLDIER LAB, a star, yellow and black
  nmAt(1.2f, 360, 88, 640, 150);
  nRect(140, 88, 580, 150, rgb565(16, 18, 22));
  nOutline(140, 88, 580, 150, rgb565(110, 116, 126), 3);
  nText("SUPER SOLDIER LAB", 360, 112, 3.0f, rgb565(240, 196, 40), true);
  for (int k = 0; k < 3; k++) {
    nRect(160, 104 + k * 6, 190 - k * 6, 107 + k * 6, rgb565(240, 196, 40));
    nRect(530 + k * 6, 104 + k * 6, 560, 107 + k * 6, rgb565(240, 196, 40));
  }
  {
    int sx[10], sy[10];
    for (int k = 0; k < 10; k++) { float a = -HALF_PI + k * PI / 5; float r = (k & 1) ? 4.5f : 10; sx[k] = 360 + (int)(r * cosf(a)); sy[k] = 138 + (int)(r * sinf(a)); }
    nPoly(sx, sy, 10, rgb565(240, 196, 40));
  }
  for (int x = 200; x < 520; x += 20) {
    if (x > 330 && x < 390) continue;
    int xs[4] = { x, x + 10, x + 14, x + 4 }, ys[4] = { 132, 132, 145, 145 };
    nPoly(xs, ys, 4, rgb565(240, 196, 40));
  }
}

// ---- the SERVER ROOM: cooling fans along the top, a big screen, four terminals, metal cupboards; silver walls
#define SRV_FANS 7
static int srvFanX(int k) { return 265 + k * 125; }
#define SRV_FAN_Y 182
#define SRV_FAN_R 50
#define SRV_MON_X0 300
#define SRV_MON_X1 980
#define SRV_MON_Y0 262
#define SRV_MON_Y1 456

static void serverWall(int x0, int x1, int y0, int y1) {   // shiny silver metal tiles
  const int T = 80;
  for (int ty = y0; ty < y1; ty += T)
    for (int tx = x0; tx < x1; tx += T) {
      int bx1 = tx + T - 1 < x1 ? tx + T - 1 : x1, by1 = ty + T - 1 < y1 ? ty + T - 1 : y1;
      int alt = ((tx / T) + (ty / T)) & 1;
      for (int y = ty; y <= by1; y++) {                    // brushed: lighter at the top, a little darker down
        int v = 206 - (y - ty) * 34 / T + alt * 6;
        fillRect(tx, y, bx1, y, rgb565(v - 6, v, v + 8));
      }
      for (int k = 0; k < 3; k++) {                        // a diagonal shine across it
        int sx0 = tx + 10 + k * 7;
        drawLine(sx0, by1 - 4, (sx0 + 40 < bx1 ? sx0 + 40 : bx1), (by1 - 44 > ty ? by1 - 44 : ty), rgb565(236, 240, 246), k == 1);
      }
      fillRect(tx, ty, bx1, ty, rgb565(246, 248, 252));    // the bevel: light top and left, dark bottom and right
      fillRect(tx, ty, tx, by1, rgb565(236, 240, 246));
      fillRect(tx, by1, bx1, by1, rgb565(110, 116, 128));
      fillRect(bx1, ty, bx1, by1, rgb565(126, 132, 144));
      for (int c = 0; c < 4; c++) {                        // the screws in its corners
        int sx = (c & 1) ? bx1 - 6 : tx + 6, sy = (c & 2) ? by1 - 6 : ty + 6;
        fillCircle(sx, sy, 2, rgb565(130, 136, 148));
        fillRect(sx - 1, sy, sx + 1, sy, rgb565(220, 224, 232));
      }
    }
  fillRect(x0, y1 - 11, x1, y1, rgb565(110, 116, 128));   // the skirting
  fillRect(x0, y1 - 11, x1, y1 - 10, rgb565(200, 206, 214));
}

static void paintMetalCupboard(int cx, int fy) {
  int x0 = cx - 48, x1 = cx + 48, top = fy - 196;
  fillRect(x0, top, x1, fy - 1, rgb565(150, 158, 170));
  for (int x = x0 + 4; x < x1 - 4; x += 3) fillRect(x, top + 6, x, fy - 12, ((x / 3) & 1) ? rgb565(160, 168, 180) : rgb565(144, 152, 164));   // brushed steel
  fillRect(x0, top, x1, top + 4, rgb565(206, 212, 222));
  fillRect(x0, top, x0 + 2, fy - 1, rgb565(196, 202, 212));
  fillRect(x1 - 2, top, x1, fy - 1, rgb565(100, 106, 118));
  fillRect(cx - 1, top + 6, cx + 1, fy - 12, rgb565(90, 96, 108));
  fillRect(cx - 9, top + 80, cx - 5, top + 120, rgb565(60, 64, 72));       // the handles, a lock
  fillRect(cx + 5, top + 80, cx + 9, top + 120, rgb565(60, 64, 72));
  fillCircle(cx + 7, top + 134, 3, rgb565(230, 190, 60));
  for (int y = top + 16; y < top + 50; y += 6) {                           // vents top and bottom
    fillRect(x0 + 10, y, cx - 10, y + 2, rgb565(80, 86, 96));
    fillRect(cx + 10, y, x1 - 10, y + 2, rgb565(80, 86, 96));
  }
  for (int y = fy - 50; y < fy - 20; y += 6) {
    fillRect(x0 + 10, y, cx - 10, y + 2, rgb565(80, 86, 96));
    fillRect(cx + 10, y, x1 - 10, y + 2, rgb565(80, 86, 96));
  }
  fillRect(x0 + 4, fy - 10, x1 - 4, fy - 1, rgb565(60, 64, 72));
}

static void paintServerTerminal(int cx, int fy, int k) {
  int x0 = cx - 55, x1 = cx + 55;
  fillRect(x0 + 6, fy - 70, x1 - 6, fy - 1, rgb565(70, 76, 88));            // the cabinet
  fillRect(x0 + 6, fy - 70, x1 - 6, fy - 67, rgb565(120, 126, 138));
  for (int y = fy - 56; y < fy - 14; y += 8) fillRect(x0 + 16, y, x1 - 16, y + 3, rgb565(46, 50, 58));
  int dx[4] = { x0, x1, x1 - 8, x0 + 8 }, dy[4] = { fy - 70, fy - 70, fy - 86, fy - 86 };
  fillPolygon(dx, dy, 4, rgb565(110, 116, 128));                            // the sloping keyboard top
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 9; c++) fillRect(x0 + 14 + c * 10, fy - 83 + r * 6, x0 + 21 + c * 10, fy - 80 + r * 6, rgb565(40, 44, 52));
  fillRect(cx - 6, fy - 100, cx + 6, fy - 86, rgb565(60, 64, 72));         // the screen's neck
  fillRect(cx - 46, fy - 152, cx + 46, fy - 98, rgb565(40, 44, 52));
  fillRect(cx - 42, fy - 148, cx + 42, fy - 102, rgb565(4, 20, 12));
  static const char *heads[4] = { "NODE 1", "NODE 2", "NODE 3", "NODE 4" };
  drawVectorText(heads[k], cx, fy - 140, 1.0f, rgb565(80, 240, 140), false);
  for (int r = 0; r < 4; r++) fillRect(cx - 36, fy - 130 + r * 7, cx - 36 + 20 + ((k * 7 + r * 13) % 44), fy - 128 + r * 7, rgb565(60, 200, 110));
  fillCircle(x1 - 14, fy - 62, 3, rgb565(80, 240, 120));
}

static void paintServerRoom() {
  // the fans' housings (their blades turn: drawn each frame)
  for (int k = 0; k < SRV_FANS; k++) {
    int x = srvFanX(k), y = SRV_FAN_Y, r = SRV_FAN_R;
    fillRect(x - r - 8, y - r - 8, x + r + 8, y + r + 8, rgb565(120, 126, 138));
    fillRect(x - r - 8, y - r - 8, x + r + 8, y - r - 6, rgb565(220, 224, 232));
    fillRect(x - r - 8, y + r + 6, x + r + 8, y + r + 8, rgb565(80, 86, 96));
    fillCircle(x, y, r + 2, rgb565(60, 64, 72));
    fillCircle(x, y, r, rgb565(24, 26, 30));
    for (int c = 0; c < 4; c++) fillCircle(x + ((c & 1) ? r + 2 : -r - 2), y + ((c & 2) ? r + 2 : -r - 2), 3, rgb565(70, 76, 88));
  }
  drawVectorText("COOLING", 640, SRV_FAN_Y + SRV_FAN_R + 22, 1.2f, rgb565(70, 80, 96), false);
  // the big screen
  fillRect(SRV_MON_X0 - 12, SRV_MON_Y0 - 12, SRV_MON_X1 + 12, SRV_MON_Y1 + 12, rgb565(40, 44, 52));
  fillRect(SRV_MON_X0 - 12, SRV_MON_Y0 - 12, SRV_MON_X1 + 12, SRV_MON_Y0 - 9, rgb565(110, 116, 128));
  fillRect(SRV_MON_X0, SRV_MON_Y0, SRV_MON_X1, SRV_MON_Y1, rgb565(6, 14, 30));
  for (int x = SRV_MON_X0 + 20; x < SRV_MON_X1; x += 40) fillRect(x, SRV_MON_Y0 + 1, x, SRV_MON_Y1 - 1, rgb565(14, 28, 52));
  for (int y = SRV_MON_Y0 + 20; y < SRV_MON_Y1; y += 40) fillRect(SRV_MON_X0 + 1, y, SRV_MON_X1 - 1, y, rgb565(14, 28, 52));
  drawVectorText("QUANTUM NETWORK", (SRV_MON_X0 + SRV_MON_X1) / 2, SRV_MON_Y0 + 22, 2.2f, rgb565(90, 210, 255), true);
  // a wire globe on the left
  int gx = SRV_MON_X0 + 130, gy = SRV_MON_Y0 + 116, gr = 70;
  drawCircle(gx, gy, gr, rgb565(70, 170, 255));
  for (int k = 1; k < 4; k++) {
    int ry = gr * k / 4;
    for (int s = -1; s <= 1; s += 2) {
      int yy = gy + s * ry, hw = (int)sqrtf((float)(gr * gr - ry * ry));
      drawLine(gx - hw, yy, gx + hw, yy, rgb565(40, 110, 190), false);
    }
  }
  drawLine(gx - gr, gy, gx + gr, gy, rgb565(40, 110, 190), false);
  for (int k = 1; k < 4; k++) {   // meridians: ellipses
    int rx = gr * k / 4;
    float px = 0, py = 0;
    for (int a = 0; a <= 360; a += 12) {
      float x = gx + rx * cosf(a * PI / 180), y = gy + gr * sinf(a * PI / 180);
      if (a) drawLine((int)px, (int)py, (int)x, (int)y, rgb565(40, 110, 190), false);
      px = x; py = y;
    }
  }
  static const int dots[5][2] = { { -30, -40 }, { 10, -50 }, { 50, -10 }, { 20, 0 }, { -10, 40 } };
  for (int k = 0; k < 5; k++) fillCircle(gx + dots[k][0], gy + dots[k][1], 4, rgb565(255, 80, 60));
  for (int k = 0; k < 4; k++) drawLine(gx + dots[k][0], gy + dots[k][1], gx + dots[k + 1][0], gy + dots[k + 1][1], rgb565(255, 160, 80), false);
  // bars and figures on the right
  static const char *rows[5] = { "CORE TEMP", "QUBITS", "LOAD", "LINKS", "FIREWALL" };
  for (int r = 0; r < 5; r++) {
    int y = SRV_MON_Y0 + 60 + r * 28, x = SRV_MON_X0 + 270;
    drawVectorText(rows[r], (float)x + 60, (float)y, 1.2f, rgb565(140, 200, 240), false);
    fillRect(x + 140, y - 6, x + 380, y + 6, rgb565(20, 40, 70));
    fillRect(x + 140, y - 6, x + 140 + 60 + (r * 47) % 170, y + 6, r == 0 ? rgb565(255, 150, 60) : rgb565(70, 200, 255));
  }
  // the four terminals and the cupboards
  for (int k = 0; k < 4; k++) paintServerTerminal(serverItems[2 + k].x, ROOM_FLOOR_Y, k);
  for (int k : { 0, 1, 6, 7 }) paintMetalCupboard(serverItems[k].x, ROOM_FLOOR_Y);
}

// ---- NUCLEAR MISSION's lab equipment (its B1 / B2 research rooms), as it was: same units, same sizes
static void nkBio(int x, int y, int r) { nmAt(1, 0, 0, 0, 0); nBioSign(x, y, r); }
static void nkGloveBox(int x0, int x1, int fy) {
  int top = fy - 150, base = fy - 60;
  fillRect(x0 + 6, base, x0 + 12, fy - 1, rgb565(150, 156, 166));
  fillRect(x1 - 12, base, x1 - 6, fy - 1, rgb565(150, 156, 166));
  fillRect(x0, top, x1, base, rgb565(176, 182, 192));
  fillRect(x0 + 8, top + 10, x1 - 8, base - 10, rgb565(120, 170, 196));   // the window, lit inside
  drawLine(x0 + 20, base - 14, x0 + 50, top + 14, rgb565(196, 226, 240), false);
  for (int s = 0; s < 2; s++) {
    int gx = x0 + (x1 - x0) * (s ? 3 : 1) / 4, gy = base - 28;
    fillCircle(gx, gy, 12, rgb565(30, 30, 34));
    fillRect(gx - 6, gy, gx + 6, gy + 24, rgb565(26, 26, 30));   // the glove hanging in
  }
  fillRect(x0 + 10, top + 16, x0 + 40, top + 30, rgb565(220, 220, 230));   // something inside: a rack of tubes
  for (int k = 0; k < 5; k++) fillRect(x0 + 12 + k * 6, top + 8, x0 + 15 + k * 6, top + 16, k & 1 ? rgb565(220, 60, 60) : rgb565(60, 160, 220));
  nkBio(x1 - 20, top + 20, 10);
}
static void nkLabFridge(int x, int fy) {   // tall, glass door, tubes on racks
  fillRect(x, fy - 150, x + 60, fy - 1, rgb565(232, 236, 240));
  fillRect(x + 6, fy - 144, x + 54, fy - 20, rgb565(150, 190, 210));
  for (int r = 0; r < 4; r++) {
    int y = fy - 136 + r * 30;
    fillRect(x + 8, y + 18, x + 52, y + 20, rgb565(200, 206, 214));
    for (int k = 0; k < 6; k++) fillRect(x + 10 + k * 7, y + 6, x + 13 + k * 7, y + 18, (k + r) % 3 == 0 ? rgb565(220, 60, 60) : (k + r) % 3 == 1 ? rgb565(60, 160, 220) : rgb565(240, 200, 60));
  }
  fillRect(x + 50, fy - 110, x + 53, fy - 70, rgb565(150, 156, 166));
  fillRect(x + 8, fy - 16, x + 22, fy - 10, rgb565(40, 200, 120));   // its display
}
static void nkCages(int x, int fy, int rows) {   // stacked animal cages
  for (int r = 0; r < rows; r++) {
    int y1 = fy - 4 - r * 46, y0 = y1 - 40;
    fillRect(x, y0, x + 80, y1, rgb565(60, 64, 72));
    fillRect(x + 3, y0 + 3, x + 77, y1 - 3, rgb565(200, 196, 170));   // bedding
    for (int k = x + 6; k < x + 78; k += 6) fillRect(k, y0 + 3, k, y1 - 3, rgb565(150, 156, 166));   // bars
    fillCircle(x + 22 + (r * 17) % 40, y1 - 9, 5, rgb565(244, 244, 244));   // a white mouse
    fillCircle(x + 27 + (r * 17) % 40, y1 - 11, 3, rgb565(244, 244, 244));
    fillRect(x + 4, y0 - 6, x + 30, y0 - 1, rgb565(250, 250, 250));   // label
  }
}
static void nkContainment(int x, int fy) {   // a sealed yellow containment box
  fillRect(x, fy - 60, x + 70, fy - 1, rgb565(220, 180, 40));
  fillRect(x, fy - 60, x + 70, fy - 52, rgb565(240, 206, 70));
  fillRect(x + 4, fy - 30, x + 66, fy - 26, rgb565(30, 30, 34));
  nkBio(x + 35, fy - 34, 13);
  fillRect(x + 6, fy - 8, x + 14, fy - 1, rgb565(80, 80, 84));
  fillRect(x + 56, fy - 8, x + 64, fy - 1, rgb565(80, 80, 84));
}
static void nkIsolation(int x, int fy) {   // a clear isolation box with a green light inside
  fillRect(x, fy - 74, x + 80, fy - 1, rgb565(170, 210, 226));
  drawRectOutline(x, fy - 74, x + 80, fy - 1, rgb565(200, 206, 214), 3);
  fillRect(x + 10, fy - 20, x + 70, fy - 6, rgb565(60, 160, 100));
  drawLine(x + 10, fy - 10, x + 30, fy - 64, rgb565(226, 244, 250), false);
  fillCircle(x + 64, fy - 64, 4, rgb565(60, 240, 120));
  fillRect(x + 30, fy - 84, x + 50, fy - 74, rgb565(150, 156, 166));   // its air filter
}
static void nkIncubator(int x, int fy) {
  fillRect(x, fy - 70, x + 60, fy - 1, rgb565(230, 232, 236));
  fillRect(x + 6, fy - 62, x + 54, fy - 18, rgb565(110, 120, 140));
  for (int k = 0; k < 3; k++) fillRect(x + 10, fy - 54 + k * 12, x + 50, fy - 52 + k * 12, rgb565(160, 170, 186));
  fillRect(x + 8, fy - 12, x + 24, fy - 6, rgb565(255, 120, 40));
}
static void nkCentrifuge(int x, int fy) {
  fillRect(x, fy - 34, x + 54, fy - 1, rgb565(226, 230, 236));
  fillRect(x - 2, fy - 40, x + 56, fy - 34, rgb565(190, 196, 206));
  fillCircle(x + 27, fy - 37, 12, rgb565(150, 156, 166));
  fillRect(x + 6, fy - 22, x + 20, fy - 16, rgb565(40, 180, 240));
}
static void nkDewar(int x, int fy) {   // a liquid nitrogen tank
  fillRect(x, fy - 90, x + 40, fy - 1, rgb565(190, 196, 206));
  fillRect(x + 4, fy - 90, x + 8, fy - 1, rgb565(226, 230, 236));
  fillRect(x + 12, fy - 102, x + 28, fy - 90, rgb565(150, 156, 166));
  fillRect(x + 6, fy - 60, x + 34, fy - 48, rgb565(40, 90, 190));
  drawVectorText("LN2", x + 20, fy - 54, 1.0f, rgb565(255, 255, 255), false);
}
static void nkMicroscope(int x, int fy) {
  fillRect(x, fy - 52, x + 110, fy - 46, rgb565(226, 230, 236));
  fillRect(x + 4, fy - 46, x + 10, fy - 1, rgb565(150, 156, 166));
  fillRect(x + 100, fy - 46, x + 106, fy - 1, rgb565(150, 156, 166));
  int mx = x + 40;   // the microscope
  fillRect(mx, fy - 60, mx + 26, fy - 52, rgb565(40, 44, 52));
  fillRect(mx + 16, fy - 96, mx + 22, fy - 60, rgb565(220, 224, 230));
  int ex[4] = { mx + 6, mx + 14, mx + 24, mx + 16 }, ey[4] = { fy - 110, fy - 116, fy - 92, fy - 86 };
  fillPolygon(ex, ey, 4, rgb565(40, 44, 52));
  fillRect(mx + 4, fy - 76, mx + 18, fy - 72, rgb565(200, 204, 210));
  fillRect(x + 76, fy - 58, x + 96, fy - 52, rgb565(60, 160, 220));   // petri dishes
}
static void nkLockers(int x, int fy) {
  for (int k = 0; k < 2; k++) {
    int x0 = x + k * 40;
    fillRect(x0, fy - 150, x0 + 38, fy - 1, rgb565(120, 128, 140));
    fillRect(x0, fy - 150, x0 + 38, fy - 147, rgb565(160, 168, 180));
    for (int v = 0; v < 4; v++) fillRect(x0 + 8, fy - 136 + v * 6, x0 + 30, fy - 134 + v * 6, rgb565(70, 76, 88));
    fillRect(x0 + 30, fy - 90, x0 + 33, fy - 70, rgb565(200, 204, 210));
    fillRect(x0 + 6, fy - 60, x0 + 18, fy - 54, rgb565(240, 240, 236));   // a name card
  }
}
static void nkFiling(int x, int fy) {
  fillRect(x, fy - 120, x + 50, fy - 1, rgb565(110, 118, 110));
  for (int k = 0; k < 4; k++) {
    int y = fy - 116 + k * 29;
    fillRect(x + 4, y, x + 46, y + 25, rgb565(128, 136, 128));
    fillRect(x + 18, y + 10, x + 32, y + 13, rgb565(200, 204, 210));
    fillRect(x + 20, y + 3, x + 30, y + 7, rgb565(240, 240, 236));
  }
}
static void nkLabBench(int x, int fy) {   // a white bench, a rack of test tubes, a flask
  fillRect(x, fy - 52, x + 110, fy - 44, rgb565(236, 238, 242));
  fillRect(x + 4, fy - 44, x + 106, fy - 1, rgb565(200, 206, 214));
  for (int k = 0; k < 3; k++) fillRect(x + 10 + k * 34, fy - 36, x + 38 + k * 34, fy - 6, rgb565(176, 182, 192));   // drawers
  fillRect(x + 8, fy - 62, x + 50, fy - 58, rgb565(150, 156, 166));   // the rack
  for (int k = 0; k < 7; k++) {
    int tx = x + 11 + k * 6;
    uint16_t c = (k % 3 == 0) ? rgb565(220, 60, 60) : (k % 3 == 1) ? rgb565(60, 200, 120) : rgb565(240, 200, 60);
    fillRect(tx, fy - 76, tx + 3, fy - 58, rgb565(220, 230, 236));
    fillRect(tx, fy - 66, tx + 3, fy - 58, c);
  }
  int fx[4] = { x + 70, x + 92, x + 86, x + 76 }, fyy[4] = { fy - 52, fy - 52, fy - 78, fy - 78 };   // a flask
  fillPolygon(fx, fyy, 4, rgb565(160, 220, 240));
  fillRect(x + 76, fy - 90, x + 86, fy - 78, rgb565(200, 230, 240));
  fillRect(x + 72, fy - 64, x + 90, fy - 53, rgb565(80, 200, 140));
}
static void nkSilverFridge(int x, int fy) {   // big, double-doored, stainless
  fillRect(x, fy - 160, x + 90, fy - 1, rgb565(200, 206, 214));
  for (int k = x + 4; k < x + 88; k += 6) fillRect(k, fy - 156, k, fy - 4, rgb565(186, 192, 202));   // brushed
  fillRect(x + 44, fy - 160, x + 46, fy - 1, rgb565(130, 136, 146));
  fillRect(x + 36, fy - 110, x + 40, fy - 60, rgb565(150, 156, 166));
  fillRect(x + 50, fy - 110, x + 54, fy - 60, rgb565(150, 156, 166));
  fillRect(x + 8, fy - 150, x + 34, fy - 140, rgb565(30, 40, 50));
  fillRect(x + 10, fy - 148, x + 22, fy - 142, rgb565(60, 220, 255));
  nkBio(x + 68, fy - 140, 9);
}
static void nkWasteDrums(int x, int fy) {   // yellow waste drums with the trefoil
  for (int k = 0; k < 3; k++) {
    int dx = x + k * 30;
    fillRect(dx, fy - 60, dx + 26, fy - 1, rgb565(220, 180, 40));
    fillRect(dx, fy - 60, dx + 26, fy - 56, rgb565(170, 140, 30));
    fillRect(dx, fy - 34, dx + 26, fy - 31, rgb565(170, 140, 30));
    fillCircle(dx + 13, fy - 44, 6, rgb565(20, 20, 22));
    fillCircle(dx + 13, fy - 44, 2, rgb565(220, 180, 40));
  }
  fillRect(x, fy - 110, x + 86, fy - 96, rgb565(200, 30, 30));
  drawVectorText("WASTE", x + 43, fy - 103, 1.3f, rgb565(255, 255, 255), true);
}

// A computer terminal, the old kind (not the server room's): a desk, a deep beige monitor, a tower
// under it, a chair. broken: its screen cracked and dead.
static void paintCrtDesk(int x, int fy, bool broken) {
  uint16_t beige = rgb565(214, 204, 176), beigeD = rgb565(176, 166, 140);
  fillRect(x, fy - 60, x + 120, fy - 54, rgb565(120, 128, 140));          // the desk
  fillRect(x + 4, fy - 54, x + 10, fy - 1, rgb565(90, 96, 106));
  fillRect(x + 110, fy - 54, x + 116, fy - 1, rgb565(90, 96, 106));
  fillRect(x + 84, fy - 50, x + 106, fy - 1, beige);                       // the tower
  fillRect(x + 88, fy - 44, x + 102, fy - 40, beigeD);
  fillRect(x + 88, fy - 34, x + 102, fy - 30, beigeD);
  fillCircle(x + 95, fy - 14, 2, broken ? rgb565(90, 30, 20) : rgb565(80, 240, 120));
  fillRect(x + 40, fy - 70, x + 72, fy - 61, beigeD);                      // the monitor's foot
  fillRect(x + 16, fy - 140, x + 96, fy - 70, beige);                      // its deep case
  fillRect(x + 16, fy - 140, x + 96, fy - 136, rgb565(232, 224, 200));
  fillRect(x + 90, fy - 136, x + 96, fy - 72, beigeD);
  fillRect(x + 24, fy - 130, x + 86, fy - 80, rgb565(30, 34, 30));        // the screen's bezel
  if (!broken) {
    fillRect(x + 27, fy - 127, x + 83, fy - 83, rgb565(6, 30, 14));
    drawVectorText("Q-TERM", x + 55, fy - 119, 1.0f, rgb565(90, 250, 130), false);
    drawVectorText("LOGIN:", x + 46, fy - 104, 0.9f, rgb565(90, 250, 130), false);
    fillRect(x + 32, fy - 94, x + 74, fy - 92, rgb565(50, 170, 90));
  } else {
    fillRect(x + 27, fy - 127, x + 83, fy - 83, rgb565(14, 16, 18));
    int cx = x + 50, cy = fy - 108;
    drawLine(cx, cy, x + 28, fy - 124, rgb565(190, 200, 210), false);    // the crack, out from where it was hit
    drawLine(cx, cy, x + 80, fy - 120, rgb565(190, 200, 210), false);
    drawLine(cx, cy, x + 70, fy - 84, rgb565(170, 180, 190), false);
    drawLine(cx, cy, x + 30, fy - 90, rgb565(170, 180, 190), false);
    drawLine(cx, cy, x + 58, fy - 126, rgb565(150, 160, 170), false);
    fillCircle(x + 70, fy - 150, 16, rgb565(150, 150, 150));              // (soot over it)
    fillCircle(x + 56, fy - 156, 12, rgb565(170, 170, 170));
  }
  fillRect(x + 22, fy - 64, x + 80, fy - 60, beige);                        // the keyboard
  for (int k = 0; k < 8; k++) fillRect(x + 25 + k * 7, fy - 63, x + 29 + k * 7, fy - 62, beigeD);
  if (broken) {                                                             // the chair, knocked over
    int cx[4] = { x + 124, x + 160, x + 162, x + 126 }, cy[4] = { fy - 12, fy - 26, fy - 20, fy - 6 };
    fillPolygon(cx, cy, 4, rgb565(40, 40, 46));
    fillRect(x + 128, fy - 6, x + 168, fy - 2, rgb565(60, 60, 66));
  } else {
    fillRect(x + 124, fy - 34, x + 156, fy - 30, rgb565(40, 40, 46));
    fillRect(x + 150, fy - 76, x + 156, fy - 30, rgb565(40, 40, 46));
    fillRect(x + 138, fy - 30, x + 142, fy - 6, rgb565(90, 90, 96));
    fillRect(x + 126, fy - 6, x + 154, fy - 3, rgb565(90, 90, 96));
  }
}

static void paintQuantumAtom(int cx, int cy, int r, uint16_t c) {   // the lab's emblem: a nucleus, three orbits
  for (int k = 0; k < 3; k++) {
    float a = k * PI / 3;
    float px = 0, py = 0;
    for (int d = 0; d <= 360; d += 10) {
      float t = d * PI / 180, ex = r * cosf(t), ey = r * 0.36f * sinf(t);
      float x = cx + ex * cosf(a) - ey * sinf(a), y = cy + ex * sinf(a) + ey * cosf(a);
      if (d) drawLineF(px, py, x, y, c, 2.0f);
      px = x; py = y;
    }
  }
  fillCircle(cx, cy, r / 6, c);
}

static void paintQLabSign(int cx, int y, const char *name, bool broken) {
  int x0 = cx - 230, x1 = cx + 230;
  fillRect(x0, y, x1, y + 70, rgb565(14, 20, 34));
  drawRectOutline(x0, y, x1, y + 70, rgb565(120, 90, 255), 3);
  paintQuantumAtom(x0 + 44, y + 35, 28, rgb565(150, 120, 255));
  drawVectorText(name, cx + 26, y + 35, 3.2f, rgb565(210, 200, 255), true);
  if (broken) {                                                             // cracked, a corner gone
    drawLine(x1 - 120, y, x1 - 90, y + 30, rgb565(220, 226, 240), false);
    drawLine(x1 - 90, y + 30, x1 - 104, y + 70, rgb565(220, 226, 240), false);
    int px[3] = { x1 - 40, x1 + 1, x1 + 1 }, py[3] = { y + 71, y + 30, y + 71 };
    fillPolygon(px, py, 3, rgb565(226, 230, 234));
  }
}

// ---- QUANTUM LAB 1: NUCLEAR MISSION's lab equipment, a glove box, a deck on the right; three scientists
#define QLAB_DECK_Y 440
static void paintQuantumLab1() {
  paintQLabSign(420, 150, "QUANTUM LAB 1", false);
  nkBio(200, 300, 18);                                                      // a biohazard sign, a shelf of bottles
  fillRect(240, 310, 400, 314, rgb565(170, 176, 186));
  for (int k = 0; k < 8; k++) fillRect(246 + k * 19, 288, 259 + k * 19, 310, k & 1 ? rgb565(60, 120, 200) : rgb565(240, 240, 236));
  paintQuantumAtom(1010, 220, 70, rgb565(170, 150, 240));                   // the emblem over the deck
  drawVectorText("QUANTUM RESEARCH", 1010, 314, 2.0f, rgb565(60, 72, 96), true);
  // the deck: its slab and a rail
  paintDeckSlab(QLAB_DECK_Y, 760, SCREEN_W - 1);
  for (int x : { 764, 900, 1040, 1180, SCREEN_W - 20 }) fillRect(x, QLAB_DECK_Y - 62, x + 3, QLAB_DECK_Y - 1, rgb565(150, 156, 166));
  fillRect(764, QLAB_DECK_Y - 64, SCREEN_W - 17, QLAB_DECK_Y - 59, rgb565(222, 226, 232));
  for (int x = 840; x < SCREEN_W - 40; x += 200) {                          // lights under it
    fillRect(x, QLAB_DECK_Y + PLAT_T + 2, x + 60, QLAB_DECK_Y + PLAT_T + 6, rgb565(250, 252, 255));
  }
  // the floor: a fridge, mouse cages, a bench, the terminal; under the deck the glove box, a microscope, a centrifuge
  nkLabFridge(150, ROOM_FLOOR_Y);
  nkCages(240, ROOM_FLOOR_Y, 3);
  nkLabBench(350, ROOM_FLOOR_Y);
  paintCrtDesk(480, ROOM_FLOOR_Y, false);
  nkGloveBox(790, 924, ROOM_FLOOR_Y);
  nkMicroscope(980, ROOM_FLOOR_Y);
  nkCentrifuge(1130, ROOM_FLOOR_Y);
  // up on the deck
  nkSilverFridge(790, QLAB_DECK_Y);
  nkIncubator(920, QLAB_DECK_Y);
  nkIsolation(1010, QLAB_DECK_Y);
  nkDewar(1120, QLAB_DECK_Y);
  nkFiling(1190, QLAB_DECK_Y);
}

// ---- QUANTUM LAB 2: the same kind of lab after an accident - two holes in the floor, things sparking
#define QLAB2_PIT1 420
#define QLAB2_PIT2 800
#define QLAB2_PIT_W 110
static const int16_t qlab2Spark[4][2] = { { 476, 352 }, { 858, 330 }, { 690, 548 }, { 985, 575 } };   // the cables' ends, the glove box, the terminal

static void paintScorch(int x, int y, int r) {   // soot: a ragged cloud, darker in the middle
  uint32_t h = (uint32_t)(x * 7919 + y * 104729);
  for (int k = 0; k < 4; k++) {
    int v = 206 - k * 26, n = 10 - k;
    float spread = r * (1.0f - k * 0.22f), rad = r * (0.34f - k * 0.05f);
    for (int j = 0; j < n; j++) {
      h = h * 1103515245u + 12345u;
      float a = (h >> 8) % 628 / 100.0f, d = spread * ((h >> 18) % 100) / 100.0f;
      fillCircle(x + (int)(cosf(a) * d * 1.3f), y + (int)(sinf(a) * d * 0.8f), (int)(rad * (0.7f + ((h >> 4) % 60) / 100.0f)), rgb565(v, v, v + 2));
    }
  }
}

static void paintBrokenPit(int x0, int x1) {
  fillRect(x0, ROOM_FLOOR_Y - 1, x1 - 1, PLAY_Y1, rgb565(6, 6, 8));         // the hole
  for (int i = 0; i < 4; i++)
    fillRect(x0 + 4 + i * 6, PLAY_Y1 - 6 + i, x1 - 5 - i * 6, PLAY_Y1 - 6 + i, rgb565(70 - i * 12, 20, 14));
  for (int s = 0; s < 2; s++) {                                             // its broken edges: jagged slab, bent rods sticking out
    int e = s ? x1 : x0, d = s ? -1 : 1;
    int jx[6] = { e, e + d * 10, e + d * 4, e + d * 14, e + d * 6, e }, jy[6] = { ROOM_FLOOR_Y, ROOM_FLOOR_Y + 6, ROOM_FLOOR_Y + 14, ROOM_FLOOR_Y + 22, ROOM_FLOOR_Y + 34, ROOM_FLOOR_Y + 40 };
    fillPolygon(jx, jy, 6, rgb565(120, 124, 132));
    drawLine(e, ROOM_FLOOR_Y + 8, e + d * 24, ROOM_FLOOR_Y + 2, rgb565(150, 90, 50), true);
    drawLine(e, ROOM_FLOOR_Y + 24, e + d * 20, ROOM_FLOOR_Y + 34, rgb565(150, 90, 50), true);
    for (int k = 0; k < 4; k++) {                                           // rubble at the edge
      int rx = e - d * (8 + k * 13), ry = ROOM_FLOOR_Y - 4 - (k & 1) * 3;
      int px[4] = { rx - 5, rx + 4, rx + 6, rx - 4 }, py[4] = { ry + 4, ry - 3, ry + 4, ry + 5 };
      fillPolygon(px, py, 4, rgb565(140, 144, 150));
    }
  }
}

static void paintPitTape(int x0, int x1) {   // (behind everything: warning tape strung across a hole on two posts)
  for (int x = x0 - 30; x < x1 + 30; x += 24) {
    fillRect(x, ROOM_FLOOR_Y - 92, x + 11, ROOM_FLOOR_Y - 86, rgb565(240, 200, 30));
    fillRect(x + 12, ROOM_FLOOR_Y - 92, x + 23, ROOM_FLOOR_Y - 86, rgb565(20, 20, 22));
  }
  fillRect(x0 - 34, ROOM_FLOOR_Y - 100, x0 - 30, ROOM_FLOOR_Y - 1, rgb565(230, 120, 30));   // its two posts
  fillRect(x1 + 30, ROOM_FLOOR_Y - 100, x1 + 34, ROOM_FLOOR_Y - 1, rgb565(230, 120, 30));
}

static void paintQuantumLab2() {
  // the wall: cracks, scorch marks
  paintScorch(560, 420, 44);
  paintScorch(1010, 470, 32);
  paintScorch(330, 250, 40);
  for (int c = 0; c < 4; c++) {
    static const int16_t cr[4][2] = { { 340, 200 }, { 700, 260 }, { 1100, 220 }, { 870, 420 } };
    int x = cr[c][0], y = cr[c][1];
    for (int k = 0; k < 5; k++) {
      int nx = x + ((k * 37 + c * 11) % 30) - 12, ny = y + 22 + (k * 13) % 14;
      drawLine(x, y, nx, ny, rgb565(150, 156, 166), false);
      x = nx; y = ny;
    }
  }
  paintQLabSign(640, 150, "QUANTUM LAB 2", true);
  // the sign: DANGER, UNSAFE FLOOR
  fillRect(1010, 290, 1210, 350, rgb565(240, 200, 30));
  drawRectOutline(1010, 290, 1210, 350, rgb565(20, 20, 22), 3);
  drawVectorText("DANGER", 1110, 308, 2.4f, rgb565(20, 20, 22), true);
  drawVectorText("UNSAFE FLOOR", 1110, 334, 1.6f, rgb565(20, 20, 22), true);
  // a ceiling panel hanging down, cables dangling over the holes
  {
    int px[4] = { 130, 270, 258, 122 }, py[4] = { CEIL_Y + 60, CEIL_Y + 60, CEIL_Y + 196, CEIL_Y + 92 };
    fillPolygon(px, py, 4, rgb565(170, 176, 186));
    drawLine(130, CEIL_Y + 60, 122, CEIL_Y + 92, rgb565(110, 116, 126), true);
    drawLine(270, CEIL_Y + 60, 258, CEIL_Y + 196, rgb565(110, 116, 126), true);
    fillRect(130, CEIL_Y + 58, 270, CEIL_Y + 62, rgb565(30, 30, 34));      // (the gap it left)
  }
  for (int c = 0; c < 2; c++) {
    int x0 = c ? 840 : 500, ex = qlab2Spark[c][0], ey = qlab2Spark[c][1];
    float px = (float)x0, py = CEIL_Y + 60;
    for (int k = 1; k <= 8; k++) {                                          // a cable sagging down to its loose end
      float t = k / 8.0f, x = x0 + (ex - x0) * t + 14 * sinf(t * PI), y = CEIL_Y + 60 + (ey - CEIL_Y - 60) * t;
      drawLineF(px, py, x, y, c ? rgb565(30, 30, 34) : rgb565(160, 40, 30), 3.0f);
      px = x; py = y;
    }
    drawLine(ex, ey, ex - 5, ey + 7, rgb565(200, 130, 60), false);          // its copper ends
    drawLine(ex, ey, ex + 4, ey + 8, rgb565(200, 130, 60), false);
  }
  // the light hanging off one wire
  drawLine(1160, CEIL_Y + 60, 1146, CEIL_Y + 150, rgb565(40, 40, 44), false);
  {
    int lx[4] = { 1114, 1178, 1182, 1118 }, ly[4] = { CEIL_Y + 150, CEIL_Y + 140, CEIL_Y + 150, CEIL_Y + 160 };
    fillPolygon(lx, ly, 4, rgb565(200, 206, 214));
  }
  // tape round the holes (the holes themselves go in after the floor)
  paintPitTape(QLAB2_PIT1, QLAB2_PIT1 + QLAB2_PIT_W);
  paintPitTape(QLAB2_PIT2, QLAB2_PIT2 + QLAB2_PIT_W);
  // what's left: lockers, a containment box, drums (one rolled over), the glove box (its glass smashed),
  // a nitrogen tank, the terminal, a bench
  nkLockers(140, ROOM_FLOOR_Y);
  nkContainment(240, ROOM_FLOOR_Y);
  nkWasteDrums(326, ROOM_FLOOR_Y);
  nkGloveBox(560, 694, ROOM_FLOOR_Y);
  {
    int gx0 = 568, gy0 = ROOM_FLOOR_Y - 140, gx1 = 686, gy1 = ROOM_FLOOR_Y - 70;   // the window, smashed
    fillRect(gx0, gy0, gx1, gy1, rgb565(40, 50, 60));
    int hx = 640, hy = ROOM_FLOOR_Y - 104;
    for (int k = 0; k < 7; k++) {
      float a = k * 0.9f + 0.3f;
      drawLine(hx, hy, hx + (int)(60 * cosf(a)), hy + (int)(34 * sinf(a)), rgb565(200, 220, 230), false);
    }
    for (int k = 0; k < 6; k++) {                                            // glass on the floor
      int sx = 560 + k * 23, sy = ROOM_FLOOR_Y - 3;
      int px[3] = { sx, sx + 7, sx + 3 }, py[3] = { sy, sy, sy - 4 };
      fillPolygon(px, py, 3, rgb565(190, 220, 236));
    }
    paintScorch(690, ROOM_FLOOR_Y - 150, 22);
  }
  nkDewar(735, ROOM_FLOOR_Y);
  paintCrtDesk(930, ROOM_FLOOR_Y, true);
  nkLabBench(1110, ROOM_FLOOR_Y);
  {
    int dx = 200, dy = ROOM_FLOOR_Y;                                         // a drum on its side, rolled out
    fillRect(dx - 30, dy - 26, dx + 30, dy - 1, rgb565(220, 180, 40));
    fillRect(dx - 30, dy - 26, dx - 26, dy - 1, rgb565(170, 140, 30));
    fillRect(dx - 2, dy - 26, dx + 1, dy - 1, rgb565(170, 140, 30));
    fillCircle(dx + 30, dy - 13, 12, rgb565(200, 160, 36));
    fillCircle(dx + 30, dy - 13, 5, rgb565(20, 20, 22));
  }
}

// ---- lift E: the CODE ROOM. The board's frame (its squares light up: drawn each frame), its terminal below the
// middle, and the missiles' display on the right (the hologram globe, the flights, the dead)
static void paintSimonRoom() {
  fillRect(SIM_X0 - 10, SIM_Y0 - 10, SIM_X0 + SIM_COLS * SIM_CW + 10, SIM_Y0 + SIM_ROWS * SIM_CH + 10, rgb565(40, 44, 54));
  drawRectOutline(SIM_X0 - 10, SIM_Y0 - 10, SIM_X0 + SIM_COLS * SIM_CW + 10, SIM_Y0 + SIM_ROWS * SIM_CH + 10, rgb565(120, 128, 146), 2);
  fillRect(SIM_X0, SIM_Y0, SIM_X0 + SIM_COLS * SIM_CW, SIM_Y0 + SIM_ROWS * SIM_CH, rgb565(14, 16, 24));
  // the terminal: a pedestal, a sloping keypad, a screen
  int cx = SIM_TERM_X, fy = ROOM_FLOOR_Y;
  fillRect(cx - 34, fy - 90, cx + 34, fy - 1, rgb565(70, 76, 88));
  fillRect(cx - 34, fy - 90, cx + 34, fy - 86, rgb565(130, 136, 150));
  fillRect(cx - 50, fy - 12, cx + 50, fy - 1, rgb565(50, 54, 64));
  int kx[4] = { cx - 60, cx + 60, cx + 52, cx - 52 }, ky[4] = { fy - 90, fy - 90, fy - 110, fy - 110 };
  fillPolygon(kx, ky, 4, rgb565(110, 116, 128));
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 8; c++) {
      static const uint8_t hue[8][3] = { { 230, 60, 60 }, { 240, 150, 40 }, { 230, 220, 60 }, { 90, 220, 90 }, { 60, 200, 220 }, { 70, 110, 240 }, { 170, 90, 230 }, { 230, 90, 180 } };
      fillRect(cx - 46 + c * 12, fy - 106 + r * 8, cx - 38 + c * 12, fy - 101 + r * 8, rgb565(hue[c][0], hue[c][1], hue[c][2]));
    }
  fillRect(cx - 8, fy - 126, cx + 8, fy - 110, rgb565(60, 64, 72));
  fillRect(cx - 54, fy - 140 - 60, cx + 54, fy - 124, rgb565(40, 44, 52));
  fillRect(cx - 50, fy - 196, cx + 50, fy - 128, rgb565(6, 16, 30));
  // the missiles' display: a dark housing, the globe's projector
  fillRect(MD_X0, MD_Y0, MD_X1, MD_Y1, rgb565(36, 40, 50));
  fillRect(MD_X0 + 6, MD_Y0 + 6, MD_X1 - 6, MD_Y1 - 6, rgb565(6, 10, 18));
  drawRectOutline(MD_X0, MD_Y0, MD_X1, MD_Y1, rgb565(120, 128, 146), 2);
  int px = (MD_X0 + MD_X1) / 2, py = MD_Y0 + 200;
  fillRect(px - 50, py, px + 50, py + 10, rgb565(70, 76, 88));
  fillRect(px - 30, py - 6, px + 30, py, rgb565(110, 116, 128));
  fillRect(MD_X0 + 18, MD_Y0 + 222, MD_X1 - 18, MD_Y0 + 223, rgb565(50, 120, 150));
  fillRect(MD_X0 + 30, MD_Y1, MD_X0 + 40, ROOM_FLOOR_Y - 1, rgb565(70, 76, 88));   // (its legs)
  fillRect(MD_X1 - 40, MD_Y1, MD_X1 - 30, ROOM_FLOOR_Y - 1, rgb565(70, 76, 88));
}

// ---- lift E: the SELF DESTRUCT room. The console (and the wall screen over it), filing cabinets, a cupboard, a desk
static void paintDestructRoom() {
  fillRect(330, 128, 950, 184, rgb565(200, 30, 30));                          // the sign
  drawRectOutline(330, 128, 950, 184, rgb565(255, 220, 60), 3);
  drawVectorText("NUCLEAR SELF DESTRUCT CONTROL", 640, 156, 3.0f, rgb565(255, 240, 220), true);
  for (int x = 20; x < SCREEN_W - 20; x += 40) {                               // hazard stripes along the wall's foot
    int sx[4] = { x, x + 20, x + 32, x + 12 }, sy[4] = { ROOM_FLOOR_Y - 26, ROOM_FLOOR_Y - 26, ROOM_FLOOR_Y - 14, ROOM_FLOOR_Y - 14 };
    fillPolygon(sx, sy, 4, rgb565(240, 196, 40));
  }
  fillRect(470, 214, 810, 430, rgb565(40, 44, 52));                           // the wall screen
  fillRect(478, 222, 802, 422, rgb565(6, 14, 22));
  for (int x = 478; x < 802; x += 27) fillRect(x, 222, x, 421, rgb565(20, 50, 66));
  for (int y = 222; y < 422; y += 25) fillRect(478, y, 801, y, rgb565(20, 50, 66));
  drawVectorText("MISSILE TRACKING", 640, 238, 1.6f, rgb565(100, 230, 255), false);
  // the console
  int cx = DS_CONSOLE_X, fy = ROOM_FLOOR_Y;
  fillRect(cx - 100, fy - 96, cx + 100, fy - 1, rgb565(60, 66, 78));
  fillRect(cx - 100, fy - 96, cx + 100, fy - 92, rgb565(130, 136, 150));
  int kx[4] = { cx - 110, cx + 110, cx + 96, cx - 96 }, ky[4] = { fy - 96, fy - 96, fy - 124, fy - 124 };
  fillPolygon(kx, ky, 4, rgb565(96, 102, 116));
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 14; c++) fillRect(cx - 86 + c * 12, fy - 118 + r * 9, cx - 78 + c * 12, fy - 112 + r * 9, rgb565(40, 44, 52));
  fillRect(cx + 50, fy - 80, cx + 90, fy - 40, rgb565(240, 196, 40));        // the big red button, under its cover
  fillCircle(cx + 70, fy - 60, 14, rgb565(210, 30, 30));
  fillRect(cx - 90, fy - 80, cx - 10, fy - 30, rgb565(30, 34, 40));
  fillRect(cx - 50, fy - 196, cx + 50, fy - 128, rgb565(40, 44, 52));        // its screen
  fillRect(cx - 46, fy - 192, cx + 46, fy - 132, rgb565(30, 6, 6));
  drawVectorText("SELF", cx, fy - 172, 1.4f, rgb565(255, 90, 70), false);
  drawVectorText("DESTRUCT", cx, fy - 152, 1.4f, rgb565(255, 90, 70), false);
  paintFilingCabinet(170, ROOM_FLOOR_Y);
  paintMetalCupboard(330, ROOM_FLOOR_Y);
  paintOfficeDesk(860, 1000, ROOM_FLOOR_Y);
  paintFilingCabinet(1110, ROOM_FLOOR_Y);
}

// ---- lift F: THE PROFESSOR's office. A world map screen, bookshelves, his desk, a globe
static void paintBossRoom() {
  fillRect(380, 128, 900, 180, rgb565(30, 24, 40));
  drawRectOutline(380, 128, 900, 180, rgb565(200, 170, 80), 3);
  drawVectorText("DR. VOSS", 640, 154, 3.4f, rgb565(230, 200, 110), true);
  fillRect(400, 214, 880, 470, rgb565(40, 44, 52));                           // the world map
  fillRect(408, 222, 872, 462, rgb565(8, 22, 40));
  static const int16_t land[][4] = { { 450, 260, 560, 330 }, { 500, 330, 560, 420 }, { 610, 250, 690, 300 }, { 620, 300, 680, 400 },
                                     { 690, 240, 830, 330 }, { 760, 330, 800, 370 }, { 780, 390, 840, 430 } };
  for (const auto &l : land) fillRect(l[0], l[1], l[2], l[3], rgb565(40, 110, 70));
  for (int x = 408; x < 872; x += 29) fillRect(x, 222, x, 461, rgb565(24, 50, 80));
  for (int k = 0; k < 6; k++) fillCircle(470 + k * 70, 280 + (k * 37) % 120, 5, rgb565(255, 60, 40));
  for (int b = 0; b < 2; b++) {                                                // bookshelves
    int x0 = 70 + b * 150, x1 = x0 + 120, top = ROOM_FLOOR_Y - 300;
    fillRect(x0, top, x1, ROOM_FLOOR_Y - 1, rgb565(110, 70, 40));
    for (int sh = 0; sh < 5; sh++) {
      int y = top + 10 + sh * 58;
      fillRect(x0 + 6, y, x1 - 6, y + 48, rgb565(60, 36, 20));
      for (int k = 0; k < 9; k++) {
        static const uint16_t bc[5] = { rgb565(160, 40, 40), rgb565(40, 80, 150), rgb565(200, 170, 80), rgb565(40, 110, 60), rgb565(120, 120, 130) };
        int bx = x0 + 8 + k * 12, h = 30 + (k * 7 + sh * 5) % 16;
        fillRect(bx, y + 48 - h, bx + 9, y + 47, bc[(k + sh * 2) % 5]);
      }
    }
  }
  fillRect(760, ROOM_FLOOR_Y - 80, 1040, ROOM_FLOOR_Y - 68, rgb565(90, 56, 30));   // his desk
  fillRect(770, ROOM_FLOOR_Y - 68, 1030, ROOM_FLOOR_Y - 1, rgb565(110, 70, 40));
  fillRect(880, ROOM_FLOOR_Y - 60, 920, ROOM_FLOOR_Y - 10, rgb565(90, 56, 30));
  fillRect(800, ROOM_FLOOR_Y - 140, 880, ROOM_FLOOR_Y - 80, rgb565(40, 44, 52));
  fillRect(806, ROOM_FLOOR_Y - 134, 874, ROOM_FLOOR_Y - 86, rgb565(120, 20, 20));
  fillRect(960, ROOM_FLOOR_Y - 96, 990, ROOM_FLOOR_Y - 80, rgb565(240, 240, 236));
  fillRect(1150, ROOM_FLOOR_Y - 120, 1156, ROOM_FLOOR_Y - 1, rgb565(150, 120, 60));   // the globe on its stand
  fillRect(1124, ROOM_FLOOR_Y - 6, 1182, ROOM_FLOOR_Y - 1, rgb565(150, 120, 60));
  fillCircle(1153, ROOM_FLOOR_Y - 150, 40, rgb565(50, 110, 180));
  fillCircle(1140, ROOM_FLOOR_Y - 162, 14, rgb565(70, 150, 80));
  fillCircle(1166, ROOM_FLOOR_Y - 136, 10, rgb565(70, 150, 80));
}

// A stage 2 room: white walls, the duct and stripe along the top; then what is in it (the kitchen; the rest empty for now)
static void paintLabRoom() {
  int idx = curRoom;
  fillRect(0, HUD_H, SCREEN_W - 1, CEIL_Y - 1, rgb565(214, 218, 224));
  for (int x = 40; x < SCREEN_W - 40; x += 180) fillRect(x, CEIL_Y - 8, x + 70, CEIL_Y - 4, rgb565(250, 252, 255));
  labStripe(0, SCREEN_W - 1, CEIL_Y, CEIL_Y + 12);
  bool clean = s2Kind[idx] == LK_CLEANING || s2Kind[idx] == LK_STORE;   // (three levels: no duct)
  if (!clean) labDuctBand(0, SCREEN_W - 1, CEIL_Y + 16, CEIL_Y + 58);   // (the cleaning room: its top level up there instead)
  if (s2Kind[idx] == LK_SERVER) serverWall(0, SCREEN_W - 1, CEIL_Y + 61, ROOM_FLOOR_Y - 1);   // (shiny metal, not white tiles)
  else if (s2Kind[idx] == LK_ARMORY || s2Kind[idx] == LK_ARMORY2) armoryWall();               // (dark gunmetal)
  else labWall(0, SCREEN_W - 1, clean ? CEIL_Y + 13 : CEIL_Y + 61, ROOM_FLOOR_Y - 1);
  if (s2Kind[idx] == LK_CLONE) {
    paintCloneLab();
  } else if (s2Kind[idx] == LK_SOLDIER) {
    paintSoldierLab();
  } else if (s2Kind[idx] == LK_SERVER) {
    paintServerRoom();
  } else if (s2Kind[idx] == LK_QLAB1) {
    paintQuantumLab1();
  } else if (s2Kind[idx] == LK_QLAB2) {
    paintQuantumLab2();
  } else if (s2Kind[idx] == LK_ARMORY) {
    paintArmory();
  } else if (s2Kind[idx] == LK_ARMORY2) {
    paintArmory2();
  } else if (s2Kind[idx] == LK_TEST) {
    paintTestRoom();
  } else if (s2Kind[idx] == LK_HAZARD) {
    paintHazStorage();
  } else if (s2Kind[idx] == LK_SIMON) {
    paintSimonRoom();
  } else if (s2Kind[idx] == LK_DESTRUCT) {
    paintDestructRoom();
  } else if (s2Kind[idx] == LK_BOSS) {
    paintBossRoom();
  } else if (s2Kind[idx] == LK_KITCHEN) {
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
  if (s2Kind[idx] == LK_HAZARD) paintHazPuddle();
  if (s2Kind[idx] == LK_QLAB2)                 // (the holes in its floor)
    for (int i = 0; i < pitCount; i++) paintBrokenPit(pits[i][0], pits[i][1]);
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
static int decorAt();
static void drawArmory2Anim();
// Lift B's rooms, moving: bubbles in the clone tanks, the vitals traces, the fans and the big screen's scan
static void drawLiftBRoomAnim(int k) {
  if (k == LK_ARMORY2) {
    drawArmory2Anim();
    return;
  }
  if (k == LK_ARMORY) {   // the blue spotlight down on the case, the glass, the QX-9 turning slowly (till he takes it)
    float pulse = 0.5f + 0.5f * sinf(clockT * 2.2f);
    float bx[4] = { 622, 658, ARMORY_CASE_X1 + 30.0f, ARMORY_CASE_X0 - 30.0f }, by[4] = { CEIL_Y + 88.0f, CEIL_Y + 88.0f, 600, 600 };
    blendPolygonF(bx, by, 4, 90, 170, 255, 0.10f + 0.04f * pulse);
    glowDisc(640, 595, 160, 70, 150, 255, 0.18f + 0.08f * pulse);
    if (!xGunTaken) {
      float bob = 4 * sinf(clockT * 1.6f);
      glowDisc(640, 500 + bob, 110, 80, 170, 255, 0.25f + 0.1f * pulse);
      drawXGun(640, 500 + bob, 0.72f, pulse);
    } else {
      drawVectorText("EMPTY", 640, 500, 2.0f, rgb565(90, 120, 170), true);
    }
    blendRect(ARMORY_CASE_X0, ARMORY_CASE_Y0, ARMORY_CASE_X1, ARMORY_CASE_Y1, 160, 210, 255, 0.12f);   // the glass
    drawLine(ARMORY_CASE_X0 + 16, ARMORY_CASE_Y1 - 10, ARMORY_CASE_X0 + 70, ARMORY_CASE_Y0 + 10, rgb565(210, 236, 255), false);
    drawLine(ARMORY_CASE_X0 + 30, ARMORY_CASE_Y1 - 10, ARMORY_CASE_X0 + 84, ARMORY_CASE_Y0 + 10, rgb565(170, 210, 250), false);
    fillRect(560, 620, 720, 623, pulse > 0.5f ? rgb565(110, 200, 255) : rgb565(60, 140, 230));   // the plinth's light strip
    return;
  }
  if (k == LK_QLAB1) {   // the glove box lit inside
    blendRect(798, ROOM_FLOOR_Y - 140, 916, ROOM_FLOOR_Y - 70, 150, 210, 240, 0.10f + 0.05f * sinf(clockT * 2));
  } else if (k == LK_QLAB2) {   // sparks: the loose cables, the glove box, the terminal; the lights flicker
    static int lastBurst[4] = { -1, -1, -1, -1 };
    for (int s = 0; s < 4; s++) {
      float rate = 0.45f + s * 0.13f, u = clockT * rate + s * 0.37f;
      int burst = (int)u;
      float in = (u - burst) / rate;                       // seconds into this burst's cycle
      if (in > 0.45f) continue;
      if (burst != lastBurst[s]) {
        lastBurst[s] = burst;
        if (s < 2 || ((burst + s) % 3) == 0) sfxPlay(SFX_ZAP);
      }
      int sx = qlab2Spark[s][0], sy = qlab2Spark[s][1];
      uint32_t h = (uint32_t)(burst * 2654435761u + s * 40503u + (int)(clockT * 24) * 97u);
      glowDisc(sx, sy, 34, 255, 230, 140, 0.30f * (1 - in / 0.45f));
      for (int j = 0; j < 7; j++) {
        h = h * 1103515245u + 12345u;
        float a = (h >> 8) % 628 / 100.0f, len = 10 + (h >> 20) % 26;
        if (s < 2) a = a * 0.5f + 0.8f;                    // (a cable's end: mostly down)
        int ex = sx + (int)(cosf(a) * len), ey = sy + (int)(sinf(a) * len);
        drawLine(sx, sy, ex, ey, (j & 1) ? rgb565(255, 250, 220) : rgb565(255, 200, 60), false);
        fillCircle(ex, ey, 1, rgb565(255, 255, 240));
      }
    }
    float f = fmodf(clockT, 5.3f);                         // the lights: a stutter every few seconds
    if ((f > 2.0f && f < 2.08f) || (f > 2.16f && f < 2.3f) || (f > 4.6f && f < 4.66f))
      blendRect(0, CEIL_Y, SCREEN_W - 1, ROOM_FLOOR_Y - 1, 0, 0, 10, 0.35f);
  } else if (k == LK_CLONE) {
    for (int t = 0; t < CLONE_TANKS; t++) {
      int cx = cloneTankX[t];
      float top = CLONE_DECK_Y + (270 - 400) * CLONE_TANK_S + 6, bottom = CLONE_DECK_Y + (384 - 400) * CLONE_TANK_S - 4;
      for (int j = 0; j < 4; j++) {
        float u = fmodf(clockT * (0.22f + j * 0.05f) + t * 0.31f + j * 0.27f, 1.0f);
        float y = bottom - u * (bottom - top), x = cx - 34 + ((t * 23 + j * 19) % 64) + 3 * sinf(clockT * 3 + j + t);
        drawCircle((int)x, (int)y, 2 + (j & 1), rgb565(230, 250, 190));
      }
    }
  } else if (k == LK_SOLDIER) {
    for (int s = 0; s < 2; s++) {
      int x0 = (s ? 775 : 505) - 44, y0 = 460, w = 88;
      float prevY = 0;
      for (int i = 0; i <= w; i += 2) {
        float u = fmodf(i / (float)w + clockT * 0.45f + s * 0.4f, 1.0f), v = fmodf(u * 2.0f, 1.0f);
        float y = (v > 0.45f && v < 0.5f) ? -20.0f : (v > 0.5f && v < 0.55f) ? 12.0f : 0.0f;
        if (i) drawLine(x0 + i - 2, y0 + (int)prevY, x0 + i, y0 + (int)y, rgb565(80, 240, 120), false);
        prevY = y;
      }
    }
    float g = 0.10f + 0.06f * sinf(clockT * 1.7f);       // the big tank's glow, breathing
    glowDisc(640, 420, 200, 160, 255, 90, g);
  } else if (k == LK_SERVER) {
    for (int f = 0; f < SRV_FANS; f++) {
      int x = srvFanX(f), y = SRV_FAN_Y, r = SRV_FAN_R;
      float spin = clockT * 7.0f * ((f & 1) ? 1 : -1) + f;
      for (int b = 0; b < 5; b++) {
        float a = spin + b * TWO_PI / 5, hub = r * 0.22f, tip = r - 3.0f;
        float xs[4] = { x + cosf(a - 0.2f) * hub, x + cosf(a - 0.45f) * tip, x + cosf(a + 0.2f) * tip, x + cosf(a + 0.2f) * hub };
        float ys[4] = { y + sinf(a - 0.2f) * hub, y + sinf(a - 0.45f) * tip, y + sinf(a + 0.2f) * tip, y + sinf(a + 0.2f) * hub };
        fillPolygonF(xs, ys, 4, rgb565(130, 138, 150));
        drawLineF(xs[0], ys[0], xs[1], ys[1], rgb565(190, 196, 206), 1.0f);
      }
      fillCircle(x, y, r / 4, rgb565(70, 76, 88));
      fillCircle(x, y, r / 8, rgb565(180, 186, 196));
      drawCircle(x, y, r * 2 / 3, rgb565(90, 96, 108));   // the guard in front
      drawLine(x - r, y, x + r, y, rgb565(90, 96, 108), true);
      drawLine(x, y - r, x, y + r, rgb565(90, 96, 108), true);
    }
    int sy = SRV_MON_Y0 + 4 + (int)fmodf(clockT * 60, (float)(SRV_MON_Y1 - SRV_MON_Y0 - 8));   // the screen's scan line
    blendRect(SRV_MON_X0 + 1, sy, SRV_MON_X1 - 1, sy + 3, 120, 220, 255, 0.35f);
    for (int t = 0; t < 4; t++) {                           // used ones: LOCKED on their screens
      if (!nodeUsed[t]) continue;
      int cx = serverItems[2 + t].x;
      fillRect(cx - 42, ROOM_FLOOR_Y - 148, cx + 42, ROOM_FLOOR_Y - 102, rgb565(40, 8, 8));
      drawVectorText("LOCKED", (float)cx, ROOM_FLOOR_Y - 125.0f, 1.6f, rgb565(255, 80, 60), true);
    }
    {
      int at = decorAt();
      if (at >= 2 && at <= 5 && agState == ST_IDLE && !termOn) {
        char b[40];
        snprintf(b, sizeof(b), nodeUsed[at - 2] ? "NODE %d - LOCKED" : "NODE %d - HOLD SQUARE: LOG IN", at - 1);
        float w = vectorTextWidth(b, 2.2f), x = clampv((float)serverItems[at].x, w / 2 + 24, SCREEN_W - w / 2 - 24);
        blendRect((int)(x - w / 2 - 10), ROOM_FLOOR_Y - 196, (int)(x + w / 2 + 10), ROOM_FLOOR_Y - 170, 0, 0, 0, 0.6f);
        drawVectorText(b, x, ROOM_FLOOR_Y - 183.0f, 2.2f, nodeUsed[at - 2] ? rgb565(255, 90, 70) : rgb565(255, 220, 120), true);
      }
    }
    for (int t = 0; t < 4; t++)                             // the terminals' cursors
      if (!nodeUsed[t] && (((int)(clockT * 2.5f) + t) & 1)) {
        int cx = serverItems[2 + t].x;
        fillRect(cx + 14, ROOM_FLOOR_Y - 109, cx + 22, ROOM_FLOOR_Y - 105, rgb565(80, 240, 140));
      }
  }
}

// ---- lift E: the CODE ROOM's board, live; the missiles' display (a hologram globe turning, each flight's progress, the dead)
static const float cityLatLon[8][2] = { { -33.9f, 151.2f }, { 23.8f, 90.4f }, { 35.7f, 139.7f }, { 40.7f, -74.0f },
                                        { 31.2f, 121.5f }, { 30.0f, 31.2f }, { 14.6f, 121.0f }, { 37.6f, 127.0f } };   // (cities[]'s order)
static void commas(char *out, int n, uint32_t v);
static int cityOfSite(int i);
static uint32_t deathsSoFar();
static bool inFlight(int i);
static uint16_t simHue(int c, float k) {     // (each square its own colour, k: how bright)
  static const uint8_t hue[8][3] = { { 255, 70, 70 }, { 255, 160, 40 }, { 250, 236, 70 }, { 90, 240, 100 }, { 60, 220, 240 }, { 80, 130, 255 }, { 190, 100, 255 }, { 255, 100, 200 } };
  const uint8_t *h = hue[(c % SIM_COLS + (c / SIM_COLS) * 3) % 8];
  return rgb565((int)(h[0] * k), (int)(h[1] * k), (int)(h[2] * k));
}
static void drawSimonBoard() {
  bool won = simState == SIM_WON && simT < 2.4f, bad = simOn && simState == SIM_BAD, good = simOn && simState == SIM_GOOD;
  for (int r = 0; r < SIM_ROWS; r++)
    for (int c = 0; c < SIM_COLS; c++) {
      int i = r * SIM_COLS + c, x0 = SIM_X0 + c * SIM_CW, y0 = SIM_Y0 + r * SIM_CH;
      uint16_t col = ((r + c) & 1) ? rgb565(34, 40, 62) : rgb565(52, 62, 94);
      bool lit = i == simLit;
      if (won) lit = ((int)(simT * 8) + c + r) % 4 == 0;
      if (bad) col = ((int)(simT * 6) & 1) ? rgb565(120, 20, 20) : col;
      if (good && ((int)(simT * 8) + c) % 3 == 0) col = rgb565(40, 140, 70);
      fillRect(x0 + 3, y0 + 3, x0 + SIM_CW - 3, y0 + SIM_CH - 3, lit ? simHue(i, 1.0f) : col);
      if (lit) glowDisc(x0 + SIM_CW / 2.0f, y0 + SIM_CH / 2.0f, 90, 255, 255, 255, 0.25f);
      else fillRect(x0 + 3, y0 + 3, x0 + SIM_CW - 3, y0 + 6, ((r + c) & 1) ? rgb565(50, 58, 86) : rgb565(70, 82, 120));
    }
  if (simOn && simState == SIM_INPUT) {      // the cursor
    int x0 = SIM_X0 + simCurX * SIM_CW, y0 = SIM_Y0 + simCurY * SIM_CH;
    uint16_t cc = ((int)(clockT * 4) & 1) ? rgb565(255, 240, 120) : rgb565(255, 190, 60);
    drawRectOutline(x0 + 1, y0 + 1, x0 + SIM_CW - 1, y0 + SIM_CH - 1, cc, 5);
  }
  // what's going on, on a plaque over the board
  char b[64];
  uint16_t tc = rgb565(240, 244, 250);
  if (simOn) {
    if (simState == SIM_SHOW) snprintf(b, sizeof(b), "ROUND %d OF %d - WATCH THE SEQUENCE", simRound + 1, SIM_ROUNDS);
    else if (simState == SIM_INPUT) snprintf(b, sizeof(b), "ROUND %d OF %d - YOUR TURN  %d / %d", simRound + 1, SIM_ROUNDS, simPos, 3 + simRound);
    else if (simState == SIM_GOOD) { snprintf(b, sizeof(b), simRound + 1 < SIM_ROUNDS ? "CORRECT!" : "SEQUENCE COMPLETE"); tc = rgb565(100, 255, 140); }
    else { snprintf(b, sizeof(b), "WRONG - WATCH AGAIN"); tc = rgb565(255, 90, 70); }
  } else if (s2NukeCode) {
    snprintf(b, sizeof(b), "SELF DESTRUCT CODE HELD - TAKE IT TO THE ROOM BELOW");
    tc = rgb565(255, 220, 120);
  } else {
    snprintf(b, sizeof(b), "NUKE SELF DESTRUCT CODE - REPEAT 3 SEQUENCES");
  }
  float w = vectorTextWidth(b, 2.0f), cx = SIM_X0 + SIM_COLS * SIM_CW / 2.0f;
  blendRect((int)(cx - w / 2 - 12), SIM_Y0 - 34, (int)(cx + w / 2 + 12), SIM_Y0 - 12, 0, 0, 0, 0.75f);
  drawVectorText(b, cx, SIM_Y0 - 23.0f, 2.0f, tc, true);
  // the terminal's screen
  int sx = SIM_TERM_X, sy = ROOM_FLOOR_Y - 162;
  if (simOn) {
    for (int k = 0; k < 3 + simRound; k++)
      fillCircle(sx - (3 + simRound - 1) * 9 + k * 18, sy, 6, k < simPos || simState == SIM_GOOD ? rgb565(100, 255, 140) : rgb565(40, 70, 90));
    drawVectorText("D-PAD: MOVE   X: PICK   SQUARE: QUIT", 480, ROOM_FLOOR_Y + 20.0f, 1.8f, rgb565(255, 230, 140), true);
  } else {
    drawVectorText(s2NukeCode ? "CODE" : "READY", (float)sx, (float)sy, 1.6f, ((int)(clockT * 2) & 1) ? rgb565(100, 230, 255) : rgb565(50, 120, 150), false);
    if (decorAt() == 0 && agState == ST_IDLE && !termOn) {
      const char *t = s2NukeCode ? "CODE HELD - USE IT IN THE ROOM BELOW" : "HOLD SQUARE: PLAY";
      float tw = vectorTextWidth(t, 2.2f);
      blendRect((int)(SIM_TERM_X - tw / 2 - 10), ROOM_FLOOR_Y - 236, (int)(SIM_TERM_X + tw / 2 + 10), ROOM_FLOOR_Y - 210, 0, 0, 0, 0.6f);
      drawVectorText(t, SIM_TERM_X, ROOM_FLOOR_Y - 223.0f, 2.2f, rgb565(255, 220, 120), true);
    }
  }
}
static void drawMissileDisplay() {
  float cx = (MD_X0 + MD_X1) / 2.0f, cy = MD_Y0 + 100.0f, r = 72, rot = clockT * 0.45f, tilt = 0.4f;
  float ct = cosf(tilt), st = sinf(tilt);
  auto proj = [&](float lat, float lon, float &x, float &y, float &z) {
    float cl = cosf(lat), px = cl * sinf(lon + rot), py = sinf(lat), pz = cl * cosf(lon + rot);
    x = cx + px * r;
    y = cy - (py * ct - pz * st) * r;
    z = py * st + pz * ct;
  };
  // the projector's beam, the glow
  float bx[4] = { cx - 26, cx + 26, cx + r, cx - r }, by[4] = { MD_Y0 + 196.0f, MD_Y0 + 196.0f, cy, cy };
  blendPolygonF(bx, by, 4, 80, 200, 255, 0.07f);
  glowDisc(cx, cy, r * 1.5f, 60, 190, 255, 0.16f + 0.04f * sinf(clockT * 3));
  const float D2R = 0.0174533f;
  auto seg = [&](float la0, float lo0, float la1, float lo1) {
    float x0, y0, z0, x1, y1, z1;
    proj(la0, lo0, x0, y0, z0);
    proj(la1, lo1, x1, y1, z1);
    bool front = z0 + z1 > 0;
    drawLineF(x0, y0, x1, y1, front ? rgb565(110, 230, 255) : rgb565(30, 80, 110), front ? 2.0f : 1.0f);
  };
  for (int la = -60; la <= 60; la += 30)       // the latitudes
    for (int k = 0; k < 24; k++) seg(la * D2R, k * 15 * D2R, la * D2R, (k + 1) * 15 * D2R);
  for (int m = 0; m < 8; m++)                  // the meridians, turning
    for (int k = 0; k < 12; k++) seg((-90 + k * 15) * D2R, m * 45 * D2R, (-90 + (k + 1) * 15) * D2R, m * 45 * D2R);
  for (int i = 0; i < sitesLaunched; i++) {    // the cities hit: red, pulsing
    if (prevented[i] || nukeDestroyed[i] || landLeft[i] > 0) continue;
    float x, y, z;
    proj(cityLatLon[cityOfSite(i) % 8][0] * D2R, cityLatLon[cityOfSite(i) % 8][1] * D2R, x, y, z);
    if (z < 0) continue;
    glowDisc(x, y, 16 + 4 * sinf(clockT * 6), 255, 60, 40, 0.6f);
    fillCircle((int)x, (int)y, 4, rgb565(255, 80, 60));
  }
  float sy = MD_Y0 + 6 + fmodf(clockT * 50, 200.0f);   // (a scan line)
  blendRect(MD_X0 + 8, (int)sy, MD_X1 - 8, (int)sy + 2, 120, 220, 255, 0.25f);
  // the missiles
  drawVectorText("NUCLEAR MISSILES", cx, MD_Y0 + 240.0f, 1.8f, rgb565(255, 220, 80), true);
  int landedN = 0;
  for (int i = 0; i < NUKE_SITES; i++) {
    int y = MD_Y0 + 266 + i * 42, x0 = MD_X0 + 14, x1 = MD_X1 - 14;
    drawTextLeft(siteName[i], (float)x0, (float)y, 1.25f, rgb565(200, 210, 230), false);
    int b0 = y + 14, b1 = y + 30;
    fillRect(x0, b0, x1, b1, rgb565(30, 34, 46));
    const char *tag = NULL;
    uint16_t tagC = rgb565(240, 244, 250);
    bool launched = i < sitesLaunched && !prevented[i];
    if (prevented[i]) { fillRect(x0, b0, x1, b1, rgb565(40, 120, 70)); tag = "STOPPED"; }
    else if (nukeDestroyed[i]) { fillRect(x0, b0, x1, b1, rgb565(50, 170, 90)); tag = "DESTROYED"; }
    else if (launched && landLeft[i] <= 0) { fillRect(x0, b0, x1, b1, rgb565(200, 30, 24)); tag = "LANDED"; landedN++; }
    else if (launched) {                       // in the air: how far it's got
      float k = landTotal[i] > 0 ? clampv(1.0f - landLeft[i] / landTotal[i], 0.0f, 1.0f) : 0;
      int w = (int)((x1 - x0) * k);
      fillRect(x0, b0, x0 + w, b1, k > 0.8f && ((int)(clockT * 4) & 1) ? rgb565(255, 60, 40) : rgb565(250, 140, 30));
      tag = "IN FLIGHT";
    } else {
      tag = "NOT LAUNCHED";
      tagC = rgb565(110, 120, 140);
    }
    drawRectOutline(x0, b0, x1, b1, rgb565(110, 118, 140), 1);
    if (tag) drawVectorText(tag, (x0 + x1) / 2.0f, (b0 + b1) / 2.0f, 1.0f, tagC, false);
  }
  char b[48], num[24];
  snprintf(b, sizeof(b), "LANDED: %d", landedN);
  drawVectorText(b, cx, MD_Y1 - 50.0f, 1.7f, landedN ? rgb565(255, 90, 70) : rgb565(200, 210, 230), true);
  commas(num, sizeof(num), deathsSoFar());
  snprintf(b, sizeof(b), "DEAD: %s", num);
  drawVectorText(b, cx, MD_Y1 - 22.0f, 1.7f, deathsSoFar() ? rgb565(255, 90, 70) : rgb565(200, 210, 230), true);
}
static void drawEndRoomsBack(int k) {
  if (k == LK_SIMON) {
    drawSimonBoard();
    drawMissileDisplay();
  } else if (k == LK_DESTRUCT) {               // the console's screen; what Square does there
    if ((int)(clockT * 2) & 1) fillCircle(DS_CONSOLE_X + 70, ROOM_FLOOR_Y - 60, 14, rgb565(255, 70, 60));
    for (int i = 0; i < NUKE_SITES; i++) {     // the wall screen: a blip for each one in the air
      if (!inFlight(i)) continue;
      float k2 = landTotal[i] > 0 ? 1.0f - landLeft[i] / landTotal[i] : 0;
      float x = 500 + 280 * k2, y = 400 - 140 * sinf(k2 * 3.14159f) - i * 8;
      glowDisc(x, y, 14, 255, 120, 40, 0.6f);
      fillCircle((int)x, (int)y, 4, rgb565(255, 160, 60));
    }
    if (decorAt() == 0 && agState == ST_IDLE && !termOn) {
      const char *t = s2NukeCode ? "HOLD SQUARE: ENTER THE SELF DESTRUCT CODE" : "SELF DESTRUCT - A NUKE CODE IS NEEDED";
      float tw = vectorTextWidth(t, 2.2f);
      blendRect((int)(DS_CONSOLE_X - tw / 2 - 10), ROOM_FLOOR_Y - 236, (int)(DS_CONSOLE_X + tw / 2 + 10), ROOM_FLOOR_Y - 210, 0, 0, 0, 0.6f);
      drawVectorText(t, DS_CONSOLE_X, ROOM_FLOOR_Y - 223.0f, 2.2f, s2NukeCode ? rgb565(255, 220, 120) : rgb565(255, 90, 70), true);
    }
  }
}

static void drawLabRoomBack() {
  int k = s2Kind[curRoom];
  if (k == LK_SIMON || k == LK_DESTRUCT) {
    drawEndRoomsBack(k);
    return;
  }
  if (k == LK_CLONE || k == LK_SOLDIER || k == LK_SERVER || k == LK_QLAB1 || k == LK_QLAB2 || k == LK_ARMORY || k == LK_ARMORY2) {
    drawLiftBRoomAnim(k);
    return;
  }
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
static void drawFoyerGuardAt(int sh, int L, int fy);
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
    if ((curShaft == 2 || curShaft == 3) && L < fgLevels[curShaft - 2] && (curShaft - 2) * 8 + L != fgLoaded) drawFoyerGuardAt(curShaft - 2, L, fy);
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
static const WideRoom cloneT = { "CLONE LAB", 2, { { 0, SCREEN_W, ROOM_FLOOR_Y }, { 220, SCREEN_W, CLONE_DECK_Y } },
                                 1, { { 140, 220, { CLONE_DECK_Y, ROOM_FLOOR_Y }, 2 } }, 0, {}, 0, {}, 640, 0 };
static const WideRoom qlab1T = { "QUANTUM LAB 1", 2, { { 0, SCREEN_W, ROOM_FLOOR_Y }, { 760, SCREEN_W, 440 } },
                                 1, { { 680, 760, { 440, ROOM_FLOOR_Y }, 2 } }, 0, {}, 0, {}, 640, 0 };
static const WideRoom qlab2T = { "QUANTUM LAB 2", 3, { { 0, 420, ROOM_FLOOR_Y }, { 530, 800, ROOM_FLOOR_Y }, { 910, SCREEN_W, ROOM_FLOOR_Y } },
                                 0, {}, 2, { { 420, 530 }, { 800, 910 } }, 0, {}, 640, 0 };
static const WideRoom &roomTemplate(int idx) {
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_CLONE) return cloneT;
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_QLAB1) return qlab1T;
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_QLAB2) return qlab2T;
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
  drWakeT = 0;
  gasT = -1.5f;                        // (the clone lab's gas: a moment before the first step)
  hzAcc = 0;                           // (hazardous storage)
  hzPuddleCool = 0;
  hzDripHit = -1;
  hzLastX = fromLeft ? 0.0f : (float)SCREEN_W;
  gasHitStep = -1;
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
static float s2Countdown(bool *impact);

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
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_ARENA) arenaEnter(fromLeft);
  // the lab's people: on their paths, walking away from the door he came in by
  npcCount = 0;
  for (int k = 0; k < MY_SHOTS; k++) myShots[k].on = false;
  susp = 0;
  suspT = 0;
  s2Recog = false;
  for (int k = 0; k < 4; k++) gShots[k].on = false;
  simOn = false;
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
    n.foyer = false;
    n.rsold = false;
    n.lvl = -1;
    n.fstate = 0;
    n.fcool = 0;
    n.fxT = 0;
    int slot = npcCount - 1;
    n.scale = (idx < S2_ROOMS) ? s2NpcScale[idx][slot] : 1.0f;
    n.burn = false;
    if (s2NpcDead[idx][slot]) {              // killed: where he fell (or burnt: nothing)
      n.dead = true;
      n.burn = s2NpcAsh[idx][slot];
      if (n.burn) n.dieT = 99;
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
  if (stage == 2 && idx < S2_ROOMS && (s2Kind[idx] == LK_ARMORY || s2Kind[idx] == LK_ARMORY2)) {   // two scientists walking, either side of the case
    addNpc(PK_SCIENTIST, ROOM_FLOOR_Y, 160, 620, 120);
    addNpc(PK_SENIOR, ROOM_FLOOR_Y, 660, 1150, 300);
  }
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_TEST) {      // three test subjects milling about
    addNpc(PK_SUBJECT, ROOM_FLOOR_Y, 200, 620, 60);
    addNpc(PK_SUBJECT, ROOM_FLOOR_Y, 460, 900, 260);
    addNpc(PK_SUBJECT, ROOM_FLOOR_Y, 720, 1150, 120);
  }
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_QLAB1) {     // three scientists: at the glove box, walking the floor, on the deck
    addNpc(PK_SCIENTIST, ROOM_FLOOR_Y, 857, 857, 0);
    if (!npcs[npcCount - 1].dead) npcs[npcCount - 1].x = 857;
    addNpc(PK_SCIENTIST, ROOM_FLOOR_Y, 200, 1180, 380);
    addNpc(PK_SCIENTIST, 440, 800, 1220, 120);
  }
  if (stage == 2 && idx < S2_ROOMS && (s2Kind[idx] == LK_SIMON || s2Kind[idx] == LK_DESTRUCT)) {   // lift E: two soldiers, twice the size
    const float posts[2][2] = { { 240, 760 }, { 450, 830 } };
    int w = s2Kind[idx] == LK_DESTRUCT;
    for (int k = 0; k < 2; k++) {
      float px = fromLeft ? posts[w][k] : SCREEN_W - posts[w][k];
      addNpc(PK_GUARD, ROOM_FLOOR_Y, px, px, 0);
      Npc &g = npcs[npcCount - 1];
      if (!g.dead) { g.x = px; g.dir = fromLeft ? -1 : 1; }   // (facing the door he comes in by)
      g.turns = false;
      g.rsold = true;
      g.sight = 2000;
      g.fcool = FG_GRACE + 0.3f + k * 0.45f;
    }
  }
  if (stage == 2 && idx < S2_ROOMS && s2Kind[idx] == LK_BOSS)        // THE PROFESSOR, twice the size (no weapon, for now)
    addNpc(PK_PROFESSOR, ROOM_FLOOR_Y, 300, 1060, 380);
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

static void computeFinalToll();
static void goChoose(int why);
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
    computeFinalToll();
    goChoose(why);
  }
}

// The bonuses (end of a stage, end of the game): minutes left, and each thing
// that never happened (since the last stage ended)
static void computeBonuses() {
  int mins = timeLeft > 0 ? (int)(timeLeft / 60) : 0;
  bonusPts[0] = (long)mins * BONUS_PER_MINUTE;
  bonusPts[1] = nShocks == 0 ? BONUS_CLEAN : 0;
  if (stage == 2 && scene != SC_STAGE_CLEAR) {   // stage 2's end: each missile stopped or destroyed, 5000 (no time, no electrocution)
    int n = 0;
    for (int i = 0; i < NUKE_SITES; i++) n += prevented[i] || nukeDestroyed[i];
    bonusPts[0] = (long)n * MISSILE_BONUS;
    bonusPts[1] = 0;
  }
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
static void s2Penalty(float secs);
static void loseTime(int secs, const char *what) {
  char b[64];
  if (stage == 2) {                          // stage 2: no clock of his own - the next launch (or impact) comes sooner
    bool impact;
    float left = s2Countdown(&impact);
    s2Penalty((float)secs);
    if (left >= 0) snprintf(b, sizeof(b), "%s  %s -%d SECONDS", what, impact ? "IMPACT" : "LAUNCH", secs);
    else snprintf(b, sizeof(b), "%s", what);
    showMsg(b, 2.6f);
    return;
  }
  timeLeft -= secs;
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
static void flyerAdd(const char *text, bool good = false);
// LOW HEALTH: at 40, 30, 20 and 10% a red "40% HEALTH" flies out at him (like a hack's), once each time he
// falls to it; from 40% down a red glow pulses round him with his heartbeat, quicker the lower he is, and
// from 20% down the screen's edges pulse red and the heartbeat is heard
static int hpBand = 100;                     // (the lowest 10% mark announced since he was last above it)
static float hpBeatT = 0, hpBeat = 0;        // (time to the next beat; the last beat's glow, fading)
static int healthPct() { return health * 100 / HEALTH_MAX; }
static void updateHealthAlerts(float dt) {
  int pct = healthPct();
  int band = pct <= 10 ? 10 : pct <= 20 ? 20 : pct <= 30 ? 30 : pct <= 40 ? 40 : 100;
  if (band < hpBand && health > 0 && gameOver == END_NONE) {
    char b[24];
    snprintf(b, sizeof(b), "%d%% HEALTH", band);
    flyerAdd(b);
    sfxPlay(SFX_HPALERT);
  }
  hpBand = band;
  hpBeat = fmaxf(0.0f, hpBeat - dt * 2.5f);
  if (pct > 40 || health <= 0 || gameOver != END_NONE) { hpBeatT = 0; hpBeat = 0; return; }
  hpBeatT -= dt;
  if (hpBeatT <= 0) {
    hpBeatT = 0.45f + 0.6f * pct / 40.0f;    // (1.05s at 40%, 0.6s at 10%)
    hpBeat = 1;
    if (pct <= 20) sfxPlay(SFX_HEART);
  }
}
static void drawHealthAura() {   // (behind him)
  if (hpBeat <= 0 || healthPct() > 40) return;
  float k = 1.0f - healthPct() / 40.0f;      // (stronger the lower)
  glowDisc(agX, agY - 70 * myScale, 120 * myScale, 255, 30, 30, (0.35f + 0.4f * k) * hpBeat);
}
static void drawHealthEdges() {  // (over everything: the screen's edges, from 20% down)
  if (hpBeat <= 0 || healthPct() > 20) return;
  float a = 0.35f * hpBeat;
  for (int i = 0; i < 6; i++) {
    float f = a * (1.0f - i / 6.0f);
    int w = 8;
    blendRect(i * w, HUD_H, i * w + w - 1, PLAY_Y1, 255, 20, 20, f);
    blendRect(SCREEN_W - 1 - i * w - w + 1, HUD_H, SCREEN_W - 1 - i * w, PLAY_Y1, 255, 20, 20, f);
    blendRect(0, HUD_H + i * w, SCREEN_W - 1, HUD_H + i * w + w - 1, 255, 20, 20, f);
    blendRect(0, PLAY_Y1 - i * w - w + 1, SCREEN_W - 1, PLAY_Y1 - i * w, 255, 20, 20, f);
  }
}

static void flyerAdd(const char *text, bool good) {   // (a free one, else the oldest)
  int k = 0;
  for (int i = 1; i < FLYERS; i++) if (flyers[i].t > flyers[k].t) k = i;
  for (int i = 0; i < FLYERS; i++) if (!flyers[i].on) { k = i; break; }
  snprintf(flyers[k].text, sizeof(flyers[k].text), "%s", text);
  flyers[k].t = 0;
  flyers[k].on = true;
  flyers[k].good = good;
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
    drawVectorTextA(flyers[i].text, SCREEN_W / 2.0f, (HUD_H + PLAY_Y1) / 2.0f, size, flyers[i].good ? 60 : 255, flyers[i].good ? 230 : 40, flyers[i].good ? 110 : 30, 0.85f * a, fmaxf(1.5f, size * 0.55f));
  }
  gfxClipAll();
}

// ============================================================ a launch: NUCLEAR MISSION's launch cut scene
// When a site launches, its cut scene plays in a square on the right while the game carries on,
// shrunk, on the left. NUCLEAR MISSION's scene is 720 x 720 pixels: it is drawn 1:1 into the square
// (pixels 1200..1919 x 180..899). Scene 1: the bottom of the silo (the klaxon, ignition, the fire
// rolling out); a white flash; scene 3: above ground at night, up out of the silo and away. The
// site's name is on a sign in both, and typed out at the end.
#define LS_X 1200
#define LS_Y 180
#define LS_SIZE 720
#define LS_SCENE1_SECONDS 5.4f
#define LS_SCENE3_SECONDS 9.0f
#define LS_GROUND 560
#define LS_FIRE 40
struct LsFire { bool on; float x, y, vx, vy, r, age, life; };
static LsFire lsFire[LS_FIRE];
static int launchSite = 0, launchQueue[NUKE_SITES * 2];   // (queued: a launch is its site, a landing 8 + its site)
static int lsScene = 0, lsTyped = 0;
static float lsT = 0, lsSoundT = 0, lsX = 0, lsY = 0, lsAng = 0, lsDir = 1, lsSpeed = 0, lsScale = 1.6f;
static uint32_t frameSave[FB_W * FB_H];       // (the game's frame, to shrink beside the scene)

static int lsRand(int lo, int hi) { return fxRnd(lo, hi); }   // lo..hi-1
// (NUCLEAR MISSION's pixels -> the square)
static void lsRect(int x0, int y0, int x1, int y1, uint16_t c) {
  x0 = clampv(x0, 0, LS_SIZE - 1); x1 = clampv(x1, 0, LS_SIZE - 1);
  y0 = clampv(y0, 0, LS_SIZE - 1); y1 = clampv(y1, 0, LS_SIZE - 1);
  if (x1 < x0 || y1 < y0) return;
  pxRect(LS_X + x0, LS_Y + y0, LS_X + x1, LS_Y + y1, c32(c));
}
static void lsPoly(const int *xs, const int *ys, int n, uint16_t c) {
  float fx[12], fy[12];
  for (int i = 0; i < n && i < 12; i++) { fx[i] = (float)(LS_X + xs[i]); fy[i] = (float)(LS_Y + ys[i]); }
  pxPoly(fx, fy, n, c32(c));
}
static void lsCircle(float x, float y, float r, uint16_t c) { pxCircle(LS_X + x, LS_Y + y, r, c32(c)); }
static void lsLine(float x0, float y0, float x1, float y1, uint16_t c, bool thick) { pxLine(LS_X + x0, LS_Y + y0, LS_X + x1, LS_Y + y1, thick ? 3.0f : 1.5f, c32(c)); }
static void lsText(const char *t, float x, float y, float size, uint16_t c, bool thick) {
  drawVectorText(t, (LS_X + x) / UNIT_K, (LS_Y + y) / UNIT_K, size / UNIT_K, c, thick);
}
static void lsTint(float x, float y, float r, int cr, int cg, int cb, float a) {   // a see-through disc
  float xs[20], ys[20];
  for (int i = 0; i < 20; i++) {
    float t = i * 6.2832f / 20;
    xs[i] = (LS_X + x + cosf(t) * r) / UNIT_K;
    ys[i] = (LS_Y + y + sinf(t) * r) / UNIT_K;
  }
  blendPolygonF(xs, ys, 20, cr, cg, cb, a);
}
static void lsHazard(int x0, int x1, int y0, int y1) {     // yellow and black
  lsRect(x0, y0, x1, y1, rgb565(240, 196, 40));
  for (int x = x0 - (y1 - y0); x < x1; x += 24) {
    int xs[4] = { x, x + 12, x + 12 + (y1 - y0), x + (y1 - y0) }, ys[4] = { y1, y1, y0, y0 };
    for (int k = 0; k < 4; k++) xs[k] = clampv(xs[k], x0, x1);
    lsPoly(xs, ys, 4, rgb565(20, 20, 22));
  }
}
static void lsAddFire(float x, float y, float vx, float vy, float r, float life) {
  for (int i = 0; i < LS_FIRE; i++)
    if (!lsFire[i].on) { lsFire[i] = { true, x, y, vx, vy, r, 0, life }; return; }
}


// ============================================================ a landing: the city hit
// When a missile lands, a city (at random, a different one each time) is hit: in the same square beside
// the game, the city's skyline at night, the warhead falling, the flash, the fireball, the shock wave
// flattening it, the mushroom cloud rising, fires; the city's name and its death toll counting up.
struct City { const char *name, *country; uint32_t lo, hi; };
static const City cities[] = {
  { "SYDNEY", "AUSTRALIA", 100000, 500000 },       { "DHAKA", "BANGLADESH", 2500000, 5000000 },
  { "TOKYO", "JAPAN", 2000000, 3000000 },          { "NEW YORK", "AMERICA", 600000, 700000 },
  { "SHANGHAI", "CHINA", 2900000, 4000000 },       { "CAIRO", "EGYPT", 2500000, 5000000 },
  { "MANILA", "PHILIPPINES", 2500000, 5000000 },   { "SEOUL", "SOUTH KOREA", 1500000, 2500000 } };
#define N_CITIES ((int)(sizeof(cities) / sizeof(cities[0])))
#define LD_SECONDS 12.0f
#define LD_FALL 1.6f                   // (the warhead falling; then the flash)
#define LD_GZ 360                      // ground zero, in the square
static int landCity[NUKE_SITES];
static uint32_t landDeaths[NUKE_SITES], deathTotal = 0;
static bool cityUsed[N_CITIES];
static int lsKind = 0;                 // the cut scene playing: 0 a launch, 1 a landing
static bool lsNoNames = false;         // (the title's and the end's cut scenes: no names, no words)
static int ldSite = 0, ldBuildings = 0;
static int16_t ldBX[32], ldBW[32], ldBH[32];

static void pickCity(int site);
static void resetStrikes() {               // (each site's city and its toll, chosen at the start of the game)
  deathTotal = 0;
  memset(cityUsed, 0, sizeof(cityUsed));
  for (int i = 0; i < NUKE_SITES; i++) pickCity(i);
}

static void pickCity(int site) {       // (one not hit yet)
  int left = 0;
  for (int c = 0; c < N_CITIES; c++) left += !cityUsed[c];
  if (left == 0) memset(cityUsed, 0, sizeof(cityUsed));
  int c;
  do c = (int)(rnd32() % N_CITIES); while (cityUsed[c]);
  cityUsed[c] = true;
  landCity[site] = c;
  const City &ct = cities[c];
  landDeaths[site] = ct.lo + rnd32() % (ct.hi - ct.lo + 1);
}

// The game over (or won): the toll. Every missile not stopped or destroyed lands - the ones in the air, the ones
// still to launch, and (unless QUANTUM LOCKDOWN was set) the sites not yet hacked
static uint32_t finalDeaths = 0;
static int finalHits = 0;
static void computeFinalToll() {
  finalDeaths = 0;
  finalHits = 0;
  for (int i = 0; i < NUKE_SITES; i++) {
    if (prevented[i] || nukeDestroyed[i]) continue;
    if (lockdown && i >= sitesHacked) continue;   // (never hacked: never launched)
    finalHits++;
    finalDeaths += landDeaths[i];
  }
}

static void commas(char *out, int n, uint32_t v) {   // 2,512,904
  char t[16];
  snprintf(t, sizeof(t), "%u", v);
  int len = (int)strlen(t), o = 0;
  for (int i = 0; i < len && o < n - 1; i++) {
    out[o++] = t[i];
    if ((len - 1 - i) % 3 == 0 && i < len - 1 && o < n - 1) out[o++] = ',';
  }
  out[o] = 0;
}

static void startLandScene(int site) {
  ldSite = site;
  lsT = 0;
  lsSoundT = 0;
  lsTyped = 0;
  for (int i = 0; i < LS_FIRE; i++) lsFire[i].on = false;
  uint32_t h = 2166136261u ^ (uint32_t)(landCity[site] * 7919 + 13);   // the city's skyline (the same each time for that city)
  int x = -10;
  ldBuildings = 0;
  while (x < LS_SIZE && ldBuildings < 32) {
    h = h * 1103515245u + 12345u;
    int w = 34 + (h >> 16) % 46;
    h = h * 1103515245u + 12345u;
    int dist = abs(x + w / 2 - LD_GZ);
    int ht = 70 + (h >> 16) % 150 + (dist < 200 ? (200 - dist) * 3 / 4 : 0);   // (taller towards the middle)
    ldBX[ldBuildings] = (int16_t)x;
    ldBW[ldBuildings] = (int16_t)w;
    ldBH[ldBuildings] = (int16_t)ht;
    ldBuildings++;
    x += w + 2 + (h >> 24) % 8;
  }
  sfxPlay(SFX_FALL);
}

static void updateLandScene(float dt) {
  float was = lsT;
  lsT += dt;
  for (int i = 0; i < LS_FIRE; i++) {           // (the debris: up, out and falling)
    LsFire &f = lsFire[i];
    if (!f.on) continue;
    f.age += dt;
    if (f.age >= f.life) { f.on = false; continue; }
    f.x += f.vx * dt;
    f.y += f.vy * dt;
    f.vy += 160 * dt;
  }
  if (was < LD_FALL && lsT >= LD_FALL) {        // detonation
    sfxPlay(SFX_BOOM);
    sfxPlay(SFX_BIGROAR);
    lsSoundT = 0.3f;
    for (int i = 0; i < 26; i++) {
      float a = -3.0f + (i % 13) * 0.115f + (i / 13) * 0.05f, sp = 160 + (i * 37) % 220;
      lsAddFire((float)LD_GZ, (float)LS_GROUND - 20, cosf(a) * sp, sinf(a) * sp, 3 + i % 4, 2.4f + (i % 5) * 0.3f);
    }
  }
  if (lsT > LD_FALL && lsT < LD_FALL + 5) {     // the roar dying away
    lsSoundT -= dt;
    if (lsSoundT <= 0) { lsSoundT = 0.35f; sfxPlay(lsT < LD_FALL + 2 ? SFX_BIGROAR : SFX_ROAR); }
  }
  if (lsT > LD_FALL + 1.0f) {                   // the words: typed on
    int want = (int)((lsT - LD_FALL - 1.0f) * 18);
    if (want > lsTyped) { lsTyped = want; if (lsTyped < 40) sfxPlay(SFX_TYPE); }
  }
}

// A building: whole (lit windows), or after the shock wave a burning ruin
static void ldBuilding(int i, bool ruined, float t, float glow) {
  int x0 = ldBX[i], w = ldBW[i], h = ldBH[i], g = LS_GROUND;
  if (!ruined) {
    int v = 18 + (i * 5) % 14;
    lsRect(x0, g - h, x0 + w, g, rgb565(v, v + 4, v + 16));
    if (glow > 0) lsRect(x0, g - h, x0 + w, g, rgb565((int)(v + 180 * glow), (int)(v + 120 * glow), (int)(v + 60 * glow)));
    for (int y = g - h + 10; y < g - 10; y += 14)                      // windows
      for (int x = x0 + 6; x < x0 + w - 8; x += 11) {
        uint32_t hh = (uint32_t)(x * 31 + y * 17 + i * 101);
        if ((hh * 2654435761u >> 28) < 7) lsRect(x, y, x + 4, y + 6, rgb565(250, 220, 120));
      }
    if (i % 3 == 0) lsRect(x0 + w / 2 - 1, g - h - 18, x0 + w / 2 + 1, g - h, rgb565(80, 84, 96));   // an aerial
    return;
  }
  int top = g - h * 2 / 5;                                             // a jagged stump, black, glowing at its edges
  int xs[6] = { x0, x0 + w / 4, x0 + w / 2, x0 + w * 3 / 4, x0 + w, x0 }, ys[6] = { top + 16, top, top + 22, top + 6, g, g };
  int px[6], py[6];
  for (int k = 0; k < 5; k++) { px[k] = xs[k]; py[k] = ys[k]; }
  px[4] = x0 + w; py[4] = g; px[5] = x0; py[5] = g;
  lsPoly(px, py, 6, rgb565(24, 14, 12));
  for (int k = 0; k < 2; k++) {                                        // its fires
    float fx = x0 + w * (0.3f + 0.4f * k), fy = top + 8;
    float fl = 14 + 8 * sinf(t * 13 + i * 3 + k * 2) + lsRand(0, 6);
    int tx[3] = { (int)fx - 9, (int)fx + 9, (int)(fx + 3 * sinf(t * 9 + i)) }, ty[3] = { (int)fy, (int)fy, (int)(fy - fl) };
    lsPoly(tx, ty, 3, rgb565(255, 120, 20));
    int ix[3] = { (int)fx - 4, (int)fx + 4, (int)fx }, iy[3] = { (int)fy, (int)fy, (int)(fy - fl * 0.55f) };
    lsPoly(ix, iy, 3, rgb565(255, 230, 90));
  }
}

static void drawLandScene() {
  gfxClip((int)(LS_Y / UNIT_K), (int)((LS_Y + LS_SIZE - 1) / UNIT_K));
  float t = lsT, b = t - LD_FALL;                                      // b: since the detonation
  const City &ct = cities[landCity[ldSite]];
  // the sky: night, then lit red and orange
  float burn = b < 0 ? 0 : fminf(1.0f, b / 2.5f);
  for (int y = 0; y < LS_GROUND; y += 8) {
    float k = y / (float)LS_GROUND;
    int r = (int)(8 + 20 * k + burn * (120 + 90 * k)), gg = (int)(10 + 14 * k + burn * (30 + 60 * k)), bb = (int)(30 + 30 * k - burn * 20);
    lsRect(0, y, LS_SIZE - 1, y + 7, rgb565(clampv(r, 0, 255), clampv(gg, 0, 255), clampv(bb, 0, 255)));
  }
  if (b < 0)
    for (int s = 0; s < 40; s++) lsRect((s * 197) % LS_SIZE, (s * 131) % 300, (s * 197) % LS_SIZE + 1, (s * 131) % 300 + 1, rgb565(220, 220, 240));
  // the shock wave and what it has flattened
  float shock = b < 0 ? -1 : 140 + 420 * b;
  float glow = b < 0 ? 0 : fmaxf(0, 0.9f - b * 0.6f);
  // the mushroom cloud (behind the near buildings): stem, cap, its ring
  if (b > 0) {
    float rise = fminf(1.0f, b / 6.0f), ease = 1 - (1 - rise) * (1 - rise);
    float capY = LS_GROUND - 60 - 300 * ease, capR = 70 + 70 * ease;
    float stemW = 26 + 22 * ease;
    for (int y = (int)capY; y < LS_GROUND; y += 6) {                   // the stem: brighter low down, churning
      float k = (y - capY) / (LS_GROUND - capY), w = stemW * (0.75f + 0.5f * k) + 4 * sinf(y * 0.08f + t * 5);
      int r = (int)(150 + 90 * k), gg = (int)(70 + 70 * k), bl = 30;
      lsRect(LD_GZ - (int)w, y, LD_GZ + (int)w, y + 5, rgb565(r, gg, bl));
    }
    for (int ring = 0; ring < 3; ring++) {                             // the cap: dark smoke outside, fire inside
      float rr = capR * (1.0f - ring * 0.28f);
      int col[3][3] = { { 90, 60, 50 }, { 210, 110, 40 }, { 255, 210, 110 } };
      for (int k = 0; k < 10; k++) {
        float a = k * 0.628f + t * (0.3f + ring * 0.2f);
        float cx = LD_GZ + cosf(a) * rr * 0.75f, cy = capY + sinf(a) * rr * 0.32f;
        lsCircle(cx, cy, rr * 0.48f, rgb565(col[ring][0], col[ring][1], col[ring][2]));
      }
    }
    lsCircle(LD_GZ, capY, capR * 0.3f, rgb565(255, 240, 180));
    if (b > 2.0f) {                                                    // the condensation ring round the stem
      float a = fminf(1.0f, (b - 2.0f) / 1.5f) * fmaxf(0.0f, 1.0f - (b - 6.0f) / 3.0f), rx = capR * 1.3f + 30 * (b - 2.0f);
      for (int k = 0; k < 56; k++) {
        float th = k * 0.1122f;
        lsTint(LD_GZ + cosf(th) * rx, capY + 120 + sinf(th) * rx * 0.18f, 20, 255, 236, 220, 0.12f * a);
      }
    }
    if (b < 1.6f) {                                                    // the fireball at the ground
      float fr = 40 + 180 * fminf(1.0f, b / 0.8f);
      lsCircle(LD_GZ, LS_GROUND - 10, fr, rgb565(255, (int)(250 - 120 * b / 1.6f), (int)(200 - 180 * b / 1.6f)));
    }
    float dust = 120 + 260 * ease;                                      // dust rolling out along the ground
    for (int k = -6; k <= 6; k++) {
      float dx = k / 6.0f * dust;
      lsCircle(LD_GZ + dx, LS_GROUND - 8, 26 + 10 * fabsf(sinf(k + t)), rgb565(110, 70, 50));
    }
  }
  // the buildings
  for (int i = 0; i < ldBuildings; i++) {
    float cx = ldBX[i] + ldBW[i] / 2.0f;
    ldBuilding(i, shock >= 0 && fabsf(cx - LD_GZ) < shock, t, glow);
  }
  lsRect(0, LS_GROUND, LS_SIZE - 1, LS_SIZE - 1, b < 0 ? rgb565(14, 14, 20) : rgb565(36, 18, 14));   // the ground
  if (shock > 0 && shock < 800) {                                      // the shock wave's front along the ground
    for (int s = -1; s <= 1; s += 2) lsTint(LD_GZ + s * shock, LS_GROUND - 6, 36, 255, 220, 170, 0.5f * fmaxf(0, 1 - shock / 800));
  }
  for (int i = 0; i < LS_FIRE; i++) {                                  // debris
    const LsFire &f = lsFire[i];
    if (f.on) lsCircle(f.x, f.y, f.r, f.age < 0.8f ? rgb565(255, 200, 80) : rgb565(160, 60, 30));
  }
  // the warhead coming down
  if (b < 0) {
    float k = t / LD_FALL, wx = LD_GZ + 260 * (1 - k), wy = -20 + (LS_GROUND - 60) * k;
    for (int j = 1; j <= 8; j++) lsTint(wx + j * 12, wy - j * 26, 6 + j, 255, 190, 120, 0.35f - j * 0.035f);
    lsCircle(wx, wy, 6, rgb565(255, 250, 230));
  }
  // the flash
  if (b >= 0 && b < 0.8f) {
    float a = 1.0f - b / 0.8f;
    lsTint(LS_SIZE / 2, LS_SIZE / 2, 560, 255, 255, 250, a);
  }
  // the words: the city, the toll (not at the end of the game: it lists them itself)
  if (b > 1.0f && !lsNoNames) {
    char name[48], line[64], num[24];
    snprintf(name, sizeof(name), "%s, %s", ct.name, ct.country);
    int n = clampv(lsTyped, 0, (int)strlen(name));
    char shown[48];
    snprintf(shown, sizeof(shown), "%.*s", n, name);
    lsRect(0, 18, LS_SIZE - 1, 120, rgb565(10, 6, 6));
    lsText("NUCLEAR STRIKE", LS_SIZE / 2, 44, 3.6f, rgb565(255, 70, 50), true);
    lsText(shown, LS_SIZE / 2, 92, 4.2f, rgb565(255, 230, 200), true);
    if (b > 2.5f) {
      float k = fminf(1.0f, (b - 2.5f) / 4.0f);
      uint32_t shownDeaths = (uint32_t)(landDeaths[ldSite] * (1 - (1 - k) * (1 - k)));
      commas(num, sizeof(num), shownDeaths);
      lsRect(0, LS_GROUND + 24, LS_SIZE - 1, LS_SIZE - 20, rgb565(10, 6, 6));
      lsText("ESTIMATED DEATHS", LS_SIZE / 2, LS_GROUND + 54, 2.6f, rgb565(230, 200, 190), false);
      lsText(num, LS_SIZE / 2, LS_GROUND + 100, 5.0f, rgb565(255, 60, 40), true);
      if (k >= 1 && deathTotal > landDeaths[ldSite]) {
        commas(num, sizeof(num), deathTotal);
        snprintf(line, sizeof(line), "WORLD TOTAL: %s", num);
        lsText(line, LS_SIZE / 2, LS_GROUND + 140, 2.2f, rgb565(230, 200, 190), false);
      }
    }
  }
  gfxClipAll();
}

// The site's sign: yellow and black edged, its name stencilled (on the title: blank)
static void lsSign(int cx, int cy, int w, float size) {
  lsRect(cx - w / 2 - 6, cy - 34, cx + w / 2 + 6, cy + 34, rgb565(20, 20, 22));
  lsHazard(cx - w / 2 - 6, cx + w / 2 + 6, cy - 34, cy - 26);
  lsHazard(cx - w / 2 - 6, cx + w / 2 + 6, cy + 26, cy + 34);
  lsRect(cx - w / 2, cy - 24, cx + w / 2, cy + 24, rgb565(36, 36, 40));
  if (!lsNoNames) lsText(siteName[launchSite], cx, cy, size, rgb565(240, 200, 60), true);
}

static void startLaunchScene(int site) {     // (site: a launch; 8 + site: a landing)
  if (launchOn) {                             // (one at a time: the next after this one)
    if (launchQueued < NUKE_SITES * 2) launchQueue[launchQueued++] = site;
    return;
  }
  launchOn = true;
  lsKind = site >> 3;
  if (lsKind) {
    startLandScene(site & 7);
    return;
  }
  launchSite = site;
  lsScene = 1;
  lsT = 0;
  lsSoundT = 0;
  lsTyped = 0;
  for (int i = 0; i < LS_FIRE; i++) lsFire[i].on = false;
  sfxPlay(SFX_KLAXON);
}

static void finishCutScene() {
  launchOn = false;
  if (launchQueued > 0) {                     // (another launched or landed meanwhile)
    int next = launchQueue[0];
    for (int i = 1; i < launchQueued; i++) launchQueue[i - 1] = launchQueue[i];
    launchQueued--;
    startLaunchScene(next);
  }
}

static void updateLaunchScene(float dt) {
  if (!launchOn) return;
  if (lsKind == 1) {
    updateLandScene(dt);
    if (lsT >= LD_SECONDS) finishCutScene();
    return;
  }
  lsT += dt;
  for (int i = 0; i < LS_FIRE; i++) {
    LsFire &f = lsFire[i];
    if (!f.on) continue;
    f.age += dt;
    if (f.age >= f.life) { f.on = false; continue; }
    f.x += f.vx * dt;
    f.y += f.vy * dt;
    f.vx *= (1 - dt * 0.7f);
    f.vy = f.vy * (1 - dt * 0.7f) - 20 * dt;
    f.r = fminf(f.r + dt * (lsScene == 1 ? 18.0f : 14.0f), 40.0f);
  }
  if (lsScene == 1) {                          // the klaxon, then ignition and the roar
    lsSoundT -= dt;
    if (lsT < 1.0f) {
      if (lsSoundT <= 0) { lsSoundT = 0.8f; sfxPlay(SFX_KLAXON); }
    } else {
      if (lsT - dt < 1.0f) { sfxPlay(SFX_IGNITE); lsSoundT = 0.3f; }
      if (lsSoundT <= 0) { lsSoundT = 0.3f; sfxPlay(SFX_BIGROAR); }
      for (int k = 0; k < 2; k++) {
        float side = lsRand(0, 2) ? 1.0f : -1.0f;
        lsAddFire(360 + side * lsRand(20, 60), 640 + lsRand(0, 30), side * lsRand(180, 420), -(float)lsRand(0, 60), 18, 1.3f);
      }
      if (lsRand(0, 3) == 0) lsAddFire((float)lsRand(40, 680), 600, (float)lsRand(-30, 31), -(float)lsRand(60, 140), 20, 2.0f);
    }
    if (lsT >= LS_SCENE1_SECONDS) { lsScene = 2; lsT = 0; }
  } else if (lsScene == 2) {                   // the white flash
    if (lsT >= 0.35f) {
      lsScene = 3;
      lsT = 0;
      for (int i = 0; i < LS_FIRE; i++) lsFire[i].on = false;
      lsX = 360;
      lsY = LS_GROUND + 230;
      lsAng = 0;
      lsDir = lsRand(0, 2) ? 1.0f : -1.0f;
      lsSpeed = 0;
      lsScale = 1.6f;
      lsTyped = 0;
    }
  } else {                                     // up out of the silo, arcing over and away
    lsSoundT -= dt;
    if (lsSoundT <= 0) { lsSoundT = 0.3f; sfxPlay(lsT < 4 ? SFX_BIGROAR : SFX_ROAR); }
    float a = lsAng * 0.017453293f, hx = sinf(a), hy = -cosf(a);
    if (lsT < 2.2f) {
      lsY = LS_GROUND + 230 - 50 * lsT * lsT;
      lsSpeed = 100 * lsT;
    } else {
      lsAng += (lsDir * 65 - lsAng) * fminf(1.0f, dt * 0.40f);
      lsSpeed = fminf(lsSpeed + 25 * dt, 260);
      lsX += hx * lsSpeed * (lsScale / 1.6f) * dt;
      lsY += hy * lsSpeed * (lsScale / 1.6f) * dt;
      lsScale = fmaxf(0.0f, lsScale * (1.0f - dt * 0.40f));
    }
    if (lsT < 4.0f)
      for (int k = 0; k < 2; k++) lsAddFire(360 + lsRand(-60, 61), LS_GROUND - 4, (float)lsRand(-140, 141), -(float)lsRand(30, 120), 14, 2.4f);
    if (lsScale > 0.12f && lsY < LS_GROUND)
      lsAddFire(lsX - hx * 4, lsY - hy * 4, -hx * 40 + lsRand(-15, 16), -hy * 40 + lsRand(-15, 16), 6 * lsScale + 3, 2.2f);
    if (lsT > 2.0f) {                          // MISSILE LAUNCHED / the site, typed on
      int len = 16 + (int)strlen(siteName[launchSite]);
      int want = (int)((lsT - 2.0f) * 16);
      if (want > len) want = len;
      while (lsTyped < want) { lsTyped++; sfxPlay(SFX_TYPE); }
    }
    if (lsT >= LS_SCENE3_SECONDS) finishCutScene();
  }
}

// The missile, any size: (x, y) its nozzle, ang in degrees (0 straight up)
static void lsMissileAt(float x, float y, float ang, float sc, bool flame) {
  float a = ang * 0.017453293f, c = cosf(a), sn = sinf(a);
  auto poly = [&](const float (*pts)[2], int n, uint16_t col) {
    int xs[8], ys[8];
    for (int i = 0; i < n; i++) {
      float lx = pts[i][0] * sc, ly = pts[i][1] * sc;
      xs[i] = (int)lroundf(x + lx * c - ly * sn);
      ys[i] = (int)lroundf(y + lx * sn + ly * c);
    }
    lsPoly(xs, ys, n, col);
  };
  if (flame) {
    float len = 40 + lsRand(0, 26);
    const float outer[3][2] = { { -9, 2 }, { 9, 2 }, { 0, len } };
    const float inner[3][2] = { { -5, 2 }, { 5, 2 }, { 0, len * 0.6f } };
    poly(outer, 3, rgb565(255, 130, 30));
    poly(inner, 3, rgb565(255, 240, 170));
  }
  if (sc < 0.25f) { lsCircle(x, y, 2, rgb565(240, 240, 240)); return; }
  const float body[4][2] = { { -10, 0 }, { 10, 0 }, { 10, -98 }, { -10, -98 } };
  const float shade[4][2] = { { 3, 0 }, { 10, 0 }, { 10, -98 }, { 3, -98 } };
  const float nose[5][2] = { { -10, -98 }, { 10, -98 }, { 7, -112 }, { 0, -126 }, { -7, -112 } };
  const float finL[4][2] = { { -10, -2 }, { -10, -26 }, { -20, -6 }, { -20, 4 } };
  const float finR[4][2] = { { 10, -2 }, { 10, -26 }, { 20, -6 }, { 20, 4 } };
  const float band1[4][2] = { { -10, -30 }, { 10, -30 }, { 10, -38 }, { -10, -38 } };
  const float band2[4][2] = { { -10, -72 }, { 10, -72 }, { 10, -80 }, { -10, -80 } };
  poly(finL, 4, rgb565(70, 74, 82));
  poly(finR, 4, rgb565(70, 74, 82));
  poly(body, 4, rgb565(232, 234, 238));
  poly(shade, 4, rgb565(190, 194, 202));
  poly(band1, 4, rgb565(30, 32, 36));
  poly(band2, 4, rgb565(30, 32, 36));
  poly(nose, 5, rgb565(200, 40, 30));
}

// Scene 1: the bottom of the missile, close up (baseY: the bottom of its body)
static void lsMissileBase(int ox, int baseY) {
  uint16_t white = rgb565(226, 228, 232), shade = rgb565(176, 180, 188), dark = rgb565(28, 30, 34);
  int bx0 = ox + 250, bx1 = ox + 470;
  lsRect(bx0, 0, bx1, baseY, white);
  lsRect(ox + 400, 0, bx1, baseY, shade);
  lsRect(bx0, 0, bx0 + 6, baseY, rgb565(150, 154, 162));
  for (int b = 0; b < 3; b++) {
    int y0 = baseY - 120 - b * 170;
    lsRect(bx0, y0 - 60, ox + 360, y0, dark);
    lsRect(ox + 361, y0 - 230, bx1, y0 - 170, dark);
  }
  for (int y = baseY - 40; y > 0; y -= 85) lsRect(bx0, y, bx1, y + 1, rgb565(120, 124, 130));
  int lfx[4] = { bx0, bx0, ox + 170, ox + 170 }, lfy[4] = { baseY - 190, baseY + 10, baseY + 40, baseY - 60 };
  lsPoly(lfx, lfy, 4, rgb565(70, 74, 82));
  int rfx[4] = { bx1, bx1, ox + 550, ox + 550 }, rfy[4] = { baseY - 190, baseY + 10, baseY + 40, baseY - 60 };
  lsPoly(rfx, rfy, 4, rgb565(56, 60, 68));
  int sx[4] = { bx0, bx1, ox + 450, ox + 270 }, sy[4] = { baseY, baseY, baseY + 34, baseY + 34 };
  lsPoly(sx, sy, 4, rgb565(90, 94, 102));
  int nx[4] = { ox + 312, ox + 408, ox + 440, ox + 280 }, ny[4] = { baseY + 34, baseY + 34, baseY + 120, baseY + 120 };
  lsPoly(nx, ny, 4, rgb565(60, 56, 54));
  int hx[4] = { ox + 330, ox + 352, ox + 366, ox + 302 }, hy[4] = { baseY + 36, baseY + 36, baseY + 118, baseY + 118 };
  lsPoly(hx, hy, 4, rgb565(110, 104, 98));
  lsRect(ox + 280, baseY + 116, ox + 440, baseY + 122, rgb565(40, 38, 36));
}

// Scene 1's background: inside the silo at the bottom, the curved walls (lit orange by the flames: lit 0..3)
static void lsSiloBase(int lit) {
  for (int x = 0; x < LS_SIZE; x += 2) {
    float k = 0.45f + 0.55f * cosf((x - 360) / 360.0f * 1.25f);
    lsRect(x, 0, x + 1, 609, rgb565((int)(70 * k), (int)(70 * k), (int)(74 * k)));
  }
  for (int y = 80; y < 610; y += 92)
    for (int x = 0; x < LS_SIZE; x += 4) {
      float k = 0.45f + 0.55f * cosf((x - 360) / 360.0f * 1.25f);
      int bend = (int)(10 * (1 - k));
      lsRect(x, y + bend, x + 3, y + bend, rgb565((int)(30 * k), (int)(30 * k), (int)(34 * k)));
      lsRect(x, y + bend + 1, x + 3, y + bend + 1, rgb565((int)(95 * k), (int)(95 * k), (int)(100 * k)));
    }
  for (int r = 0; r < 2; r++) lsRect(70 + r * 40, 0, 74 + r * 40, 609, rgb565(60, 62, 66));   // a ladder
  for (int y = 20; y < 600; y += 26) lsRect(70, y, 114, y + 3, rgb565(56, 58, 62));
  for (int i = 0; i < 3; i++) {                                                             // pipes
    int x = 600 + i * 26;
    lsRect(x, 0, x + 14, 609, rgb565(58 - i * 8, 66 - i * 8, 62 - i * 8));
    lsRect(x + 2, 0, x + 4, 609, rgb565(90, 98, 94));
  }
  lsRect(150, 230, 186, 244, rgb565(40, 40, 44));                                         // beacon housings
  lsRect(534, 230, 570, 244, rgb565(40, 40, 44));
  lsRect(0, 610, LS_SIZE - 1, LS_SIZE - 1, rgb565(28, 26, 24));                           // the flame trench
  lsHazard(0, LS_SIZE - 1, 610, 622);
  for (int x = 0; x < LS_SIZE; x += 16) lsRect(x, 623, x + 3, LS_SIZE - 1, rgb565(16, 15, 14));
  int dx[3] = { 280, 440, 360 }, dy[3] = { LS_SIZE - 1, LS_SIZE - 1, 650 };
  lsPoly(dx, dy, 3, rgb565(70, 66, 60));
  lsSign(86, 360, 152, 1.6f);                                                               // the site's sign on the wall
  if (lit > 0) pxBlend(LS_X, LS_Y, LS_X + LS_SIZE - 1, LS_Y + LS_SIZE - 1, 255, 120, 20, lit * 0.10f);
}

// Scene 3's background: the silo from above ground at night, hatch doors open; the site's sign by the fence
static void lsSiloTop() {
  for (int y = 0; y < LS_GROUND; y += 2) {
    float k = (float)y / LS_GROUND;
    lsRect(0, y, LS_SIZE - 1, y + 1, rgb565((int)(5 + 30 * k * k), (int)(7 + 14 * k), (int)(20 + 36 * k)));
  }
  uint32_t st = 777;                          // (the same stars each frame)
  for (int i = 0; i < 180; i++) {
    st = st * 1664525u + 1013904223u;
    int x = (int)((st >> 8) % LS_SIZE);
    st = st * 1664525u + 1013904223u;
    int y = (int)((st >> 8) % (LS_GROUND - 100));
    int b = 60 + (int)((st >> 20) % 150);
    lsRect(x, y, x + 1, y + 1, rgb565(b, b, b + 20 > 255 ? 255 : b + 20));
  }
  int xs[24], ys[24], n = 0;                  // the hills
  xs[n] = 0; ys[n++] = LS_GROUND;
  for (int i = 0; i <= 20; i++) {
    xs[n] = i * LS_SIZE / 20;
    ys[n++] = LS_GROUND - 30 - (int)(36 * fabsf(sinf(i * 1.3f + 0.4f)) + 20 * fabsf(sinf(i * 0.5f)));
  }
  xs[n] = LS_SIZE - 1; ys[n++] = LS_GROUND;
  for (int i = 1; i + 1 < n; i++) {            // (as strips: pxPoly takes up to 12 points)
    int qx[4] = { xs[i], xs[i + 1], xs[i + 1], xs[i] }, qy[4] = { ys[i], ys[i + 1], LS_GROUND, LS_GROUND };
    lsPoly(qx, qy, 4, rgb565(14, 16, 26));
  }
  lsRect(0, LS_GROUND, LS_SIZE - 1, LS_SIZE - 1, rgb565(26, 25, 22));
  lsRect(200, LS_GROUND - 6, 520, LS_GROUND + 30, rgb565(84, 84, 80));
  lsRect(200, LS_GROUND - 6, 520, LS_GROUND - 3, rgb565(130, 130, 124));
  lsRect(270, LS_GROUND - 6, 450, LS_SIZE - 1, rgb565(6, 6, 6));
  for (int y = LS_GROUND + 34; y < LS_SIZE; y += 30) lsRect(270, y, 450, y + 1, rgb565(20, 20, 20));
  lsHazard(204, 266, LS_GROUND + 8, LS_GROUND + 18);
  lsHazard(454, 516, LS_GROUND + 8, LS_GROUND + 18);
  int lx[4] = { 200, 270, 236, 160 }, ly[4] = { LS_GROUND - 6, LS_GROUND - 6, LS_GROUND - 120, LS_GROUND - 112 };
  lsPoly(lx, ly, 4, rgb565(92, 94, 96));
  int rx[4] = { 450, 520, 560, 484 }, ry[4] = { LS_GROUND - 6, LS_GROUND - 6, LS_GROUND - 112, LS_GROUND - 120 };
  lsPoly(rx, ry, 4, rgb565(78, 80, 82));
  for (int x = 8; x < LS_SIZE; x += 24) {
    if (x > 150 && x < 570) continue;
    lsRect(x, LS_GROUND - 22, x + 1, LS_GROUND, rgb565(60, 60, 64));
  }
  lsLine(0, LS_GROUND - 18, 150, LS_GROUND - 18, rgb565(60, 60, 64), false);
  lsLine(570, LS_GROUND - 18, LS_SIZE - 1, LS_GROUND - 18, rgb565(60, 60, 64), false);
  for (int s = 0; s < 2; s++) {
    int mx = s ? 640 : 80;
    lsRect(mx - 2, LS_GROUND - 150, mx + 2, LS_GROUND, rgb565(70, 70, 74));
    lsRect(mx - 16, LS_GROUND - 160, mx + 16, LS_GROUND - 148, rgb565(90, 90, 94));
    lsRect(mx - 13, LS_GROUND - 157, mx + 13, LS_GROUND - 151, rgb565(255, 240, 190));
  }
  // the site's sign on posts by the fence
  lsRect(546, LS_GROUND + 60, 550, LS_GROUND + 120, rgb565(70, 70, 74));
  lsRect(672, LS_GROUND + 60, 676, LS_GROUND + 120, rgb565(70, 70, 74));
  lsSign(611, LS_GROUND + 60, 212, 2.2f);
}

static void drawLaunchScene() {
  if (lsKind == 1) {
    drawLandScene();
    return;
  }
  gfxClip((int)(LS_Y / UNIT_K), (int)((LS_Y + LS_SIZE - 1) / UNIT_K));
  if (lsScene == 1) {
    float t = lsT;
    bool burning = t >= 1.0f;
    int lit = 0;
    if (burning) lit = (t < 1.2f) ? 3 : 2 + (lsRand(0, 3) == 0 ? 1 : 0) - (lsRand(0, 5) == 0 ? 1 : 0);
    lsSiloBase(lit);
    bool on = ((int)(t * 4)) & 1;                                  // beacons
    lsCircle(168, 224, 10, on ? rgb565(255, 40, 30) : rgb565(90, 16, 12));
    lsCircle(552, 224, 10, on ? rgb565(90, 16, 12) : rgb565(255, 40, 30));
    int ox = 0, oy = 0;
    if (burning) {                                                 // shaking
      int amt = (t < 1.6f) ? 9 : 5;
      ox = lsRand(-amt, amt + 1);
      oy = lsRand(-amt, amt + 1);
    }
    float rise = (t > 1.5f) ? 34.0f * (t - 1.5f) * (t - 1.5f) : 0;
    int baseY = (int)(430 - rise) + oy;
    for (int c2 = 0; c2 < 2; c2++) {                               // the umbilicals, snapping away
      int wx = c2 ? 610 : 110, wy = c2 ? 190 : 300;
      int mx = c2 ? 470 : 250, my = baseY - (c2 ? 360 : 240);
      if (!burning) {
        lsLine(wx, wy, mx + ox, my, rgb565(30, 30, 30), true);
        lsLine(wx, wy + 3, mx + ox, my + 3, rgb565(30, 30, 30), true);
      } else {
        float k = fminf((t - 1.0f) / 0.6f, 1.0f);
        lsLine(wx, wy, wx + (mx - wx) * (1 - k) * 0.6f, wy + 40 + 160 * k, rgb565(30, 30, 30), true);
      }
    }
    if (burning) {                                                 // the flame down into the trench
      int ny = baseY + 120;
      float grow = fminf((t - 1.0f) / 0.4f, 1.0f);
      int len = (int)((LS_SIZE - ny + 40) * grow);
      int w0 = 80, w1 = (int)(150 + 60 * grow);
      int fx[4] = { 360 - w0 + ox, 360 + w0 + ox, 360 + w1 + ox + lsRand(-12, 13), 360 - w1 + ox + lsRand(-12, 13) };
      int fy[4] = { ny, ny, ny + len, ny + len };
      lsPoly(fx, fy, 4, rgb565(255, 120, 20));
      int mx2[4] = { 360 - 60 + ox, 360 + 60 + ox, 360 + 90 + ox, 360 - 90 + ox };
      lsPoly(mx2, fy, 4, rgb565(255, 200, 60));
      int cx2[3] = { 360 - 40 + ox, 360 + 40 + ox, 360 + ox }, cy2[3] = { ny, ny, ny + (int)(len * 0.75f) + lsRand(-20, 21) };
      lsPoly(cx2, cy2, 3, rgb565(255, 250, 220));
    } else if (t > 0.6f) {                                         // sparks just before
      for (int k = 0; k < 6; k++) {
        int x = 360 + lsRand(-70, 71), y = 560 + lsRand(0, 20);
        lsLine(x, y, x + lsRand(-20, 21), y + lsRand(10, 40), rgb565(255, 230, 120), false);
      }
    }
    for (int i = 0; i < LS_FIRE; i++) {                            // fire and smoke across the floor
      const LsFire &f = lsFire[i];
      if (!f.on) continue;
      float k = f.age / f.life;
      uint16_t col = k < 0.25f ? rgb565(255, 220, 90) : k < 0.5f ? rgb565(255, 140, 30) : k < 0.75f ? rgb565(170, 70, 30) : rgb565(70, 60, 56);
      lsCircle(f.x + ox, f.y + oy, f.r, col);
    }
    lsMissileBase(ox, baseY);
  } else if (lsScene == 2) {
    lsRect(0, 0, LS_SIZE - 1, LS_SIZE - 1, rgb565(255, 255, 255));
  } else {
    float t = lsT;
    lsSiloTop();
    if (t < 4.5f) {                                                // the glow round the silo mouth
      float g = fminf(t / 0.5f, 1.0f) * (t > 3.0f ? (4.5f - t) / 1.5f : 1.0f);
      lsTint(360, LS_GROUND, 130, 255, 150, 50, 0.22f * g * (0.85f + 0.15f * lsRand(0, 100) / 100.0f));
    }
    for (int i = 0; i < LS_FIRE; i++) {                            // smoke
      const LsFire &f = lsFire[i];
      if (!f.on) continue;
      lsTint(f.x, f.y, f.r, 190, 186, 184, 0.6f * (1.0f - f.age / f.life));
    }
    if (lsScale > 0.12f) {                                          // the missile (hidden below the ground in the shaft)
      gfxClip((int)(LS_Y / UNIT_K), (int)((LS_Y + LS_GROUND - 7) / UNIT_K));
      lsMissileAt(lsX, lsY, lsAng, lsScale, true);
      gfxClip((int)(LS_Y / UNIT_K), (int)((LS_Y + LS_SIZE - 1) / UNIT_K));
    }
    if (lsTyped > 0 && !lsNoNames) {                                // MISSILE LAUNCHED, the site
      char head[20] = "MISSILE LAUNCHED";
      int n1 = lsTyped < 16 ? lsTyped : 16;
      head[n1] = 0;
      lsText(head, 360, 110, 4.5f, rgb565(255, 70, 50), true);
      int n2 = lsTyped - 16;
      if (n2 > 0) {
        char dest[24];
        snprintf(dest, sizeof(dest), "%.*s", n2, siteName[launchSite]);
        lsText(dest, 360, 170, 5.0f, rgb565(240, 240, 245), true);
      }
    }
  }
  gfxClipAll();
}

// The frame with a launch on: the game shrunk on the left, the scene on the right
static void composeLaunchFrame() {
  memcpy(frameSave, g_frame, sizeof(frameSave));
  pxRect(0, 0, FB_W - 1, FB_H - 1, 0);
  drawLaunchScene();
  const int GW = 1200, GH = 675, GY = (FB_H - GH) / 2;   // (5/8 of the size)
  for (int y = 0; y < GH; y++) {
    int sy = y * FB_H / GH;
    const uint32_t *r0 = frameSave + sy * FB_W, *r1 = frameSave + (sy + 1 < FB_H ? sy + 1 : sy) * FB_W;
    uint32_t *d = g_frame + (GY + y) * FB_W;
    for (int x = 0; x < GW; x++) {
      int sx = x * FB_W / GW, sx1 = sx + 1 < FB_W ? sx + 1 : sx;
      uint32_t a = r0[sx], b = r0[sx1], c = r1[sx], e = r1[sx1];
      d[x] = (((a & 0xFF00FF) + (b & 0xFF00FF) + (c & 0xFF00FF) + (e & 0xFF00FF)) >> 2 & 0xFF00FF) |
             (((a & 0x00FF00) + (b & 0x00FF00) + (c & 0x00FF00) + (e & 0x00FF00)) >> 2 & 0x00FF00) | (a & 0xFF000000);
    }
  }
  pxRect(GW, 0, GW + 1, FB_H - 1, c32(rgb565(60, 64, 80)));   // (a line between them)
  drawVectorText(lsKind ? "NUCLEAR IMPACT" : "NUCLEAR LAUNCH", (LS_X + LS_SIZE / 2) / UNIT_K, (LS_Y - 50) / UNIT_K, 3.0f,
                 fmodf(clockT, 0.6f) < 0.35f ? rgb565(255, 60, 40) : rgb565(140, 30, 20), true);
}

// The last one dealt with, and at least one destroyed in flight: CONGRATULATIONS (all of them stopped, or the best he could)
static void checkAllStopped() {
  if (congratsDone || gameOver != END_NONE) return;
  bool launching = sitesHacked == NUKE_SITES || lockdown;
  if (!launching || sitesLaunched < sitesHacked) return;
  int destroyedN = 0, landedN = 0;
  for (int i = 0; i < sitesLaunched; i++) {
    if (prevented[i]) continue;
    if (nukeDestroyed[i]) destroyedN++;
    else if (landLeft[i] > 0) return;        // (one still in the air)
    else landedN++;
  }
  if (destroyedN == 0 || launchOn) return;
  congratsDone = true;
  congratsAll = landedN == 0;
  congratsT = 8.0f;
  sfxPlay(SFX_FANFARE);
}
static bool inFlight(int i) { return i < sitesLaunched && !prevented[i] && !nukeDestroyed[i] && landLeft[i] > 0; }
static int cityOfSite(int i) { return landCity[i]; }
static uint32_t deathsSoFar() { return deathTotal; }

// STAGE 2's countdown (the clock at the top): to the next launch; with none left to come, to the next impact.
// -1: nothing left (every launch made or stopped, nothing in the air). *impact: which it is.
static float s2Countdown(bool *impact) {
  *impact = false;
  bool launching = sitesHacked == NUKE_SITES || lockdown;
  int next = sitesLaunched;
  while (next < NUKE_SITES && prevented[next]) next++;
  if (!launching && next < NUKE_SITES) {     // still hacking: the rest of the hacking, then the first launch's countdown
    float t = hackSecs[sitesHacked] - hackT;
    for (int j = sitesHacked + 1; j < NUKE_SITES; j++) t += hackSecs[j];
    return t + LAUNCH_MINUTES[next] * 60;
  }
  if (launching && next < sitesHacked) return LAUNCH_MINUTES[next] * 60 - (next == sitesLaunched ? launchT : 0);
  float best = -1;
  for (int i = 0; i < NUKE_SITES; i++)
    if (inFlight(i) && (best < 0 || landLeft[i] < best)) best = landLeft[i];
  *impact = best >= 0;
  return best;
}
// A penalty in stage 2: the countdown at the top, 30 seconds sooner (whatever it's counting to)
static void s2Penalty(float secs) {
  bool impact;
  float left = s2Countdown(&impact);
  if (left < 0) return;
  bool launching = sitesHacked == NUKE_SITES || lockdown;
  if (impact) {                              // the next to land
    int soon = -1;
    for (int i = 0; i < NUKE_SITES; i++)
      if (inFlight(i) && (soon < 0 || landLeft[i] < landLeft[soon])) soon = i;
    if (soon >= 0) landLeft[soon] = fmaxf(0.5f, landLeft[soon] - secs);
  } else if (launching) {                    // the next launch
    int next = sitesLaunched;
    while (next < NUKE_SITES && prevented[next]) next++;
    if (next == sitesLaunched) launchT = fminf(launchT + secs, LAUNCH_MINUTES[next] * 60 - 0.5f);
  } else {                                   // still hacking: the hack under way (what's over carries on)
    hackT += secs;
  }
}

// The launch sites: hacking, launching, landing (in play time)
static void updateNukes(float dt) {
  char b[64];
  if (!lockdown && sitesHacked < NUKE_SITES) {
    hackT += dt;
    if (hackT >= hackSecs[sitesHacked]) {
      hackT = stage == 2 ? hackT - hackSecs[sitesHacked] : 0;   // (stage 2: a penalty past its end carries on)
      snprintf(b, sizeof(b), "%s HACKED", siteName[sitesHacked]);
      flyerAdd(b);
      sfxPlay(SFX_TICK24);
      sitesHacked++;
    }
  }
  bool launching = sitesHacked == NUKE_SITES || lockdown;
  while (launching && sitesLaunched < sitesHacked && prevented[sitesLaunched]) {   // (stopped: passed over, the next one's countdown starts)
    sitesLaunched++;
    launchT = 0;
  }
  if (launching && sitesLaunched < sitesHacked) {
    launchT += dt;
    if (launchT >= LAUNCH_MINUTES[sitesLaunched] * 60) {
      launchT = 0;
      landLeft[sitesLaunched] = 60.0f * (LAND_MIN_MINUTES + (LAND_MAX_MINUTES - LAND_MIN_MINUTES) * (rnd32() % 1000) / 1000.0f);
      landTotal[sitesLaunched] = landLeft[sitesLaunched];
      startLaunchScene(sitesLaunched);     // (NUCLEAR MISSION's launch, beside the game)
      sitesLaunched++;
    }
  }
  int landed = 0, flown = 0;
  for (int i = 0; i < sitesLaunched; i++) {
    if (prevented[i] || nukeDestroyed[i]) continue;
    flown++;
    if (landLeft[i] > 0) {
      landLeft[i] -= dt;
      if (landLeft[i] <= 0) {                  // landed: a city hit (its cut scene beside the game)
        deathTotal += landDeaths[i];         // (its city chosen at the start)
        snprintf(b, sizeof(b), "NUCLEAR STRIKE ON %s", cities[landCity[i]].name);
        flyerAdd(b);
        startLaunchScene(8 + i);
      }
    }
    landed += landLeft[i] <= 0;
  }
  bool anyDestroyed = false;
  for (int i = 0; i < NUKE_SITES; i++) anyDestroyed |= nukeDestroyed[i];
  (void)anyDestroyed;                        // (they landing is no longer the end: stage 2 ends with his health, or Dr. Voss)
  (void)flown;
  (void)landed;
  checkAllStopped();
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
    bool stopped = prevented[i], hacked = i < sitesHacked, launched = i < sitesLaunched && !stopped, landed = launched && landLeft[i] <= 0;
    bool safe = (lockdown && !hacked) || stopped;
    drawVectorText(siteName[i], x0 + 14 + vectorTextWidth(siteName[i], 1.35f) / 2, y, 1.35f, safe ? rgb565(110, 120, 140) : rgb565(220, 226, 240), false);
    // how far along each stage is (0..1): filled from the left
    float fill[3] = { hacked ? 1.0f : 0.0f, launched ? 1.0f : 0.0f, landed ? 1.0f : 0.0f };
    if (launched && !landed && landTotal[i] > 0) fill[2] = clampv(1.0f - landLeft[i] / landTotal[i], 0.0f, 1.0f);
    else if (hacked && !launched && !stopped && i == sitesLaunched && (sitesHacked == NUKE_SITES || lockdown)) fill[1] = clampv(launchT / (LAUNCH_MINUTES[i] * 60), 0.0f, 1.0f);
    else if (!hacked && !lockdown && i == sitesHacked) fill[0] = clampv(hackT / hackSecs[i], 0.0f, 1.0f);
    (void)blink;
    for (int c = 0; c < 3; c++) {
      int bx = bx0 + c * (bw + gap);
      fillRect(bx, y - 9, bx + bw, y + 9, safe ? rgb565(36, 60, 48) : rgb565(44, 48, 62));
      int w = (int)lroundf(bw * fill[c]);
      if (w > 0) fillRect(bx, y - 9, bx + w, y + 9, done[c]);
      if (nukeDestroyed[i] && c == 2) {        // destroyed in flight: DESTROYED in green
        fillRect(bx, y - 9, bx + bw, y + 9, rgb565(50, 170, 90));
        drawVectorText("DESTROYED", bx + bw / 2.0f, (float)y, 0.9f, rgb565(230, 255, 235), true);
      }
      if (stopped && c == 1) {                 // its launch stopped: STOPPED in green
        fillRect(bx, y - 9, bx + bw, y + 9, rgb565(50, 170, 90));
        drawVectorText("STOPPED", bx + bw / 2.0f, (float)y, 1.0f, rgb565(230, 255, 235), true);
      }
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

// ---- the server room's four terminals (NODE 1..4): each takes one QUANTUM PASSWORD, once, and with it
// stops one hacked site's launch (not one already launched). The room's camera and drone wait while
// he's logged in, but he can't log in with the drone's light red (it's firing at him).
enum SnPage : uint8_t { SN_LOGIN, SN_MENU, SN_SENDING, SN_DONE, SN_LATE, SN_USED, SN_NOPASS, SN_NOSITES };
#define SN_LOGIN_SECONDS 3.0f
#define SN_SEND_SECONDS 3.0f
static bool preventable(int i) { return i >= sitesLaunched && i < sitesHacked && !prevented[i]; }
static int preventList(int *out) {
  int n = 0;
  for (int i = 0; i < NUKE_SITES; i++) if (preventable(i)) out[n++] = i;
  return n;
}
static void useServerNode(int n) {
  termOn = true;
  termKind = 2;
  curNode = n;
  termT = 0;
  termSel = 0;
  termPage = nodeUsed[n] ? SN_USED : s2PassFound - passUsed < PASS_PER_LOGIN ? SN_NOPASS : SN_LOGIN;
  sfxPlay(termPage == SN_LOGIN ? SFX_FOUND : SFX_DENIED);
}
static void snLogOff() {
  if (termPage == SN_DONE) {               // SITE LAUNCH STOPPED, flying out at him in green
    char b[48];
    snprintf(b, sizeof(b), "%s LAUNCH STOPPED", siteName[snSite]);
    flyerAdd(b, true);
  }
  termOn = false;
  sfxPlay(SFX_KEY);
  if (drGrace < 1.0f) drGrace = 1.0f;     // (a moment before the drone's at it again)
}
static void updateServerNode(float dt) {
  float was = termT;
  termT += dt;
  bool go = (pressed & (BTN_USE | BTN_JUMP)) != 0;
  int list[NUKE_SITES], n = preventList(list);
  switch (termPage) {
    case SN_LOGIN:
      if ((int)(was * 8) != (int)(termT * 8) && termT < 1.8f) sfxPlay(SFX_KEY);
      if (termT >= SN_LOGIN_SECONDS || (go && termT > 0.3f)) {
        termPage = n > 0 ? SN_MENU : SN_NOSITES;
        termT = 0;
        termSel = 0;
        sfxPlay(SFX_KEY);
      }
      break;
    case SN_MENU:
      if (termSel > n) termSel = n;          // (a site launched under him: the list is shorter)
      if (pressed & BTN_UP) { termSel = (termSel + n) % (n + 1); sfxPlay(SFX_KEY); }
      if (pressed & BTN_DOWN) { termSel = (termSel + 1) % (n + 1); sfxPlay(SFX_KEY); }
      if (go && termT > 0.2f) {
        if (termSel >= n) { snLogOff(); break; }   // LOG OFF (the password's not used)
        snSite = list[termSel];
        termPage = SN_SENDING;
        termT = 0;
        sfxPlay(SFX_POWERUP);
      }
      break;
    case SN_SENDING:
      if ((int)(was * 5) != (int)(termT * 5)) sfxPlay(SFX_BEEP);
      if (termT >= SN_SEND_SECONDS) {
        termT = 0;
        if (!preventable(snSite)) { termPage = SN_LATE; sfxPlay(SFX_DENIED); break; }   // (it launched while the abort went)
        prevented[snSite] = true;
        if (snSite == sitesLaunched) launchT = 0;
        nodeUsed[curNode] = true;
        passUsed += PASS_PER_LOGIN;
        termPage = SN_DONE;
        addScore(PREVENT_SCORE, agX, agY - 200);
        sfxPlay(SFX_FANFARE);                // (the news flies out when he logs off)
      }
      break;
    case SN_LATE:
      if (go && termT > 0.4f) { termPage = n > 0 ? SN_MENU : SN_NOSITES; termT = 0; termSel = 0; sfxPlay(SFX_KEY); }
      break;
    default:                                 // done, used, no password, nothing to stop: off
      if (go && termT > 0.4f) snLogOff();
      break;
  }
}

// ---- the CODE ROOM's terminal: Impossible Mission's sequence. Squares light up (a note each) - 3, then 4, then 5;
// he repeats each exactly (the D-pad moves the cursor, X picks a square). A mistake: a new sequence, the same length.
// All three: a NUKE SELF DESTRUCT CODE (he can hold only one; using it in the room below, he can come back for another).
static void simNewSeq() {
  int n = 3 + simRound;
  for (int k = 0; k < n; k++) {
    int c;
    do c = fxRnd(0, SIM_COLS * SIM_ROWS); while (k && c == simSeq[k - 1]);
    simSeq[k] = c;
  }
  simState = SIM_SHOW;
  simT = -0.7f;
  simPos = 0;
  simLit = -1;
}
static void startSimon() {
  setState(ST_IDLE, 0.1f);
  if (s2NukeCode) {
    showMsg("YOU ALREADY HOLD A SELF DESTRUCT CODE - USE IT IN THE ROOM BELOW", 3.0f);
    sfxPlay(SFX_DENIED);
    return;
  }
  simOn = true;
  simRound = 0;
  simCurX = 3;
  simCurY = 2;
  simNewSeq();
  sfxPlay(SFX_POWERUP);
}
static void foundCard(int level);
static void updateSimon(float dt) {
  simT += dt;
  if (simLitT > 0 && (simLitT -= dt) <= 0) simLit = -1;
  if (pressed & BTN_USE) {                   // (Square: walk away from it)
    simOn = false;
    simLit = -1;
    showMsg("SEQUENCE ABANDONED", 1.5f);
    sfxPlay(SFX_KEY);
    return;
  }
  int n = 3 + simRound;
  switch (simState) {
    case SIM_SHOW: {
      if (simT < 0) break;
      int k = (int)(simT / (SIM_ON + SIM_GAP));
      if (k >= n) { simState = SIM_INPUT; simT = 0; simLit = -1; simPos = 0; break; }
      if (simT - k * (SIM_ON + SIM_GAP) < SIM_ON) {
        if (simLit != simSeq[k]) { simLit = simSeq[k]; sfxPlay((uint8_t)(SFX_TONE + simLit)); }
      } else {
        simLit = -1;
      }
      break;
    }
    case SIM_INPUT:
      if (pressed & BTN_LEFT) simCurX = (simCurX + SIM_COLS - 1) % SIM_COLS;
      if (pressed & BTN_RIGHT) simCurX = (simCurX + 1) % SIM_COLS;
      if (pressed & BTN_UP) simCurY = (simCurY + SIM_ROWS - 1) % SIM_ROWS;
      if (pressed & BTN_DOWN) simCurY = (simCurY + 1) % SIM_ROWS;
      if (pressed & BTN_JUMP) {
        int c = simCurY * SIM_COLS + simCurX;
        simLit = c;
        simLitT = 0.32f;
        sfxPlay((uint8_t)(SFX_TONE + c));
        if (c != simSeq[simPos]) { simState = SIM_BAD; simT = 0; sfxPlay(SFX_DENIED); }
        else if (++simPos >= n) { simState = SIM_GOOD; simT = 0; }
      }
      break;
    case SIM_GOOD:
      if (simT - dt < 0.4f && simT >= 0.4f) sfxPlay(SFX_FOUND);
      if (simT > 1.3f) {
        if (++simRound < SIM_ROUNDS) { simNewSeq(); break; }
        simOn = false;                       // all three: the code
        simState = SIM_WON;
        simT = 0;
        s2NukeCode = true;
        snprintf(nukeCodeText, sizeof(nukeCodeText), "%c%d-%03d-%c%c", 'A' + fxRnd(0, 26), fxRnd(1, 10), fxRnd(0, 1000), 'A' + fxRnd(0, 26), 'A' + fxRnd(0, 26));
        addScore(1000, SIM_TERM_X, ROOM_FLOOR_Y - 220);
        foundCard(-6);
      }
      break;
    case SIM_BAD:
      if (simT > 1.6f) simNewSeq();          // (the same round again, a new sequence)
      break;
    default:
      break;
  }
}

// ---- the SELF DESTRUCT console: with a NUKE SELF DESTRUCT CODE, destroy one missile in flight (the code's used up)
enum DsPage : uint8_t { DS_NOCODE, DS_MENU, DS_SENDING, DS_DONE, DS_NONE, DS_LATE };
#define DS_SEND_SECONDS 3.0f
static int dsSite = 0;
static int flightList(int *out) {
  int n = 0;
  for (int i = 0; i < NUKE_SITES; i++) if (inFlight(i)) out[n++] = i;
  return n;
}
static void useDestructTerm() {
  int l[NUKE_SITES];
  termOn = true;
  termKind = 3;
  termT = 0;
  termSel = 0;
  termPage = !s2NukeCode ? DS_NOCODE : flightList(l) > 0 ? DS_MENU : DS_NONE;
  sfxPlay(termPage == DS_MENU ? SFX_FOUND : SFX_DENIED);
}
static void updateDestructTerm(float dt) {
  float was = termT;
  termT += dt;
  bool go = (pressed & (BTN_USE | BTN_JUMP)) != 0;
  int list[NUKE_SITES], n = flightList(list);
  switch (termPage) {
    case DS_MENU:
      if (termSel > n) termSel = n;
      if (pressed & BTN_UP) { termSel = (termSel + n) % (n + 1); sfxPlay(SFX_KEY); }
      if (pressed & BTN_DOWN) { termSel = (termSel + 1) % (n + 1); sfxPlay(SFX_KEY); }
      if (go && termT > 0.2f) {
        if (termSel >= n) { termOn = false; sfxPlay(SFX_KEY); break; }   // LOG OFF (the code kept)
        dsSite = list[termSel];
        termPage = DS_SENDING;
        termT = 0;
        sfxPlay(SFX_POWERUP);
      }
      break;
    case DS_SENDING:
      if ((int)(was * 5) != (int)(termT * 5)) sfxPlay(SFX_BEEP);
      if (termT >= DS_SEND_SECONDS) {
        termT = 0;
        if (!inFlight(dsSite)) { termPage = DS_LATE; sfxPlay(SFX_DENIED); break; }   // (it landed while the code went)
        nukeDestroyed[dsSite] = true;
        s2NukeCode = false;
        termPage = DS_DONE;
        addScore(PREVENT_SCORE, agX, agY - 200);
        sfxPlay(SFX_BOOM);
        sfxPlay(SFX_FANFARE);
        char b[48];
        snprintf(b, sizeof(b), "%s MISSILE DESTROYED", siteName[dsSite]);
        flyerAdd(b, true);
        checkAllStopped();
      }
      break;
    case DS_LATE:
      if (go && termT > 0.4f) { termPage = n > 0 ? DS_MENU : DS_NONE; termT = 0; termSel = 0; sfxPlay(SFX_KEY); }
      break;
    default:                                 // done, no code, nothing in the air: off
      if (go && termT > 0.4f) { termOn = false; sfxPlay(SFX_KEY); }
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
  if (!hasKnife || stage != 2 || (mode != MODE_ROOM && fgLoaded < 0)) return -1;
  for (int i = 0; i < npcCount; i++) {
    const Npc &n = npcs[i];
    if (!n.dead && (n.aimT <= 0 || n.foyer || n.rsold) && fabsf(agY - n.y) < 0.5f && fabsf(agX - n.x) < KNIFE_RANGE * fmaxf(1.0f, n.scale * 0.75f)) return i;
  }
  return -1;
}
static int killTarget = -1;
static bool onWalkway();
static bool canFire() {                      // the pistol (or the QX-9): in a lab room, or on a walkway
  return (hasGun || hasXGun || hasFlamer) && stage == 2 && ((mode == MODE_ROOM && curRoom < S2_ROOMS) || onWalkway()) && gameOver == END_NONE;
}
// Someone alive in line on his level, the way he faces (then Square fires, as in NUCLEAR MISSION; else it searches)
static bool targetInLine() {
  if (!canFire() || (mode != MODE_ROOM && fgLoaded < 0)) return false;
  for (int k = 0; k < npcCount; k++) {
    const Npc &n = npcs[k];
    if (!n.dead && fabsf(agY - n.y) < 60 && agFacing * (n.x - agX) > 0) return true;
  }
  return false;
}
// His bullets: across the room on his level; the first one they reach goes down (not the door guard)
// Someone killed (shot, or with the knife). A room with no hazard of its own whose people are all dead,
// one of them shot: the gunfire brings a security drone. It comes down from the ceiling on the far side,
// hovers for 5 seconds, then scans for him like any drone - and it stays in that room.
static void roomKill(bool shot) {
  if (stage != 2 || mode != MODE_ROOM || curRoom >= S2_ROOMS) return;
  if (shot) s2RoomShot[curRoom] = true;
  RoomInfo &ri = rooms[curRoom];
  if (ri.hazard != HZ_NONE || s2Kind[curRoom] == LK_CLONE || s2Kind[curRoom] >= LK_SIMON || !s2RoomShot[curRoom]) return;
  for (int i = 0; i < npcCount; i++) if (!npcs[i].dead) return;
  ri.hazard = HZ_DRONE;
  curHazard = HZ_DRONE;
  drX = agX < SCREEN_W / 2 ? SCREEN_W - 160.0f : 160.0f;
  drY = CEIL_Y + 10.0f;
  drTX = drX;
  drTY = CEIL_Y + 110.0f;
  drVX = drVY = 0;
  drScan = false;
  drAlerted = false;
  drGrace = 0;
  drLight = 0;
  drFlashT = 0;
  drWakeT = DRONE_WAKE_SECONDS;
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].life = 0;
  showMsg("GUNFIRE HEARD - SECURITY DRONE DISPATCHED", 3.0f);
  sfxPlay(SFX_ALARM);
}

// Keep what's happened to someone (killed, his size): his room's record, or the foyer's
static void npcPersist(int i) {
  const Npc &n = npcs[i];
  if (n.foyer) {
    if (n.lvl < 0 || n.lvl >= FG_MAXL || n.fsh < 0 || n.fsh >= FG_LIFTS || n.fslot < 0 || n.fslot >= FG_MAXS) return;
    fgDead[n.fsh][n.lvl][n.fslot] = n.dead;
    fgDX[n.fsh][n.lvl][n.fslot] = n.x;
    fgDDir[n.fsh][n.lvl][n.fslot] = (int8_t)n.dir;
    fgScale[n.fsh][n.lvl][n.fslot] = n.scale;
    fgAsh[n.fsh][n.lvl][n.fslot] = n.burn;
    return;
  }
  if (mode != MODE_ROOM || curRoom >= S2_ROOMS) return;
  s2NpcDead[curRoom][i] = n.dead;
  s2NpcDX[curRoom][i] = n.x;
  s2NpcDY[curRoom][i] = n.y;
  s2NpcDDir[curRoom][i] = (int8_t)n.dir;
  s2NpcScale[curRoom][i] = n.scale;
  s2NpcAsh[curRoom][i] = n.burn;
}
static void npcKilled(int i, bool shot) {
  Npc &n = npcs[i];
  n.dead = true;
  n.dieT = 0;
  n.watching = false;
  n.aimT = 0;
  n.fstate = 0;
  npcPersist(i);
  sfxPlay(SFX_HIT);
  addScore(500, n.x, n.y - 170 * n.scale);
  susp = 0;
  suspT = 0;
  if (n.kind == PK_PROFESSOR && bossDeadT < 0) {   // THE PROFESSOR: the end of the game (once he's down)
    bossDeadT = 0;
    showMsg("DR. VOSS IS DEAD", 3.0f);
    sfxPlay(SFX_FANFARE);
  }
  roomKill(shot);
}
static void xGunHit(int i) {               // the QX-9: half the size (1 in 10: double); a quarter size and he's gone
  Npc &n = npcs[i];
  n.fxT = 0.5f;
  sfxPlay(SFX_POWERUP);
  if (fxRnd(0, 10) == 0) {
    n.scale = fminf(4.0f, n.scale * 2);
    showMsg("DOUBLED IN SIZE!", 1.8f);
  } else {
    n.scale *= 0.5f;
    if (n.scale < 0.3f) {
      showMsg("SHRUNK TO NOTHING", 1.8f);
      npcKilled(i, true);
      return;
    }
    showMsg("SHRUNK TO HALF SIZE", 1.6f);
  }
  npcPersist(i);
}


// ============================================================ the FX-5 INFERNO (the second EXPERIMENTAL ARMORY's)
// A flamethrower: held, it pours out burning gas (unlimited fuel, for now). Anyone it catches burns, falls,
// chars and crumbles to ash - nothing left. Flames are particles: in a room or on a walkway they fly flat
// out from the nozzle; in lift D's top-down rooms they fly where he faces.
#define FLAMES 96
struct Flame { bool on; float x, y, vx, vy, age, life; };
static Flame flames[FLAMES];
static float flameSndT = 0;
static float frnd(float a, float b) { return a + (b - a) * (fxRnd(0, 10001) / 10000.0f); }
static void flameEmit(float x, float y, float dx, float dy, float dt) {   // (dx, dy: the direction, unit)
  int n = (int)(dt * 140) + 1;
  for (int k = 0; k < n; k++) {
    int f = -1;
    for (int i = 0; i < FLAMES; i++) if (!flames[i].on) { f = i; break; }
    if (f < 0) return;
    float a = atan2f(dy, dx) + frnd(-0.18f, 0.18f), sp = frnd(430, 560);
    flames[f] = { true, x, y, cosf(a) * sp, sinf(a) * sp, 0, frnd(0.38f, 0.52f) };
  }
  flameSndT -= dt;
  if (flameSndT <= 0) { flameSndT = 0.22f; sfxPlay(SFX_FLAME); }
}
static void updateFlames(float dt) {
  for (int i = 0; i < FLAMES; i++) {
    Flame &f = flames[i];
    if (!f.on) continue;
    f.age += dt;
    if (f.age >= f.life) { f.on = false; continue; }
    f.x += f.vx * dt;
    f.y += f.vy * dt;
    f.vx *= (1 - 1.6f * dt);
    f.vy = f.vy * (1 - 1.6f * dt) - 40 * dt;   // (rising a little as it slows)
  }
}
static void drawFlames() {
  for (int i = 0; i < FLAMES; i++) {
    const Flame &f = flames[i];
    if (!f.on) continue;
    float k = f.age / f.life, r = 8 + 26 * k;
    if (k < 0.55f) {
      glowDisc(f.x, f.y, r * 1.6f, 255, (int)(200 - 140 * k), 40, 0.55f * (1 - k));
      fillCircle((int)f.x, (int)f.y, (int)(r * 0.45f), k < 0.2f ? rgb565(255, 250, 200) : rgb565(255, 170, 40));
    } else {
      glowDisc(f.x, f.y, r * 1.4f, 90, 80, 76, 0.4f * (1 - k));       // smoke
    }
  }
}

// The flamer itself: a stubby gun, a fuel tank under it, a nozzle with a pilot light (facing right; sc 1 ~ 220 long)
static void drawFlamer(float cx, float cy, float sc, float glow, int dir = 1) {
  auto P = [&](float x, float y, float &ox, float &oy) { ox = cx + x * sc * dir; oy = cy + y * sc; };
  float xs[6], ys[6], a, b, c, d;
  const float body[6][2] = { { -110, -16 }, { 30, -16 }, { 46, -8 }, { 46, 10 }, { -60, 14 }, { -110, 10 } };
  for (int i = 0; i < 6; i++) P(body[i][0], body[i][1], xs[i], ys[i]);
  fillPolygonF(xs, ys, 6, rgb565(70, 74, 82));
  P(-100, -16, a, b); P(30, -10, c, d);
  fillRectF(a, b, c, d, rgb565(130, 136, 146));
  P(-80, 14, a, b); P(10, 44, c, d);                       // the fuel tank, red, a hazard band
  fillRectF(a, b, c, d, rgb565(200, 40, 30));
  P(-80, 24, a, b); P(10, 30, c, d);
  fillRectF(a, b, c, d, rgb565(240, 200, 40));
  P(-80, 29, a, b);
  fillCircle((int)a, (int)b, (int)(15 * sc), rgb565(170, 34, 26));
  P(10, 29, a, b);
  fillCircle((int)a, (int)b, (int)(15 * sc), rgb565(170, 34, 26));
  const float grip[4][2] = { { -50, 12 }, { -26, 12 }, { -34, 58 }, { -60, 58 } };
  for (int i = 0; i < 4; i++) P(grip[i][0], grip[i][1], xs[i], ys[i]);
  fillPolygonF(xs, ys, 4, rgb565(40, 34, 30));
  P(46, -6, a, b); P(110, 6, c, d);                        // the barrel, the nozzle
  fillRectF(a, b, c, d, rgb565(110, 116, 126));
  P(104, -10, a, b); P(118, 10, c, d);
  fillRectF(a, b, c, d, rgb565(60, 64, 72));
  P(124, 0, a, b);
  glowDisc(a, b, 16 * sc, 120, 170, 255, 0.5f * glow);      // the pilot light: blue
  fillCircle((int)a, (int)b, (int)(4 * sc) + 1, rgb565(150, 200, 255));
  P(-60, -2, a, b);
  drawVectorText("FX-5", a, b, 1.2f * sc, rgb565(240, 200, 60), true);
}


// In a room (or on a walkway): his flames catching someone - he burns
static void flameHitNpcs() {
  for (int i = 0; i < npcCount; i++) {
    Npc &n = npcs[i];
    if (n.dead) continue;
    for (int k = 0; k < FLAMES; k++) {
      const Flame &f = flames[k];
      if (!f.on || f.age > f.life * 0.75f) continue;
      if (fabsf(f.x - n.x) < 30 * fmaxf(n.scale, 0.5f) && f.y > n.y - 170 * n.scale && f.y < n.y + 4) {
        n.burn = true;
        npcKilled(i, true);
        sfxPlay(SFX_SCREAM);
        break;
      }
    }
  }
}

// A body burning: flames on him standing, falling; charred black on the floor; ash crumbling; then nothing
static void drawBurning(const Npc &n) {
  float t = n.dieT, sc = AGENT_SCALE * n.scale;
  if (t < 0.9f) {
    AgentPose p = PERSON_IDLE;
    p.armN = 60 + 40 * sinf(t * 18); p.armF = 50 - 40 * sinf(t * 16);
    drawPersonU(p, n.x, n.y, n.dir, sc, t < 0.4f ? n.kind : PK_CHAR);
  } else if (t < 1.8f) {
    AgentPose p = PERSON_LIE;
    p.spin = -90;
    drawPersonU(p, n.x, n.y, n.dir, sc, PK_CHAR);
  } else if (t < 3.2f) {
    float k = (t - 1.8f) / 1.4f, w = 60 * n.scale * (1 - 0.6f * k), h = 18 * n.scale * (1 - k);
    for (int j = 0; j < 5; j++) fillCircle((int)(n.x - w + j * w / 2), (int)(n.y - h * 0.5f), (int)(h * 0.9f) + 1, rgb565(110, 106, 100));
    glowDisc(n.x, n.y - 30 - 60 * k, 30, 120, 116, 110, 0.3f * (1 - k));
    return;
  } else {
    return;
  }
  for (int j = 0; j < 7; j++) {                                // the flames on him
    float fx = n.x + (j - 3) * 9 * n.scale + 4 * sinf(clockT * 13 + j);
    float base = t < 0.9f ? n.y - 20 * n.scale - (j % 3) * 45 * n.scale : n.y - 10;
    float hgt = (18 + 14 * sinf(clockT * 17 + j * 2)) * n.scale * (t < 1.8f ? 1.0f : 0.5f);
    glowDisc(fx, base - hgt * 0.5f, 22 * n.scale, 255, 140, 40, 0.5f);
    int tx[3] = { (int)fx - 7, (int)fx + 7, (int)fx }, ty[3] = { (int)base, (int)base, (int)(base - hgt) };
    fillPolygon(tx, ty, 3, rgb565(255, 150, 30));
  }
}

// ---- lift C's second armory: the FX-5 INFERNO under glass, red light
static void paintArmory2() {
  paintArmory();
  fillRect(380, 140, 900, 200, rgb565(24, 8, 6));                 // its own sign over the first's
  drawRectOutline(380, 140, 900, 200, rgb565(255, 110, 40), 3);
  drawVectorText("EXPERIMENTAL ARMORY 2", 640, 170, 3.2f, rgb565(255, 170, 90), true);
  fillRect(560, 636, 720, 660, rgb565(14, 10, 8));
  drawVectorText("PROTOTYPE FX-5", 640, 648, 1.4f, rgb565(255, 150, 80), true);
  fillRect(620, CEIL_Y + 80, 660, CEIL_Y + 88, rgb565(255, 160, 90));
}
static void drawArmory2Anim() {
  float pulse = 0.5f + 0.5f * sinf(clockT * 2.6f);
  float bx[4] = { 622, 658, ARMORY_CASE_X1 + 30.0f, ARMORY_CASE_X0 - 30.0f }, by[4] = { CEIL_Y + 88.0f, CEIL_Y + 88.0f, 600, 600 };
  blendPolygonF(bx, by, 4, 255, 120, 50, 0.10f + 0.04f * pulse);
  glowDisc(640, 595, 160, 255, 110, 40, 0.18f + 0.08f * pulse);
  if (!flamerTaken) {
    float bob = 4 * sinf(clockT * 1.6f);
    glowDisc(640, 500 + bob, 110, 255, 120, 50, 0.22f + 0.1f * pulse);
    drawFlamer(640, 490 + bob, 0.72f, pulse);
  } else {
    drawVectorText("EMPTY", 640, 500, 2.0f, rgb565(170, 110, 80), true);
  }
  blendRect(ARMORY_CASE_X0, ARMORY_CASE_Y0, ARMORY_CASE_X1, ARMORY_CASE_Y1, 255, 200, 160, 0.10f);
  drawLine(ARMORY_CASE_X0 + 16, ARMORY_CASE_Y1 - 10, ARMORY_CASE_X0 + 70, ARMORY_CASE_Y0 + 10, rgb565(255, 230, 210), false);
  fillRect(560, 620, 720, 623, pulse > 0.5f ? rgb565(255, 150, 70) : rgb565(220, 80, 40));
}

static void updateMyShots(float dt) {
  for (int k = 0; k < MY_SHOTS; k++) {
    MyShot &b = myShots[k];
    if (!b.on) continue;
    float x0 = b.x;
    b.x += b.dir * MY_SHOT_SPEED * dt;
    if ((mode != MODE_ROOM && fgLoaded < 0) || b.x < -20 || b.x > SCREEN_W + 20) { b.on = false; continue; }
    for (int i = 0; i < npcCount; i++) {
      Npc &n = npcs[i];
      if (n.dead || fabsf(b.level - n.y) > 60) continue;
      float lo = fminf(x0, b.x) - 14, hi = fmaxf(x0, b.x) + 14;
      if (n.x < lo || n.x > hi) continue;
      b.on = false;
      if (b.ray) {                            // the QX-9 (it doesn't care about vests)
        xGunHit(i);
        break;
      }
      if (n.knifeOnly) {                      // the door guard's vest
        sfxPlay(SFX_CLUNK);
        showMsg("BULLET-PROOF VEST - ONLY THE KNIFE WILL DO", 2.5f);
        break;
      }
      npcKilled(i, true);
      break;
    }
  }
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
  } else if (s2Act == 6) {                   // searching the foyer guard: crouched over him, his card
    p.legN = 80; p.kneeN = -120; p.legF = 50; p.kneeF = -100;
    p.torso = 30 + 6 * sinf(t * 12); p.armN = 70 + 30 * sinf(t * 9); p.elbowN = 40; p.armF = 60; p.elbowF = 40;
    p.plant = 1;
    if ((int)(before / SEARCH_BEEP_SECONDS) != (int)(t / SEARCH_BEEP_SECONDS) && t < 1.1f) sfxPlay(SFX_BEEP);
    if (before < 1.1f && t >= 1.1f) {
      fgSearched[fgCard] = true;
      foundCard(3);
      addScore(1000, agX, CAR_FLOOR_Y - 170);
    }
    if (t >= 1.5f) {
      s2Act = 0;
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
      npcKilled(killTarget, false);
      if (n.knifeOnly && mode == MODE_ROOM && s2Kind[curRoom] == LK_SECURITY) {   // the door guard: the others will open fire
        secAlertT = SEC_ALERT_SECONDS;
        for (int k = 0; k < npcCount; k++) npcs[k].watching = false;
      }
    }
    if (t >= 0.8f) {
      s2Act = 0;
      setState(ST_IDLE, 0.2f);
    }
  } else if (s2Act == 5 && hasFlamer) {      // the FX-5: arm out, flames pouring out while it's held
    static const AgentPose AIMF = { 0, -2, 88, 2, 80, 14, 6, -4, -8, -3, 0, 0, 0, 1, 0 };
    p = AIMF;
    int hx, hy, mx, my;
    agentHand(AIMF, agX, agY, agFacing, myScale, hx, hy, mx, my);
    flameEmit((float)hx + agFacing * 40.0f, (float)hy - 3.0f, (float)agFacing, 0, dt);
    if (t >= 0.15f && !((held & (BTN_FIRE | BTN_USE)) && canFire())) {
      s2Act = 0;
      setState(ST_IDLE, 0.15f);
    }
  } else if (s2Act == 5) {                   // the pistol: arm out, fire
    static const AgentPose AIMP = { 0, -2, 88, 2, 80, 14, 6, -4, -8, -3, 0, 0, 0, 1, 0 };
    p = AIMP;
    if (before < 0.06f && t >= 0.06f) {      // a bullet out of the muzzle (the QX-9: a blue bolt)
      sfxPlay(hasXGun ? SFX_RAY : SFX_SHOT);
      gunFlashT = 0.07f;
      int hx, hy, mx, my;
      agentHand(AIMP, agX, agY, agFacing, myScale, hx, hy, mx, my);
      for (int k = 0; k < MY_SHOTS; k++)
        if (!myShots[k].on) {
          myShots[k] = { true, (float)hx + agFacing * 26.0f, (float)hy - 3.0f, agY, agFacing, hasXGun };
          break;
        }
    }
    if (t >= 0.32f) {
      if ((held & (BTN_FIRE | BTN_USE)) && canFire()) {   // held: keep firing
        s2ActT = 0;
      } else {
        s2Act = 0;
        setState(ST_IDLE, 0.15f);
      }
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
    if (n.fxT > 0) n.fxT -= dt;
    if (n.dead) {
      n.dieT += dt;
      continue;
    }
    if (n.foyer || n.rsold) {                // lift C's foyer guard: NUCLEAR MISSION's soldier (lift E's rooms too)
      bool see = (n.rsold ? (mode == MODE_ROOM && fabsf(agY - n.y) < 60) : onWalkway()) && agState != ST_KO && agState != ST_CAUGHT && gameOver == END_NONE;
      if (see) n.dir = agX < n.x ? -1 : 1;
      if (n.fstate == 0) {
        n.fcool -= dt;
        if (n.fcool <= 0 && see) { n.fstate = 1; n.aimT = 0.001f; }
      } else if (n.fstate == 1) {
        n.aimT += dt;
        if (n.aimT >= FG_AIM) {
          int hx, hy, mx, my;
          static const AgentPose AIM = { 0, -2, 88, 2, 80, 14, 6, -4, -8, -3, 0, 0, 0, 1, 0 };
          agentHand(AIM, n.x, n.y, n.dir, AGENT_SCALE * n.scale, hx, hy, mx, my);
          for (int k = 0; k < 4; k++)
            if (!gShots[k].on) { gShots[k] = { true, (float)mx, (float)my, n.dir, n.rsold ? agY - 78 : (float)my }; break; }
          sfxPlay(SFX_SHOT);
          shotFlashT = 0.1f;
          n.fstate = 2;
          n.t = 0;
        }
      } else if (n.t >= FG_RECOVER) {
        n.fstate = 0;
        n.aimT = 0;
        n.fcool = 1.0f + fxRnd(0, 80) / 100.0f;
      }
      continue;
    }
    if (n.aimT > 0) {                        // a guard drawing his pistol: then he fires
      float was = n.aimT;
      n.aimT += dt;
      if (was < GUARD_AIM_SECONDS && n.aimT >= GUARD_AIM_SECONDS) {
        sfxPlay(SFX_SHOT);
        shotFlashT = 0.12f;
        if (hasVest && health > SHOT_HURT) { // the vest: it hurts (half), he stays up, the guard fires again (till he's nearly done)
          hurt(SHOT_HURT / 2);
          showMsg("HIT - THE VEST HELD", 1.2f);
          n.aimT = 0.001f;
          continue;
        }
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
static bool s2NoSusp() { return mode == MODE_ROOM && curRoom < S2_ROOMS && (s2Kind[curRoom] == LK_SIMON || s2Kind[curRoom] == LK_DESTRUCT || s2Kind[curRoom] == LK_BOSS); }
static void updateSuspicion(float dt) {
  if (npcCount == 0 || s2Recog || agState == ST_CAUGHT || agState == ST_KO || s2Act || secAlertT >= 0 || s2NoSusp()) return;
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
    if (n.fxT > 0) glowDisc(n.x, n.y - 90 * n.scale, 90 * fmaxf(n.scale, 0.5f), 90, 180, 255, n.fxT * 1.4f);
    if (n.dead && n.burn) {                  // the FX-5's: burning, charred, ash, gone
      drawBurning(n);
      continue;
    }
    if (n.dead) {                            // killed: down, a pool spreading under him
      float f = fminf(1.0f, n.dieT / 0.5f);
      int pw = (int)((10 + 50 * fminf(1.0f, n.dieT / 3.0f)) * n.scale);
      if (n.dieT > 0.4f) fillRect((int)n.x - pw, (int)n.y - 3, (int)n.x + pw, (int)n.y - 1, rgb565(130, 10, 14));
      agentLerp(PERSON_IDLE, PERSON_LIE, f * f, p);
      p.spin = -90 * f * f;
      drawPersonU(p, n.x, n.y, n.dir, AGENT_SCALE * n.scale, n.kind);
      continue;
    }
    if (n.aimT > 0) {                        // a guard: pistol up, at him
      static const AgentPose AIM = { 0, -2, 88, 2, 80, 14, 6, -4, -8, -3, 0, 0, 0, 1, 0 };
      agentLerp(PERSON_IDLE, AIM, fminf(1.0f, n.aimT / 0.25f), p);
      drawPersonU(p, n.x, n.y, n.dir, AGENT_SCALE * n.scale, n.kind);
      if (shotFlashT > 0) {
        int hx, hy, mx, my;
        agentHand(p, n.x, n.y, n.dir, AGENT_SCALE * n.scale, hx, hy, mx, my);
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
    } else if (n.a == n.b) {                 // standing at his post: still
      p = PERSON_IDLE;
      p.head = 2 * sinf(n.t * 0.7f);
    } else if (n.pause > 0) {                // busy: reaching forward, working
      p = PERSON_IDLE;
      p.armN = 70 + 12 * sinf(n.t * 8); p.elbowN = 50; p.armF = 60 + 10 * sinf(n.t * 8 + 1.5f); p.elbowF = 56;
      p.torso = 8;
      p.head = 10;
    } else {
      personWalk(p, n.t);
    }
    drawPersonU(p, n.x, n.y, n.dir, AGENT_SCALE * n.scale, n.kind);
    if (n.kind == PK_CLEANER) {              // a mop in the near hand
      int hx, hy, mx, my;
      agentHand(p, n.x, n.y, n.dir, AGENT_SCALE * n.scale, hx, hy, mx, my);
      float fx = hx + n.dir * 34.0f;
      drawLineF((float)hx - n.dir * 6.0f, hy - 40.0f, fx, n.y - 8.0f, rgb565(170, 130, 80), 3.0f);
      fillRectF(fx - 16, n.y - 12, fx + 16, n.y - 1, rgb565(230, 230, 220));
      fillRectF(fx - 4, n.y - 16, fx + 4, n.y - 11, rgb565(60, 140, 200));
    }
    if (secAlertT >= 0 && n.kind == PK_SECURITY && !n.knifeOnly) {   // on alert: ! over their heads
      drawVectorTextW("!", n.x, n.y - 200 * fmaxf(n.scale, 0.6f), 5.0f, rgb565(255, 60, 40), 3.0f);
    } else if (n.watching && susp > 0) {     // ? over their head, redder with each point
      float k = (float)susp / SUSP_MAX;
      uint16_t c = rgb565(255, (int)(220 - 170 * k), (int)(80 - 40 * k));
      drawVectorTextW("?", n.x, n.y - 200 * fmaxf(n.scale, 0.6f), 4.0f + k * 2.0f, c, 2.6f);
    }
  }
}

static void drawFoyerGuardAt(int sh, int L, int fy) {   // (not the ones in npcs[]: standing at their posts, or where they fell)
  for (int k = 0; k < fgSlots[sh]; k++) {
    float sc = fgScale[sh][L][k];
    if (fgAsh[sh][L][k]) continue;           // (burnt to ash)
    if (fgDead[sh][L][k]) {
      AgentPose p = PERSON_LIE;
      p.spin = -90;
      float x = fgDX[sh][L][k];
      fillRect((int)x - (int)(60 * sc), fy - 3, (int)x + (int)(60 * sc), fy - 1, rgb565(130, 10, 14));
      drawPersonU(p, x, (float)fy, fgDDir[sh][L][k], AGENT_SCALE * sc, PK_GUARD);
    } else {
      drawPersonU(PERSON_IDLE, fgPostX[sh][k], (float)fy, -1, AGENT_SCALE * sc, PK_GUARD);
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
static bool panelGun = false, panelVest = false, panelXGun = false, panelFlamer = false, panelCode = false;
static void foundCard(int level) {          // (-1: the knife, -2: the pistol, -3: the vest, -4: the QX-9, -5: the FX-5)
  panelXGun = level == -4;
  panelFlamer = level == -5;
  panelKnife = level == -1;
  panelGun = level == -2;
  panelVest = level == -3;
  panelCode = level == -6;
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
  if (panelCode) {                           // the NUKE SELF DESTRUCT CODE
    drawVectorText("NUKE SELF DESTRUCT CODE", SCREEN_W / 2, y0 + 40, 3.2f, rgb565(255, 220, 80), true);
    fillRect(SCREEN_W / 2 - 200, y0 + 80, SCREEN_W / 2 + 200, y0 + 160, rgb565(30, 6, 6));
    drawRectOutline(SCREEN_W / 2 - 200, y0 + 80, SCREEN_W / 2 + 200, y0 + 160, rgb565(255, 80, 60), 2);
    drawVectorTextW(nukeCodeText, SCREEN_W / 2, y0 + 120, 5.0f, rgb565(255, 90, 70), 2.6f);
    drawVectorText("IT DESTROYS ONE MISSILE IN FLIGHT", SCREEN_W / 2, y1 - 56, 2.0f, rgb565(240, 244, 250), false);
    drawVectorText("USE IT IN THE ROOM BELOW (LIFT E)", SCREEN_W / 2, y1 - 26, 2.0f, rgb565(255, 220, 120), false);
    return;
  }
  if (panelFlamer) {                         // the FX-5
    drawVectorText("EXPERIMENTAL FLAMETHROWER FOUND", SCREEN_W / 2, y0 + 40, 3.0f, rgb565(255, 170, 90), true);
    glowDisc(SCREEN_W / 2, y0 + 130, 140, 255, 120, 50, 0.3f);
    drawFlamer(SCREEN_W / 2, y0 + 120, 1.3f, 0.5f + 0.5f * sinf(clockT * 3));
    drawVectorText("HOLD R1 OR R2 - THEY BURN TO ASH", SCREEN_W / 2, y1 - 52, 2.0f, rgb565(255, 220, 120), false);
    drawVectorText("(UNLIMITED FUEL - FOR NOW)", SCREEN_W / 2, y1 - 24, 1.7f, rgb565(190, 200, 220), false);
    return;
  }
  if (panelXGun) {                           // the QX-9
    drawVectorText("EXPERIMENTAL GUN FOUND", SCREEN_W / 2, y0 + 40, 3.2f, rgb565(140, 200, 255), true);
    glowDisc(SCREEN_W / 2, y0 + 130, 140, 80, 170, 255, 0.3f);
    drawXGun(SCREEN_W / 2, y0 + 126, 1.3f, 0.5f + 0.5f * sinf(clockT * 3));
    drawVectorText("A SHOT HALVES THEIR SIZE - 3 AND THEY'RE GONE", SCREEN_W / 2, y1 - 52, 2.0f, rgb565(255, 220, 120), false);
    drawVectorText("(NOW AND THEN IT DOUBLES THEM)   R1 OR R2 TO FIRE", SCREEN_W / 2, y1 - 24, 1.7f, rgb565(190, 200, 220), false);
    return;
  }
  if (panelVest) {                           // the vest
    drawVectorText("BULLET-PROOF VEST FOUND", SCREEN_W / 2, y0 + 40, 3.2f, rgb565(240, 244, 250), true);
    int kx = SCREEN_W / 2, ky = y0 + 130;
    int vx[8] = { kx - 60, kx - 30, kx - 14, kx + 14, kx + 30, kx + 60, kx + 56, kx - 56 };
    int vy[8] = { ky - 50, ky - 60, ky - 34, ky - 34, ky - 60, ky - 50, ky + 60, ky + 60 };
    fillPolygon(vx, vy, 8, rgb565(40, 46, 60));
    fillRect(kx - 48, ky - 4, kx + 48, ky + 10, rgb565(60, 68, 86));
    drawVectorText("POLICE", kx, ky + 30, 1.8f, rgb565(230, 236, 250), false);
    drawVectorText("YOU PUT IT ON - SHOTS HURT LESS   +500", SCREEN_W / 2, y1 - 40, 2.0f, rgb565(255, 220, 120), false);
    return;
  }
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

static int atGuardBody();
// What X / SQUARE would do here (stage 2's lift areas)
static void drawS2Prompts() {
  if (s2Act || agState != ST_IDLE || gameOver != END_NONE) return;
  const char *t = NULL;
  float x = agX;
  if (atWorker()) t = "X: KNOCK OUT";
  else if (atBody()) t = wkState == WK_DOWN ? "SQUARE: SEARCH" : "SQUARE: INTO THE BIN";
  else if (atGuardBody() == 1) t = "SQUARE: SEARCH";
  else if (atFoyerAid() && !s2Aid[curShaft][carFloor]) t = "SQUARE: FIRST AID";
  if (!t) return;
  float w = vectorTextWidth(t, 2.4f);
  x = clampv(x, w / 2 + 24, SCREEN_W - w / 2 - 24);
  blendRect((int)(x - w / 2 - 10), CAR_FLOOR_Y - 196, (int)(x + w / 2 + 10), CAR_FLOOR_Y - 172, 0, 0, 0, 0.55f);
  drawVectorText(t, x, CAR_FLOOR_Y - 184, 2.4f, rgb565(255, 220, 120), true);
}

// The clone lab's hot gas: which step of its sequence it's in (-1: none), and how far (0 warn, 1 blasting)
static int gasStep(float &into, int &phase) {
  if (gasT < 0) return -1;
  float t = fmodf(gasT, GAS_CYCLE);
  int st = (int)(t / GAS_STEP);
  if (st > 2) return -1;
  into = t - st * GAS_STEP;
  if (into < GAS_WARN) phase = 0;
  else if (into < GAS_WARN + GAS_BLAST) { phase = 1; into -= GAS_WARN; }
  else return -1;
  return st;
}
static void updateGas(float dt) {
  float was = gasT;
  gasT += dt;
  float into;
  int ph, st = gasStep(into, ph);
  if (st < 0) return;
  float into0;
  int ph0, st0 = was >= 0 ? (gasStep(into0, ph0)) : -1;
  if (ph == 1 && (st0 != st || ph0 != 1)) sfxPlay(SFX_HISS);       // (a blast starting)
  if (ph != 1 || agState == ST_KO || agState == ST_CAUGHT || gameOver != END_NONE) return;
  bool up = s2Kind[curRoom] == LK_QLAB2;                              // (Quantum Lab 2: up out of its two holes)
  if (!up && fabsf(agY - ROOM_FLOOR_Y) > 2) return;                   // (the clone lab: only on the floor; not on the deck)
  int blastId = (int)(gasT / GAS_CYCLE) * 3 + st;
  if (gasHitStep == blastId) return;
  for (int k = 0; k < (up ? 1 : 2); k++) {
    float spread = up ? 30 + 34 * fminf(1.0f, into / 0.3f) : 26 + 14 * fminf(1.0f, into / 0.3f);
    float vx = up ? (float)holeGasX[holeGasSeq[st]] : (float)gasX[gasSeq[st][k]];
    if (fabsf(agX - vx) < spread) {
      gasHitStep = blastId;
      hurt(GAS_HURT);
      sfxPlay(SFX_SCREAM);
      showMsg("SCALDED BY HOT GAS", 1.6f);
      break;
    }
  }
}

static void drawHoleGas();
static void drawGas() {
  if (s2Kind[curRoom] == LK_QLAB2) {
    drawHoleGas();
    return;
  }
  float into = 0;
  int ph = 0, st = gasStep(into, ph);
  int ny = CLONE_DECK_Y + PLAT_T + 44;                                  // (the nozzles' mouths)
  for (int v = 0; v < GAS_VENTS; v++) {
    bool on = st >= 0 && (gasSeq[st][0] == v || gasSeq[st][1] == v);
    bool lit = on && (ph == 1 || ((int)(into * 10) & 1));
    fillRect(gasX[v] + 16, ny - 16, gasX[v] + 26, ny - 10, lit ? rgb565(255, 50, 40) : rgb565(90, 20, 20));   // its lamp
    if (lit) glowDisc(gasX[v] + 21, ny - 13, 18, 255, 60, 40, 0.35f);
    if (!on) continue;
    if (ph == 0) {                                                       // the warning: wisps
      for (int k = 0; k < 3; k++) {
        float u = fmodf(into * 1.6f + k * 0.33f, 1.0f);
        glowDisc(gasX[v] + (k - 1) * 6, ny + u * 30, 6 + u * 6, 230, 220, 200, 0.25f * (1 - u));
      }
      continue;
    }
    float grow = fminf(1.0f, into / 0.25f), fade = into > GAS_BLAST - 0.3f ? (GAS_BLAST - into) / 0.3f : 1.0f;
    float bottom = ny + (ROOM_FLOOR_Y - ny) * grow;
    for (int y = ny; y < bottom; y += 4) {                               // the column: widening, hot at the core
      float k = (y - ny) / (float)(ROOM_FLOOR_Y - ny), w = 12 + 30 * k;
      blendRect(gasX[v] - (int)w, y, gasX[v] + (int)w, y + 3, 240, 236, 226, 0.45f * fade);
      blendRect(gasX[v] - (int)(w * 0.45f), y, gasX[v] + (int)(w * 0.45f), y + 3, 255, 190, 120, 0.45f * fade);
    }
    for (int k = 0; k < 6; k++) {                                        // puffs rushing down
      float u = fmodf(clockT * 2.2f + k / 6.0f + v * 0.13f, 1.0f), y = ny + u * (bottom - ny);
      glowDisc(gasX[v] + 10 * sinf(clockT * 7 + k * 2), y, 14 + 20 * u, 255, 245, 230, 0.32f * fade);
    }
    if (grow >= 1) for (int s2 = -1; s2 <= 1; s2 += 2)                   // spreading along the floor
      glowDisc(gasX[v] + s2 * (30 + 20 * fmodf(clockT * 3, 1.0f)), ROOM_FLOOR_Y - 14, 30, 250, 240, 225, 0.35f * fade);
  }
}

// Quantum Lab 2's gas: up out of a hole in the floor, one at a time - left, left, right
static void drawHoleGas() {
  float into = 0;
  int ph = 0, st = gasStep(into, ph);
  if (st < 0) return;
  int x = holeGasX[holeGasSeq[st]], top = CEIL_Y + 60, base = ROOM_FLOOR_Y + 30;
  if (ph == 0) {                                                         // the warning: the hole glowing, wisps rising
    glowDisc(x, ROOM_FLOOR_Y + 10, 60, 255, 120, 40, 0.25f + 0.2f * ((int)(into * 10) & 1));
    for (int k = 0; k < 4; k++) {
      float u = fmodf(into * 1.4f + k * 0.25f, 1.0f);
      glowDisc(x + (k - 1.5f) * 14, ROOM_FLOOR_Y - u * 80, 8 + u * 10, 230, 220, 200, 0.28f * (1 - u));
    }
    return;
  }
  float grow = fminf(1.0f, into / 0.25f), fade = into > GAS_BLAST - 0.3f ? (GAS_BLAST - into) / 0.3f : 1.0f;
  float y1 = base - (base - top) * grow;
  for (int y = base; y > y1; y -= 4) {                                   // the column: wide as the hole, widening up
    float k = (base - y) / (float)(base - top), w = 44 + 26 * k;
    blendRect(x - (int)w, y - 3, x + (int)w, y, 240, 236, 226, 0.45f * fade);
    blendRect(x - (int)(w * 0.45f), y - 3, x + (int)(w * 0.45f), y, 255, 190, 120, 0.45f * fade);
  }
  for (int k = 0; k < 7; k++) {                                          // puffs rushing up
    float u = fmodf(clockT * 2.2f + k / 7.0f, 1.0f), y = base - u * (base - y1);
    glowDisc(x + 14 * sinf(clockT * 7 + k * 2), y, 18 + 22 * u, 255, 245, 230, 0.32f * fade);
  }
  if (grow >= 1) for (int s2 = -1; s2 <= 1; s2 += 2)                     // billowing along the ceiling
    glowDisc(x + s2 * (40 + 30 * fmodf(clockT * 3, 1.0f)), top + 20, 40, 250, 240, 225, 0.35f * fade);
}


// ============================================================ lift D's rooms: SECURITY ZONES, seen from above
// A different game: the room from above, him walking any of 8 ways, from the door at the bottom left to the
// one at the top right (or back). Desks, crates, cabinets in the way - cover for both sides. Soldiers hold
// the room and more come in through the door at the top in threes, in a V, till 15 have come; each one
// that sees him aims (0.55s, NUCLEAR MISSION's) and fires, and again every 1 to 1.8 seconds. He fires back
// the way he's facing (R1 / R2 or Square): the pistol, the QX-9, or the FX-5's flames (they burn to ash).
#define AR_X0 30
#define AR_X1 1250
#define AR_Y0 70
#define AR_Y1 700
#define AR_DOOR_L0 588                       // the left door (bottom left): its gap in the left wall
#define AR_DOOR_L1 684
#define AR_DOOR_R0 86                        // the right door (top right): its gap in the right wall
#define AR_DOOR_R1 182
#define AR_SPAWN_X0 580                      // the soldiers' doors: one in the top wall...
#define AR_SPAWN_X1 700
#define AR_SPAWN2_X0 940                     // ...one in the bottom wall (he can't go through either)
#define AR_SPAWN2_X1 1060
#define AR_R 21                              // (him, them: their radius)
#define AR_SC 1.35f                          // (and how big they're drawn)
#define AR_SPEED 260.0f
#define AR_GUARD_SPEED 150.0f
#define AR_TOTAL 15                          // soldiers a room, all told
#define AR_GMAX 16                           // (room for all 15: the shot stay where they fell)
#define AR_SHOTS 24
#define AR_KILL_SCORE 250
enum ObKind : uint8_t { OB_DESK, OB_CRATE, OB_CABINET, OB_SOFA, OB_PILLAR, OB_TABLE };
struct Obstacle { int16_t x0, y0, x1, y1; uint8_t kind; };
static const Obstacle arObs[] = {
  { 200, 440, 340, 490, OB_DESK },   { 400, 560, 470, 630, OB_CRATE },  { 470, 590, 530, 650, OB_CRATE },
  { 560, 330, 710, 380, OB_DESK },   { 820, 470, 870, 640, OB_CABINET }, { 900, 220, 980, 300, OB_CRATE },
  { 1000, 420, 1150, 476, OB_SOFA }, { 290, 190, 350, 250, OB_PILLAR }, { 690, 540, 800, 600, OB_TABLE },
  { 1080, 250, 1130, 300, OB_PILLAR }, { 140, 290, 200, 350, OB_CRATE } };
#define AR_OBS ((int)(sizeof(arObs) / sizeof(arObs[0])))
struct AGuard { bool on, dead, burn, walking; float x, y, tx, ty, face, aimT, cool, t, dieT, scale, strafeT; uint8_t fstate; };
struct AShot { bool on, mine, ray; float x, y, vx, vy; int dmg; };
static AGuard ag[AR_GMAX];
static AShot ash[AR_SHOTS];
static int arSpawned = 0;
static float apX = 0, apY = 0, apFace = 0, apWalkT = 0, apFireT = 0, arWaveT = 0, arFlashT = 0;
static bool arOn() { return stage == 2 && mode == MODE_ROOM && curRoom < S2_ROOMS && s2Kind[curRoom] == LK_ARENA; }

static bool arBlocked(float x, float y, float r) {   // in a wall (not a door's gap) or something
  for (int i = 0; i < AR_OBS; i++) {
    const Obstacle &o = arObs[i];
    float cx = clampv(x, (float)o.x0, (float)o.x1), cy = clampv(y, (float)o.y0, (float)o.y1);
    if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r) return true;
  }
  if (y - r < AR_Y0) return true;            // (the soldiers' doors too: shut to him)
  if (y + r > AR_Y1) return true;
  if (x - r < AR_X0 && !(y - r > AR_DOOR_L0 && y + r < AR_DOOR_L1)) return true;
  if (x + r > AR_X1 && !(y - r > AR_DOOR_R0 && y + r < AR_DOOR_R1 && !rooms[curRoom].noRightDoor)) return true;
  return false;
}
static bool arSolid(float x, float y) {      // (for shots and flames: inside something or past a wall)
  if (x < AR_X0 || x > AR_X1 || y < AR_Y0 || y > AR_Y1) return true;
  for (int i = 0; i < AR_OBS; i++) {
    const Obstacle &o = arObs[i];
    if (x >= o.x0 && x <= o.x1 && y >= o.y0 && y <= o.y1) return true;
  }
  return false;
}
static bool arSight(float x0, float y0, float x1, float y1) {   // a clear line between
  float d = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
  int n = (int)(d / 12) + 1;
  for (int k = 1; k < n; k++) {
    float u = k / (float)n, x = x0 + (x1 - x0) * u, y = y0 + (y1 - y0) * u;
    for (int i = 0; i < AR_OBS; i++) {
      const Obstacle &o = arObs[i];
      if (x >= o.x0 && x <= o.x1 && y >= o.y0 && y <= o.y1) return false;
    }
  }
  return true;
}

static void arSpawnGuard(float x, float y, float tx, float ty, bool walking) {
  for (int i = 0; i < AR_GMAX; i++)
    if (!ag[i].on) {
      AGuard &g = ag[i];
      memset(&g, 0, sizeof(g));
      g.on = true;
      g.x = x; g.y = y; g.tx = tx; g.ty = ty;
      g.walking = walking;
      g.face = atan2f(ty - y, tx - x);
      g.scale = 1;
      g.cool = 1.4f + i * 0.35f;
      g.strafeT = frnd(1.5f, 3.0f);
      arSpawned++;
      return;
    }
}
static void arWave() {                       // three in through one of the two doors (at random), in a V, to a spot with cover
  static const float spots[3][2] = { { 640, 250 }, { 470, 300 }, { 820, 330 } }, spots2[2][2] = { { 1060, 560 }, { 900, 350 } };
  bool bottom = fxRnd(0, 2) == 1;
  const float *c = bottom ? spots2[fxRnd(0, 2)] : spots[fxRnd(0, 3)];
  static const float form[3][2] = { { 0, 0 }, { -90, 40 }, { 90, 40 } };
  int n = (AR_TOTAL - arSpawned < 3 ? AR_TOTAL - arSpawned : 3);
  for (int k = 0; k < n; k++) {
    if (bottom) arSpawnGuard((AR_SPAWN2_X0 + AR_SPAWN2_X1) / 2 + form[k][0] * 0.5f, AR_Y1 + 30 + form[k][1], c[0] + form[k][0], c[1] + form[k][1], true);
    else arSpawnGuard((AR_SPAWN_X0 + AR_SPAWN_X1) / 2 + form[k][0] * 0.5f, AR_Y0 - 30 - form[k][1], c[0] + form[k][0], c[1] + form[k][1], true);
  }
  sfxPlay(SFX_ALARM);
  showMsg(bottom ? "MORE SOLDIERS - THE BOTTOM DOOR!" : "MORE SOLDIERS - THE TOP DOOR!", 1.8f);
}

// ---- searching what's in the room (Square, held, beside it); one of the 3 rooms has the ACCESS CARD LEVEL 5
static float arProg[16];
static int arSearchAt = -1;
static int arObsAt() {                       // the thing he's beside (or -1)
  for (int i = 0; i < AR_OBS; i++) {
    const Obstacle &o = arObs[i];
    float cx = clampv(apX, (float)o.x0, (float)o.x1), cy = clampv(apY, (float)o.y0, (float)o.y1);
    if (hypotf(apX - cx, apY - cy) < AR_R + 16) return i;
  }
  return -1;
}

// ---- the extra hazards: one room has a drone, one a wall turret's homing missiles (the third, just soldiers)
#define TD_SCAN_LEN 470.0f                   // the drone: its beam's reach...
#define TD_SCAN_HALF 0.45f                   // ...and half its width (radians)
#define TD_SPEED 119.0f                      // (170, less 30%)
#define TD_KEEP 170.0f                       // (how close it comes)
#define TD_FIRE_EVERY 0.8f
static float tdX, tdY, tdAng, tdFireT, tdLostT, tdGrace, tdPatrolT;
static bool tdChase = false;
#define TU_X 800                             // the turret: on the top wall, right of middle
#define TU_Y (AR_Y0 - 2)
#define TU_EVERY 10.0f                       // a missile every 10 seconds...
#define TU_WARN 1.0f                         // ...its light flashing for the last one
#define MS_SPEED 235.0f                      // (he's quicker: 260)
#define MS_TURN 2.4f                         // (radians a second)
#define MS_LIFE 4.0f                         // (then it's spent)
#define MS_HURT 15
static float tuT, tuAng, msX, msY, msAng, msLife, exT = -1, exX, exY;
static bool msOn = false;
static float msTrail[12][2];
static int msTrailN = 0;
static bool arTall(float x, float y) {        // (what's tall enough to stop a missile or hide him from the drone: walls, pillars, cabinets)
  if (x < AR_X0 || x > AR_X1 || y < AR_Y0 || y > AR_Y1) return true;
  for (int i = 0; i < AR_OBS; i++) {
    const Obstacle &o = arObs[i];
    if ((o.kind == OB_PILLAR || o.kind == OB_CABINET) && x >= o.x0 && x <= o.x1 && y >= o.y0 && y <= o.y1) return true;
  }
  return false;
}
static bool tdSight(float x0, float y0, float x1, float y1) {
  int n = (int)(hypotf(x1 - x0, y1 - y0) / 12) + 1;
  for (int k = 1; k < n; k++) if (arTall(x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n)) return false;
  return true;
}
static void arKill(int i, int how);
static void arEllipse(float cx, float cy, float rx, float ry, float ang, uint16_t c);
static void msExplode(bool hurtHim) {
  msOn = false;
  exT = 0; exX = msX; exY = msY;
  sfxPlay(SFX_BOOM);
  if (hurtHim) { hurt(MS_HURT); showMsg("HIT BY THE MISSILE!", 1.4f); }
  for (int i = 0; i < AR_GMAX; i++) {        // (it doesn't care whose it hits)
    AGuard &g = ag[i];
    if (g.on && !g.dead && hypotf(g.x - msX, g.y - msY) < 60) arKill(i, 1);
  }
}
static void updateArHazards(float dt) {
  uint8_t hz = arHaz[curRoom];
  if (hz == AH_DRONE) {
    if (tdGrace > 0) { tdGrace -= dt; return; }
    float d = hypotf(apX - tdX, apY - tdY);
    if (!tdChase) {                          // patrolling, the beam sweeping round
      tdPatrolT += dt;
      float px = 640 + 450 * cosf(tdPatrolT * 0.21f), py = 385 + 240 * sinf(tdPatrolT * 0.21f);
      tdX += (px - tdX) * fminf(1.0f, dt * 0.84f);
      tdY += (py - tdY) * fminf(1.0f, dt * 0.84f);
      tdAng += 1.1f * dt;
      float a = atan2f(apY - tdY, apX - tdX), da = fabsf(remainderf(a - tdAng, 6.2832f));
      if (d < TD_SCAN_LEN && da < TD_SCAN_HALF && tdSight(tdX, tdY, apX, apY)) {
        tdChase = true;
        tdLostT = 0;
        tdFireT = 0.5f;
        sfxPlay(SFX_ALARM);
        showMsg("THE DRONE HAS SEEN YOU", 1.6f);
      }
    } else {                                 // after him: closing in, firing
      tdAng = atan2f(apY - tdY, apX - tdX);
      if (d > TD_KEEP) { tdX += cosf(tdAng) * TD_SPEED * dt; tdY += sinf(tdAng) * TD_SPEED * dt; }
      bool see = tdSight(tdX, tdY, apX, apY);
      tdFireT -= dt;
      if (see && d < 520 && tdFireT <= 0) {
        tdFireT = TD_FIRE_EVERY;
        for (int k = 0; k < AR_SHOTS; k++)
          if (!ash[k].on) { ash[k] = { true, false, false, tdX + cosf(tdAng) * 22, tdY + sinf(tdAng) * 22, cosf(tdAng) * 560, sinf(tdAng) * 560, BULLET_HURT }; break; }
        sfxPlay(SFX_SHOT);
      }
      tdLostT = (see && d < 650) ? 0 : tdLostT + dt;
      if (tdLostT > 2.5f) tdChase = false;   // (lost him: back to its round)
    }
  } else if (hz == AH_TURRET) {
    tuAng = atan2f(apY - TU_Y, apX - TU_X);
    tuT += dt;
    if (tuT >= TU_EVERY - TU_WARN && tuT - dt < TU_EVERY - TU_WARN) sfxPlay(SFX_WARN);
    if (tuT >= TU_EVERY) {
      tuT = 0;
      msOn = true;
      msX = TU_X + cosf(tuAng) * 40; msY = TU_Y + sinf(tuAng) * 40;
      msAng = tuAng;
      msLife = MS_LIFE;
      msTrailN = 0;
      sfxPlay(SFX_IGNITE);
      showMsg("MISSILE - RUN!", 1.2f);
    }
  }
  if (msOn) {                                // the missile: turning after him, till it's spent
    float want = atan2f(apY - msY, apX - msX), da = remainderf(want - msAng, 6.2832f), turn = MS_TURN * dt;
    msAng += clampv(da, -turn, turn);
    msX += cosf(msAng) * MS_SPEED * dt;
    msY += sinf(msAng) * MS_SPEED * dt;
    if (msTrailN < 12) msTrailN++;
    for (int k = msTrailN - 1; k > 0; k--) { msTrail[k][0] = msTrail[k - 1][0]; msTrail[k][1] = msTrail[k - 1][1]; }
    msTrail[0][0] = msX; msTrail[0][1] = msY;
    msLife -= dt;
    if (hypotf(apX - msX, apY - msY) < AR_R + 12) msExplode(true);
    else if (arTall(msX, msY)) msExplode(hypotf(apX - msX, apY - msY) < 70);
    else if (msLife <= 0) { msOn = false; exT = 0; exX = msX; exY = msY; sfxPlay(SFX_HIT); showMsg("IT RAN OUT", 1.0f); }
  }
  if (exT >= 0) { exT += dt; if (exT > 0.8f) exT = -1; }
}
static void drawArHazards() {
  uint8_t hz = arHaz[curRoom];
  if (hz == AH_DRONE) {
    float half = TD_SCAN_HALF, len = tdChase ? 240.0f : TD_SCAN_LEN;
    float xs[3] = { tdX, tdX + cosf(tdAng - half) * len, tdX + cosf(tdAng + half) * len }, ys[3] = { tdY, tdY + sinf(tdAng - half) * len, tdY + sinf(tdAng + half) * len };
    if (tdGrace <= 0) blendPolygonF(xs, ys, 3, tdChase ? 255 : 255, tdChase ? 60 : 230, tdChase ? 40 : 90, tdChase ? 0.14f : 0.12f);
    arEllipse(tdX + 10, tdY + 14, 30, 22, 0, rgb565(22, 24, 28));       // its shadow, on the floor below
    for (int k = 0; k < 4; k++) {                                      // the rotors, blurred
      float a = 0.785f + k * 1.5708f, rx = tdX + cosf(a) * 24, ry = tdY + sinf(a) * 24;
      drawLineF(tdX, tdY, rx, ry, rgb565(60, 64, 72), 4);
      glowDisc(rx, ry, 14, 200, 210, 220, 0.25f);
      drawCircle((int)rx, (int)ry, 11, rgb565(150, 156, 166));
    }
    fillCircle((int)tdX, (int)tdY, 15, rgb565(60, 64, 74));
    fillCircle((int)tdX, (int)tdY, 11, rgb565(90, 96, 108));
    fillCircle((int)(tdX + cosf(tdAng) * 9), (int)(tdY + sinf(tdAng) * 9), 4, tdChase ? rgb565(255, 50, 40) : rgb565(80, 255, 120));
  } else if (hz == AH_TURRET) {
    fillRect(TU_X - 34, HUD_H + 4, TU_X + 34, AR_Y0 + 14, rgb565(60, 64, 74));   // its mount on the wall
    drawRectOutline(TU_X - 34, HUD_H + 4, TU_X + 34, AR_Y0 + 14, rgb565(110, 116, 128), 2);
    drawLineF((float)TU_X, (float)TU_Y, TU_X + cosf(tuAng) * 40, TU_Y + sinf(tuAng) * 40, rgb565(40, 44, 52), 10);   // its launcher, following him
    fillCircle(TU_X, TU_Y, 16, rgb565(90, 96, 108));
    bool warn = tuT >= TU_EVERY - TU_WARN;
    fillCircle(TU_X, TU_Y, 6, warn && ((int)(clockT * 10) & 1) ? rgb565(255, 50, 40) : rgb565(110, 30, 30));
  }
  if (msOn) {
    for (int k = msTrailN - 1; k >= 1; k--)                            // its smoke
      glowDisc(msTrail[k][0], msTrail[k][1], 8 + k * 1.5f, 180, 180, 176, 0.35f * (1 - k / 12.0f));
    float ca = cosf(msAng), sa = sinf(msAng);
    glowDisc(msX - ca * 22, msY - sa * 22, 22, 255, 170, 60, 0.8f);
    drawLineF(msX - ca * 20, msY - sa * 20, msX + ca * 14, msY + sa * 14, rgb565(220, 224, 230), 10);
    drawLineF(msX - ca * 20 - sa * 8, msY - sa * 20 + ca * 8, msX - ca * 20 + sa * 8, msY - sa * 20 - ca * 8, rgb565(170, 40, 30), 4);   // (its fins)
    fillCircle((int)(msX + ca * 14), (int)(msY + sa * 14), 5, rgb565(220, 40, 30));
  }
  if (exT >= 0) {
    float k = exT / 0.8f;
    glowDisc(exX, exY, 30 + 90 * k, 255, 190, 80, 0.9f * (1 - k));
    glowDisc(exX, exY, 20 + 40 * k, 255, 250, 210, 0.8f * (1 - k));
  }
}

static void arenaEnter(bool fromLeft) {
  memset(ag, 0, sizeof(ag));
  memset(ash, 0, sizeof(ash));
  for (int i = 0; i < FLAMES; i++) flames[i].on = false;
  apX = fromLeft ? 70.0f : 1210.0f;
  apY = fromLeft ? 636.0f : 134.0f;
  apFace = fromLeft ? 0.0f : 3.14159f;
  apWalkT = apFireT = arWaveT = arFlashT = 0;
  memset(arProg, 0, sizeof(arProg));
  tdX = fromLeft ? 1100.0f : 200.0f; tdY = fromLeft ? 200.0f : 600.0f;   // (the drone: the far side, a grace before it starts)
  tdAng = 0; tdChase = false; tdGrace = 3.0f; tdPatrolT = fromLeft ? 5.5f : 2.4f; tdFireT = 0;
  tuT = 3.0f; tuAng = 1.57f; msOn = false; exT = -1;                     // (the turret: its first missile 7 seconds in)
  arSpawned = arKilled[curRoom];
  static const float posts[3][2] = { { 640, 250 }, { 960, 380 }, { 1100, 560 } };
  int n = (AR_TOTAL - arSpawned < 3 ? AR_TOTAL - arSpawned : 3);
  for (int k = 0; k < n; k++) arSpawnGuard(posts[k][0], posts[k][1], posts[k][0], posts[k][1], false);
  if (arSpawned >= AR_TOTAL && n == 0) showMsg("THE ROOM IS CLEAR", 2.0f);
  else showMsg("GET TO THE DOOR AT THE TOP RIGHT", 2.5f);
}

static void arKill(int i, int how) {         // (how: 0 shot / knifed, 1 burnt, 2 shrunk to nothing)
  AGuard &g = ag[i];
  g.dead = true;
  g.burn = how == 1;
  g.dieT = 0;
  if (how == 2) g.dieT = 9;                  // (gone)
  arKilled[curRoom]++;
  addScore(AR_KILL_SCORE, g.x, g.y - 30);
  sfxPlay(how == 1 ? SFX_SCREAM : SFX_HIT);
  if (arKilled[curRoom] >= AR_TOTAL) showMsg("ALL 15 DOWN - THE ROOM IS CLEAR", 2.5f);
}

static void updateArena(float dt) {
  if (agState == ST_KO || gameOver != END_NONE) return;
  // him: 8 ways
  float dx = ((held & BTN_RIGHT) ? 1.0f : 0.0f) - ((held & BTN_LEFT) ? 1.0f : 0.0f);
  float dy = ((held & BTN_DOWN) ? 1.0f : 0.0f) - ((held & BTN_UP) ? 1.0f : 0.0f);
  if (dx != 0 || dy != 0) {
    float l = sqrtf(dx * dx + dy * dy);
    dx /= l; dy /= l;
    apFace = atan2f(dy, dx);
    float nx = apX + dx * AR_SPEED * dt, ny = apY + dy * AR_SPEED * dt;
    if (!arBlocked(nx, apY, AR_R)) apX = nx;
    if (!arBlocked(apX, ny, AR_R)) apY = ny;
    apWalkT += dt;
  }
  agX = apX;                                 // (for anything that asks where he is)
  // out through a door
  if (apX < AR_X0 - 6) { leaveThroughEdge(false); return; }
  if (apX > AR_X1 + 6) { leaveThroughEdge(true); return; }
  // firing, the way he faces
  float fx = cosf(apFace), fy = sinf(apFace), mx = apX + fx * 46, my = apY + fy * 46;
  if (apFireT > 0) apFireT -= dt;
  if (arFlashT > 0) arFlashT -= dt;
  bool trig = (held & (BTN_FIRE | BTN_USE)) != 0;   // (R1 / R2 or Square; X, in here, is for searching)
  if (trig && hasFlamer) {
    flameEmit(mx, my, fx, fy, dt);
  } else if (trig && (hasGun || hasXGun) && apFireT <= 0) {
    apFireT = 0.32f;
    for (int k = 0; k < AR_SHOTS; k++)
      if (!ash[k].on) {
        float sp = hasXGun ? 700.0f : 900.0f;
        ash[k] = { true, true, hasXGun, mx, my, fx * sp, fy * sp, 0 };
        break;
      }
    sfxPlay(hasXGun ? SFX_RAY : SFX_SHOT);
    arFlashT = 0.07f;
  } else if ((pressed & (BTN_FIRE | BTN_USE)) && !hasGun && !hasXGun && !hasFlamer) {
    showMsg(hasKnife ? "NO GUN - X TO USE THE KNIFE UP CLOSE" : "NO WEAPON - KEEP MOVING", 1.6f);
  }
  // the knife (X, a soldier up close) - otherwise X held beside something searches it
  bool knifed = false;
  if ((pressed & BTN_JUMP) && hasKnife)
    for (int i = 0; i < AR_GMAX; i++) {
      AGuard &g = ag[i];
      if (g.on && !g.dead && hypotf(g.x - apX, g.y - apY) < 52) { arKill(i, 0); knifed = true; break; }
    }
  arSearchAt = arObsAt();
  if (!knifed && arSearchAt >= 0 && !arSearched[curRoom][arSearchAt] && (held & BTN_JUMP)) {
    float was = arProg[arSearchAt];
    arProg[arSearchAt] += dt;
    if ((int)(was / SEARCH_BEEP_SECONDS) != (int)(arProg[arSearchAt] / SEARCH_BEEP_SECONDS)) sfxPlay(SFX_BEEP);
    if (arProg[arSearchAt] >= SEARCH_SECONDS) {
      arSearched[curRoom][arSearchAt] = true;
      if (curRoom == arCardRoom && arSearchAt == arCardObs) foundCard(5);
      else {
        snprintf(floatText, sizeof(floatText), "NOTHING");
        floatColour = rgb565(150, 156, 170);
        nothingShowT = NOTHING_SHOW; nothingX = apX - 40; nothingY = apY - 50;
        sfxPlay(SFX_BEEP);
      }
    }
  }
  // the flames: past walls and through things they stop; they burn whoever they reach
  for (int k = 0; k < FLAMES; k++) {
    Flame &f = flames[k];
    if (!f.on) continue;
    if (arSolid(f.x, f.y)) { f.on = false; continue; }
    if (f.age > f.life * 0.75f) continue;
    for (int i = 0; i < AR_GMAX; i++) {
      AGuard &g = ag[i];
      if (g.on && !g.dead && hypotf(g.x - f.x, g.y - f.y) < 30 * fmaxf(g.scale, 0.6f)) { arKill(i, 1); break; }
    }
  }
  // the shots
  for (int k = 0; k < AR_SHOTS; k++) {
    AShot &s = ash[k];
    if (!s.on) continue;
    s.x += s.vx * dt;
    s.y += s.vy * dt;
    if (arSolid(s.x, s.y)) { s.on = false; continue; }
    if (s.mine) {
      for (int i = 0; i < AR_GMAX; i++) {
        AGuard &g = ag[i];
        if (!g.on || g.dead || hypotf(g.x - s.x, g.y - s.y) > (AR_R + 4) * fmaxf(g.scale, 0.5f)) continue;
        s.on = false;
        if (s.ray) {
          if (fxRnd(0, 10) == 0) { g.scale = fminf(3.0f, g.scale * 2); showMsg("DOUBLED IN SIZE!", 1.4f); }
          else if ((g.scale *= 0.5f) < 0.3f) arKill(i, 2);
          sfxPlay(SFX_POWERUP);
        } else {
          arKill(i, 0);
        }
        break;
      }
    } else if (hypotf(apX - s.x, apY - s.y) < AR_R + 3) {
      s.on = false;
      if (hasVest) { hurt(s.dmg / 2 > 0 ? s.dmg / 2 : 1); showMsg("HIT - THE VEST HELD", 1.2f); }
      else { hurt(s.dmg); showMsg("SHOT!", 1.0f); }
      sfxPlay(SFX_HIT);
    }
  }
  updateArHazards(dt);
  // the soldiers
  int alive = 0;
  for (int i = 0; i < AR_GMAX; i++) {
    AGuard &g = ag[i];
    if (!g.on) continue;
    g.t += dt;
    if (g.dead) {
      g.dieT += dt;
      if (g.burn && g.dieT > 3.5f) g.on = false;
      continue;
    }
    alive++;
    if (g.walking) {                         // in through the door, to his spot
      float ddx = g.tx - g.x, ddy = g.ty - g.y, d = sqrtf(ddx * ddx + ddy * ddy);
      if (d < 4) { g.walking = false; g.cool = frnd(0.6f, 1.4f); }
      else { g.x += ddx / d * AR_GUARD_SPEED * dt; g.y += ddy / d * AR_GUARD_SPEED * dt; g.face = atan2f(ddy, ddx); }
      continue;
    }
    bool see = arSight(g.x, g.y, apX, apY) && hypotf(apX - g.x, apY - g.y) < 1100;
    if (see) g.face = atan2f(apY - g.y, apX - g.x);
    if (g.fstate == 0) {
      g.cool -= dt;
      g.strafeT -= dt;
      if (g.strafeT <= 0) {                  // a step to one side now and then
        g.strafeT = frnd(1.8f, 3.2f);
        float a = g.face + (fxRnd(0, 2) ? 1.57f : -1.57f), nx = g.x + cosf(a) * 50, ny = g.y + sinf(a) * 50;
        if (!arBlocked(nx, ny, AR_R + 2)) { g.tx = nx; g.ty = ny; }
      }
      float ddx = g.tx - g.x, ddy = g.ty - g.y, d = sqrtf(ddx * ddx + ddy * ddy);
      if (d > 2) { g.x += ddx / d * 70 * dt; g.y += ddy / d * 70 * dt; }
      if (g.cool <= 0 && see) { g.fstate = 1; g.aimT = 0.001f; }
    } else if (g.fstate == 1) {
      g.aimT += dt;
      if (g.aimT >= FG_AIM) {
        float a = g.face, sp = 520;
        for (int k = 0; k < AR_SHOTS; k++)
          if (!ash[k].on) { ash[k] = { true, false, false, g.x + cosf(a) * 30 * g.scale, g.y + sinf(a) * 30 * g.scale, cosf(a) * sp, sinf(a) * sp, SHOT_HURT }; break; }
        sfxPlay(SFX_SHOT);
        g.fstate = 2;
        g.t = 0;
      }
    } else if (g.t >= FG_RECOVER) {
      g.fstate = 0;
      g.aimT = 0;
      g.cool = frnd(1.0f, 1.8f);
    }
  }
  // the next three, when he's down to one (till 15 have come)
  if (alive <= 1 && arSpawned < AR_TOTAL) {
    arWaveT += dt;
    if (arWaveT >= 2.5f) { arWaveT = 0; arWave(); }
  } else {
    arWaveT = 0;
  }
}

// ---- drawing it
static void arEllipse(float cx, float cy, float rx, float ry, float ang, uint16_t c) {
  float xs[16], ys[16];
  float ca = cosf(ang), sa = sinf(ang);
  for (int k = 0; k < 16; k++) {
    float t = k * 6.2832f / 16, ex = cosf(t) * rx, ey = sinf(t) * ry;
    xs[k] = cx + ex * ca - ey * sa;
    ys[k] = cy + ex * sa + ey * ca;
  }
  fillPolygonF(xs, ys, 16, c);
}
// Someone from above: shadow, feet stepping, shoulders across the way he faces, a gun out front, the head
static void arPerson(float x, float y, float face, float sc, float walkT, bool walking, uint16_t body, uint16_t bodyD, uint16_t head, int gun) {
  float ca = cosf(face), sa = sinf(face);
  arEllipse(x + 5, y + 6, 20 * sc, 16 * sc, face, rgb565(20, 22, 26));
  if (walking) {
    float s = sinf(walkT * 10) * 9 * sc;
    for (int f = -1; f <= 1; f += 2) {
      float fx = x - sa * 7 * sc * f + ca * s * f, fy = y + ca * 7 * sc * f + sa * s * f;
      arEllipse(fx, fy, 7 * sc, 4 * sc, face, rgb565(24, 24, 28));
    }
  }
  float gl = gun == 3 ? 40 : gun == 2 ? 36 : gun == 1 ? 26 : 30;     // its gun: out ahead, both hands on it
  if (gun >= 0) {
    float gx0 = x + ca * 6 * sc, gy0 = y + sa * 6 * sc, gx1 = x + ca * gl * sc, gy1 = y + sa * gl * sc;
    uint16_t gc = gun == 2 ? rgb565(180, 190, 206) : gun == 3 ? rgb565(70, 74, 82) : rgb565(26, 26, 30);
    drawLineF(gx0, gy0, gx1, gy1, gc, (gun == 3 ? 8.0f : 5.0f) * sc);
    if (gun == 3) arEllipse(x + ca * 16 * sc - sa * 6 * sc, y + sa * 16 * sc + ca * 6 * sc, 9 * sc, 5 * sc, face, rgb565(200, 40, 30));   // (the FX-5's tank)
    if (gun == 2) fillCircle((int)gx1, (int)gy1, (int)(4 * sc) + 1, rgb565(120, 210, 255));
    for (int h = -1; h <= 1; h += 2)                                    // arms out to it
      drawLineF(x - sa * 12 * sc * h, y + ca * 12 * sc * h, x + ca * 18 * sc, y + sa * 18 * sc, bodyD, 5 * sc);
  }
  arEllipse(x, y, 11 * sc, 19 * sc, face, bodyD);                       // shoulders
  arEllipse(x - ca * 1 * sc, y - sa * 1 * sc, 9 * sc, 16 * sc, face, body);
  fillCircle((int)x, (int)y, (int)(9 * sc) + 1, head);                  // head (or helmet)
}

static void drawObstacle(const Obstacle &o) {
  fillRect(o.x0 + 6, o.y0 + 6, o.x1 + 6, o.y1 + 6, rgb565(30, 32, 38));   // its shadow
  int cx = (o.x0 + o.x1) / 2, cy = (o.y0 + o.y1) / 2;
  switch (o.kind) {
    case OB_DESK:
      fillRect(o.x0, o.y0, o.x1, o.y1, rgb565(130, 96, 64));
      drawRectOutline(o.x0, o.y0, o.x1, o.y1, rgb565(90, 64, 40), 2);
      fillRect(cx - 22, o.y0 + 6, cx + 22, o.y0 + 14, rgb565(30, 32, 38));   // a monitor (from above), a keyboard
      fillRect(cx - 18, o.y0 + 22, cx + 18, o.y0 + 32, rgb565(200, 200, 204));
      fillRect(o.x1 - 30, o.y0 + 10, o.x1 - 12, o.y0 + 34, rgb565(236, 236, 230));
      break;
    case OB_CRATE:
      fillRect(o.x0, o.y0, o.x1, o.y1, rgb565(170, 130, 80));
      drawRectOutline(o.x0, o.y0, o.x1, o.y1, rgb565(110, 80, 46), 3);
      drawLineF(o.x0 + 3, o.y0 + 3, o.x1 - 3, o.y1 - 3, rgb565(120, 88, 50), 3);
      drawLineF(o.x1 - 3, o.y0 + 3, o.x0 + 3, o.y1 - 3, rgb565(120, 88, 50), 3);
      break;
    case OB_CABINET:
      fillRect(o.x0, o.y0, o.x1, o.y1, rgb565(150, 156, 166));
      drawRectOutline(o.x0, o.y0, o.x1, o.y1, rgb565(100, 106, 116), 2);
      for (int y = o.y0 + 28; y < o.y1; y += 28) fillRect(o.x0 + 4, y, o.x1 - 4, y + 1, rgb565(100, 106, 116));
      break;
    case OB_SOFA:
      fillRect(o.x0, o.y0, o.x1, o.y1, rgb565(120, 40, 46));
      fillRect(o.x0, o.y0, o.x1, o.y0 + 14, rgb565(90, 28, 34));
      for (int k = 0; k < 3; k++) {
        int x0 = o.x0 + 6 + k * (o.x1 - o.x0 - 12) / 3;
        fillRect(x0 + 2, o.y0 + 18, x0 + (o.x1 - o.x0 - 12) / 3 - 2, o.y1 - 4, rgb565(150, 54, 60));
      }
      break;
    case OB_PILLAR:
      fillRect(o.x0, o.y0, o.x1, o.y1, rgb565(150, 150, 146));
      drawRectOutline(o.x0, o.y0, o.x1, o.y1, rgb565(110, 110, 106), 3);
      fillCircle(cx, cy, 8, rgb565(120, 120, 116));
      break;
    default:   // a table, chairs at it
      fillRect(o.x0, o.y0, o.x1, o.y1, rgb565(200, 204, 210));
      drawRectOutline(o.x0, o.y0, o.x1, o.y1, rgb565(140, 146, 156), 2);
      for (int k = 0; k < 2; k++) fillRect(o.x0 + 14 + k * 50, o.y1 + 4, o.x0 + 38 + k * 50, o.y1 + 16, rgb565(60, 64, 72));
      break;
  }
}

static void drawArena() {
  // the floor: tiles
  for (int y = HUD_H; y < SCREEN_H; y += 80)
    for (int x = 0; x < SCREEN_W; x += 80) {
      bool alt = ((x / 80) + (y / 80)) & 1;
      fillRect(x, y, x + 79, y + 79, alt ? rgb565(78, 82, 92) : rgb565(72, 76, 86));
      fillRect(x, y, x + 79, y, rgb565(60, 64, 72));
      fillRect(x, y, x, y + 79, rgb565(60, 64, 72));
    }
  // the walls (gaps for the doors)
  uint16_t wc = rgb565(40, 44, 52), wl = rgb565(96, 102, 116);
  fillRect(0, HUD_H, SCREEN_W - 1, AR_Y0 - 1, wc);
  fillRect(0, AR_Y1 + 1, SCREEN_W - 1, SCREEN_H - 1, wc);
  fillRect(0, HUD_H, AR_X0 - 1, SCREEN_H - 1, wc);
  fillRect(AR_X1 + 1, HUD_H, SCREEN_W - 1, SCREEN_H - 1, wc);
  fillRect(AR_X0, AR_Y0 - 3, AR_X1, AR_Y0 - 1, wl);
  fillRect(AR_X0 - 3, AR_Y0, AR_X0 - 1, AR_Y1, wl);
  fillRect(AR_X1 + 1, AR_Y0, AR_X1 + 3, AR_Y1, wl);
  fillRect(AR_X0, AR_Y1 + 1, AR_X1, AR_Y1 + 3, wl);
  // the doors: in at the bottom left, out at the top right, the soldiers' at the top
  fillRect(0, AR_DOOR_L0, AR_X0 + 3, AR_DOOR_L1, rgb565(72, 76, 86));
  drawVectorText("IN", AR_X0 + 34, (AR_DOOR_L0 + AR_DOOR_L1) / 2.0f, 1.6f, rgb565(120, 220, 140), true);
  bool sealed = rooms[curRoom].noRightDoor;
  if (!sealed) {
    fillRect(AR_X1 - 3, AR_DOOR_R0, SCREEN_W - 1, AR_DOOR_R1, rgb565(72, 76, 86));
    float p = 0.5f + 0.5f * sinf(clockT * 4);
    drawVectorText("EXIT", AR_X1 - 44, (AR_DOOR_R0 + AR_DOOR_R1) / 2.0f, 1.8f, rgb565((int)(140 + 100 * p), 255, (int)(140 + 100 * p)), true);
    int ax[3] = { AR_X1 - 10, AR_X1 + 14, AR_X1 - 10 }, ay[3] = { (AR_DOOR_R0 + AR_DOOR_R1) / 2 - 14, (AR_DOOR_R0 + AR_DOOR_R1) / 2, (AR_DOOR_R0 + AR_DOOR_R1) / 2 + 14 };
    fillPolygon(ax, ay, 3, rgb565(120, 255, 140));
  } else {
    fillRect(AR_X1 - 3, AR_DOOR_R0, AR_X1 + 12, AR_DOOR_R1, rgb565(120, 40, 36));
    drawVectorText("SEALED", AR_X1 - 50, (AR_DOOR_R0 + AR_DOOR_R1) / 2.0f, 1.5f, rgb565(255, 120, 100), true);
  }
  fillRect(AR_SPAWN_X0, HUD_H, AR_SPAWN_X1, AR_Y0 + 2, rgb565(30, 30, 34));
  fillRect(AR_SPAWN_X0 - 8, HUD_H, AR_SPAWN_X0 - 1, AR_Y0, rgb565(240, 196, 40));
  fillRect(AR_SPAWN_X1 + 1, HUD_H, AR_SPAWN_X1 + 8, AR_Y0, rgb565(240, 196, 40));
  fillCircle(AR_SPAWN_X1 + 24, HUD_H + 14, 6, (int)(clockT * 3) & 1 ? rgb565(255, 50, 40) : rgb565(110, 20, 20));
  fillRect(AR_SPAWN2_X0, AR_Y1 - 2, AR_SPAWN2_X1, SCREEN_H - 1, rgb565(30, 30, 34));   // the bottom one
  fillRect(AR_SPAWN2_X0 - 8, AR_Y1, AR_SPAWN2_X0 - 1, SCREEN_H - 1, rgb565(240, 196, 40));
  fillRect(AR_SPAWN2_X1 + 1, AR_Y1, AR_SPAWN2_X1 + 8, SCREEN_H - 1, rgb565(240, 196, 40));
  fillCircle(AR_SPAWN2_X1 + 24, SCREEN_H - 12, 6, (int)(clockT * 3 + 1) & 1 ? rgb565(255, 50, 40) : rgb565(110, 20, 20));
  // things in the room
  for (int i = 0; i < AR_OBS; i++) {
    drawObstacle(arObs[i]);
    const Obstacle &o = arObs[i];
    int cx = (o.x0 + o.x1) / 2;
    if (arSearched[curRoom][i]) {                                    // searched: a tick
      drawLineF(cx - 8.0f, o.y0 - 10.0f, cx - 2.0f, o.y0 - 4.0f, rgb565(90, 230, 110), 3);
      drawLineF(cx - 2.0f, o.y0 - 4.0f, cx + 10.0f, o.y0 - 18.0f, rgb565(90, 230, 110), 3);
    } else if (arProg[i] > 0) {                                     // part searched: its bar
      fillRect(cx - 30, o.y0 - 16, cx + 30, o.y0 - 10, rgb565(30, 30, 34));
      fillRect(cx - 30, o.y0 - 16, cx - 30 + (int)(60 * arProg[i] / SEARCH_SECONDS), o.y0 - 10, rgb565(90, 230, 110));
    }
  }
  // the dead (shot: where they fell; burnt: burning, charred, ash, gone)
  for (int i = 0; i < AR_GMAX; i++) {
    const AGuard &g = ag[i];
    if (!g.on || !g.dead) continue;
    if (!g.burn) {
      if (g.dieT > 5) continue;                                     // (shrunk to nothing: dieT 9)
      float ds = g.scale * AR_SC;
      fillCircle((int)g.x, (int)g.y + 4, (int)(18 * ds), rgb565(110, 10, 14));
      arEllipse(g.x, g.y, 24 * ds, 11 * ds, g.face + 1.2f, rgb565(70, 86, 52));
      fillCircle((int)(g.x + cosf(g.face + 1.2f) * 18 * ds), (int)(g.y + sinf(g.face + 1.2f) * 18 * ds), (int)(8 * ds), rgb565(60, 72, 44));
      continue;
    }
    float t = g.dieT;
    if (t < 1.8f) {
      arPerson(g.x, g.y, g.face, g.scale * AR_SC, 0, false, rgb565(34, 30, 28), rgb565(20, 18, 16), rgb565(26, 22, 20), -1);
      for (int j = 0; j < 6; j++) {
        float a = j * 1.05f + clockT * 3, r = 12 * g.scale;
        glowDisc(g.x + cosf(a) * r, g.y + sinf(a) * r, (14 + 6 * sinf(clockT * 15 + j)) * g.scale, 255, 140, 40, t < 1.0f ? 0.6f : 0.35f);
      }
    } else if (t < 3.5f) {
      float k = (t - 1.8f) / 1.7f;
      for (int j = 0; j < 5; j++) fillCircle((int)(g.x + (j - 2) * 6), (int)(g.y + ((j * 7) % 5) - 2), (int)((10 - 7 * k) * g.scale) + 1, rgb565(118, 114, 108));
      glowDisc(g.x, g.y - 20 * k, 24, 120, 116, 110, 0.3f * (1 - k));
    }
  }
  // the soldiers
  for (int i = 0; i < AR_GMAX; i++) {
    const AGuard &g = ag[i];
    if (!g.on || g.dead) continue;
    if (g.fstate == 1)                                              // (aiming: a red sight line)
      drawLineF(g.x + cosf(g.face) * 34 * g.scale, g.y + sinf(g.face) * 34 * g.scale, g.x + cosf(g.face) * 160, g.y + sinf(g.face) * 160, rgb565(255, 40, 40), 1.0f);
    arPerson(g.x, g.y, g.face, g.scale * AR_SC, g.t, g.walking, rgb565(90, 110, 70), rgb565(64, 80, 50), rgb565(70, 86, 50), 0);
    fillCircle((int)g.x, (int)g.y, (int)(5 * g.scale * AR_SC) + 1, rgb565(88, 104, 64));   // (his helmet's top)
    if (g.fstate == 2 && g.t < 0.08f) glowDisc(g.x + cosf(g.face) * 40, g.y + sinf(g.face) * 40, 18, 255, 220, 120, 0.9f);
  }
  // him
  if (healthPct() <= 40 && hpBeat > 0) glowDisc(apX, apY, 60, 255, 30, 30, (0.35f + 0.4f * (1 - healthPct() / 40.0f)) * hpBeat);
  int gun = hasFlamer ? 3 : hasXGun ? 2 : hasGun ? 1 : -1;
  bool walking = (held & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)) != 0;
  if (s2Disguised) arPerson(apX, apY, apFace, AR_SC, apWalkT, walking, rgb565(60, 100, 160), rgb565(40, 70, 120), rgb565(60, 44, 30), gun);
  else arPerson(apX, apY, apFace, AR_SC, apWalkT, walking, rgb565(44, 46, 56), rgb565(28, 30, 38), rgb565(40, 30, 24), gun);
  if (arFlashT > 0) glowDisc(apX + cosf(apFace) * 40, apY + sinf(apFace) * 40, 20, hasXGun ? 120 : 255, hasXGun ? 200 : 220, hasXGun ? 255 : 120, 0.9f);
  // shots, flames
  for (int k = 0; k < AR_SHOTS; k++) {
    const AShot &s = ash[k];
    if (!s.on) continue;
    float l = sqrtf(s.vx * s.vx + s.vy * s.vy), ux = s.vx / l, uy = s.vy / l;
    if (s.ray) { glowDisc(s.x, s.y, 18, 90, 180, 255, 0.6f); drawLineF(s.x - ux * 22, s.y - uy * 22, s.x, s.y, rgb565(170, 220, 255), 3); }
    else drawLineF(s.x - ux * 16, s.y - uy * 16, s.x, s.y, s.mine ? rgb565(255, 240, 160) : rgb565(255, 150, 90), 3);
  }
  drawFlames();
  drawArHazards();
  if (arSearchAt >= 0 && !arSearched[curRoom][arSearchAt] && arProg[arSearchAt] <= 0)   // (beside something: what Square does)
    drawVectorText("X: SEARCH", apX, apY - 46, 1.8f, rgb565(255, 230, 140), true);
  if (nothingShowT > 0)
    drawVectorText(floatText, nothingX, nothingY - (NOTHING_SHOW - nothingShowT) * 20, 2.5f, floatColour, false);
  // how many are left
  int left = AR_TOTAL - arKilled[curRoom];
  char b[40];
  snprintf(b, sizeof(b), left > 0 ? "SOLDIERS LEFT: %d" : "ROOM CLEAR", left);
  drawVectorText(b, 170, AR_Y0 + 20, 1.8f, left > 0 ? rgb565(255, 140, 120) : rgb565(120, 255, 140), true);
}

// Lift C's foyer guard at the level the car's stopped at: into npcs[] (the knife, the guns, the drawing all
// work on him there); everywhere else his record (fgDead...) is what's drawn
static void syncFoyerGuard() {
  int sh = curShaft - 2;
  bool act = stage == 2 && mode == MODE_SHAFT && (sh == 0 || sh == 1) && !carMoving && carFloor >= 0 && carFloor < fgLevels[sh < 0 || sh > 1 ? 0 : sh];
  int key = act ? sh * 8 + carFloor : -1;
  if (act && fgLoaded != key) {
    int L = carFloor;
    for (int k = 0; k < fgSlots[sh]; k++) {
      Npc &n = npcs[k];
      memset(&n, 0, sizeof(n));
      n.kind = PK_GUARD;
      n.dead = fgDead[sh][L][k];
      n.x = n.dead ? fgDX[sh][L][k] : fgPostX[sh][k];
      n.y = CAR_FLOOR_Y;
      n.a = n.b = n.x;
      n.dir = n.dead ? fgDDir[sh][L][k] : -1;     // (facing the walkway's door, where he'd come in from the last lift's rooms)
      n.dieT = 9;
      n.sight = 2000;
      n.scale = fgScale[sh][L][k];
      n.burn = fgAsh[sh][L][k];
      if (n.burn) n.dieT = 99;
      n.foyer = true;
      n.lvl = (int8_t)L;
      n.fsh = (int8_t)sh;
      n.fslot = (int8_t)k;
      n.fcool = FG_GRACE + k * 0.45f;            // (the second one a little after the first)
    }
    npcCount = fgSlots[sh];
    fgLoaded = key;
    for (int k = 0; k < MY_SHOTS; k++) myShots[k].on = false;
  } else if (!act && fgLoaded >= 0) {
    fgLoaded = -1;
    if (mode == MODE_SHAFT) npcCount = 0;
  }
  if (stage == 2 && mode == MODE_SHAFT && fgLoaded < 0) npcCount = 0;   // (no room's people out here)
  if (!act && mode == MODE_SHAFT)
    for (int k = 0; k < 4; k++) gShots[k].on = false;
}
static void updateGShots(float dt) {     // the foyer guard's bullets: across the walkway (jump them)
  for (int k = 0; k < 4; k++) {
    GShot &g = gShots[k];
    if (!g.on) continue;
    float x0 = g.x;
    g.x += g.dir * FG_SHOT_SPEED * dt;
    if (g.x < -30 || g.x > SCREEN_W + 30) { g.on = false; continue; }
    g.y += (g.ty - g.y) * fminf(1.0f, dt * 6);   // (a big soldier's, in a room: down to his chest)
    bool inRoom = mode == MODE_ROOM && agState != ST_JUMP && fabsf(g.y - (agY - 78)) < 50;   // (jump them)
    if (fminf(x0, g.x) - 12 <= agX && agX <= fmaxf(x0, g.x) + 12 && (onWalkway() || inRoom) && agState != ST_KO && gameOver == END_NONE) {
      g.on = false;
      if (hasVest) { hurt(SHOT_HURT / 2); showMsg("HIT - THE VEST HELD", 1.2f); }
      else { hurt(SHOT_HURT); showMsg("SHOT BY A GUARD!", 1.4f); }
      sfxPlay(SFX_HIT);
      shotFlashT = 0.12f;
    }
  }
}
// Beside the dead foyer guard: 1 the one with the card (not searched yet), 2 any other (nothing on him)
static int bodyGuard = -1;                 // (which of npcs[] that is)
static int atGuardBody() {
  bodyGuard = -1;
  if (!onWalkway() || fgLoaded < 0) return 0;
  for (int k = 0; k < npcCount; k++) {
    const Npc &n = npcs[k];
    if (!n.foyer || !n.dead || fabsf(agX - n.x) > 100) continue;
    bodyGuard = k;
    return (n.fsh == 0 && n.lvl == fgCard && n.fslot == 0 && !fgSearched[fgCard]) ? 1 : 2;
  }
  return 0;
}

// Each frame of play in stage 2
static void updateStage2(float dt) {
  if (s2BumpT > 0) s2BumpT -= dt;
  if (congratsT > 0 && !termOn) congratsT -= dt;
  if (bossDeadT >= 0 && gameOver == END_NONE && (bossDeadT += dt) > 2.5f) endGame(END_WIN);   // THE PROFESSOR down: the end
  if (simState == SIM_WON) simT += dt;
  updateWorker(dt);
  syncFoyerGuard();
  if (fgLoaded >= 0) updateNpcs(dt);
  updateGShots(dt);
  updateFlames(dt);
  if (!arOn()) flameHitNpcs();
  if (mode == MODE_ROOM && curRoom < S2_ROOMS) {
    updateSuspicion(dt);
    updateNpcs(dt);
    if (s2Kind[curRoom] == LK_CLONE || s2Kind[curRoom] == LK_QLAB2) updateGas(dt);
    if (s2Kind[curRoom] == LK_HAZARD) updateHazStorage(dt);
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
      } else if (((pressed & BTN_FIRE) && canFire()) || ((pressed & BTN_USE) && targetInLine())) {
        int f = want ? want : agFacing;  // ...the pistol (R1 / R2; Square with someone in line)
        startS2Act(5);
        agFacing = f;
      } else if (pressed & BTN_JUMP) {
        if (want) agFacing = want;
        startJump();
      } else if ((pressed & BTN_USE) && atGuardBody() == 1) {
        startS2Act(6);                  // ...lift C's foyer guard: the level 3 card on him
        agFacing = npcs[bodyGuard].x < agX ? -1 : 1;
      } else if ((pressed & BTN_USE) && atGuardBody() == 2 && !atFoyerAid()) {   // (a body by the first aid: the first aid comes first)
        const Npc &g = npcs[bodyGuard];
        showMsg(g.fsh == 0 && g.lvl == fgCard ? "NOTHING MORE ON HIM" : "NOTHING ON HIM", 1.6f);
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
      if (((pressed & BTN_FIRE) && canFire()) || ((pressed & BTN_USE) && targetInLine())) {
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
          if (decor[i].kind == DEC_LAB && s2Kind[curRoom] == LK_SIMON && i == 0) {   // the CODE ROOM's terminal: play
            searchT = 0;
            startSimon();
            break;
          }
          if (decor[i].kind == DEC_LAB && s2Kind[curRoom] == LK_DESTRUCT && i == 0) {   // the self destruct console
            searchT = 0;
            useDestructTerm();
            break;
          }
          if (decor[i].kind == DEC_LAB && s2Kind[curRoom] == LK_SERVER && i >= 2 && i <= 5) {   // a server room terminal: log in
            searchT = 0;
            if (curHazard == HZ_CAMDRONE && drLight == 2) {   // (not with the drone firing at him)
              showMsg("UNDER FIRE - CAN'T LOG IN", 2.0f);
              sfxPlay(SFX_DENIED);
              setState(ST_IDLE, 0.1f);
              break;
            }
            useServerNode(i - 2);
            break;
          }
          if (decor[i].kind == DEC_LAB) {      // the lab's things: nothing in them yet
            bool first = !s2Searched[curRoom][i];
            s2Searched[curRoom][i] = true;
            if (first) addScore(SEARCH_SCORE, px, py - 26);
            if (first && s2Kind[curRoom] == LK_ARMORY2 && i == 0) {        // the FX-5, out of its case
              flamerTaken = true;
              bool had = hasGun || hasKnife || hasXGun;
              hasFlamer = true;
              hasGun = hasKnife = hasXGun = false;
              foundCard(-5);
              if (had) showMsg("YOU LEAVE YOUR OLD WEAPON IN THE CASE", 3.0f);
              break;
            }
            if (first && s2Kind[curRoom] == LK_ARMORY && i == 0 && hasFlamer) {   // (he keeps the FX-5)
              showMsg("THE QX-9 - YOU KEEP THE FX-5", 2.5f);
              sfxPlay(SFX_CLUNK);
              break;
            }
            if (first && s2Kind[curRoom] == LK_ARMORY && i == 0) {         // the QX-9, out of its case
              xGunTaken = true;
              bool had = hasGun || hasKnife;
              hasXGun = true;
              hasGun = hasKnife = false;
              foundCard(-4);
              if (had) showMsg("YOU LEAVE YOUR OLD WEAPON IN THE CASE", 3.0f);
              break;
            }
            if (first && s2Kind[curRoom] == LK_KITCHEN && i == s2KnifeItem) {   // the kitchen's knife
              if (hasGun || hasXGun || hasFlamer) {   // (one weapon at a time: he keeps the stronger)
                showMsg(hasFlamer ? "A KNIFE - YOU KEEP THE FX-5" : hasXGun ? "A KNIFE - YOU KEEP THE QX-9" : "A KNIFE - YOU KEEP YOUR PISTOL", 2.5f);
                sfxPlay(SFX_CLUNK);
                break;
              }
              hasKnife = true;
              foundCard(-1);
              break;
            }
            if (first && s2Kind[curRoom] == LK_SECURITY && i == s2GunItem) {    // a pistol in a gun rack
              if (hasXGun || hasFlamer) {
                showMsg(hasFlamer ? "A PISTOL - YOU KEEP THE FX-5" : "A PISTOL - YOU KEEP THE QX-9", 2.5f);
                sfxPlay(SFX_CLUNK);
                break;
              }
              hasGun = true;
              foundCard(-2);
              if (hasKnife) {                  // (one weapon at a time: the knife goes)
                hasKnife = false;
                showMsg("KNIFE DROPPED - ONE WEAPON AT A TIME", 3.0f);
              }
              break;
            }
            if (first && s2Hold[curRoom][i] == S2H_VEST) {                    // the bullet-proof vest: he puts it on
              hasVest = true;
              addScore(VEST_SCORE, px, py - 56);
              foundCard(-3);
              break;
            }
            if (first && s2Hold[curRoom][i] == S2H_PASS) {                    // a QUANTUM PASSWORD
              s2PassFound++;
              sfxPlay(SFX_FOUND);
              sfxPlay(SFX_FANFARE);
              addScore(PASS_SCORE, px, py - 56);
              passShowT = DOC_SHOW;
              docShowT = 0;
              break;
            }
            if (first && s2Hold[curRoom][i] == S2H_DOC) {                     // TOP SECRET DOCUMENTS
              s2DocsFound++;
              docValue = S2_DOC_VALUE;
              sfxPlay(SFX_FOUND);
              sfxPlay(SFX_FANFARE);
              addScore(docValue, px, py - 56);
              docShowT = DOC_SHOW;
              break;
            }
            if (first && s2Kind[curRoom] == LK_DESTRUCT && i == s2DestructCard) {   // the self destruct room: the level 5 card
              foundCard(5);
              break;
            }
            if (first && s2Kind[curRoom] == LK_HAZARD && i == s2HazCard) {      // hazardous storage: the level 4 card
              foundCard(4);
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
        if (stage == 2) { s2Penalty(CAM_PENALTY); timeLeft = 1; }   // (stage 2: the next launch or impact, sooner)
        else timeLeft -= CAM_PENALTY;
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
  if (drWakeT > 0) {                   // just called in: down out of the ceiling to hover, no beam, no gun - then it scans
    drWakeT -= dt;
    float dx = drTX - drX, dy = drTY - drY;
    drX += dx * fminf(1.0f, dt * 1.5f);
    drY += dy * fminf(1.0f, dt * 1.5f);
    drVX = drVY = 0;
    drLight = 0;
    drFireT = 0.45f;
    if (drWakeT <= 0) {
      drScan = true;
      drScanAng = agX > drX ? -90.0f : 90.0f;      // (starting away from him)
      drScanDir = agX > drX ? -1.0f : 1.0f;
      sfxPlay(SFX_BEEP);
    }
    return;
  }
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
  if (curHazard == HZ_DRONE || curHazard == HZ_CAMDRONE) updateDrone(dt);
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
static void drawServerNode();
static void drawDestructTerm();
static void drawTerminal() {
  if (termKind == 1) {
    drawFacilityTerminal();
    return;
  }
  if (termKind == 2) {
    drawServerNode();
    return;
  }
  if (termKind == 3) {
    drawDestructTerm();
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
  if (stage == 2) snprintf(b, sizeof(b), "%d OF %d FOUND", s2DocsFound, S2_DOCS);
  else snprintf(b, sizeof(b), "%d OF %d FOUND", docsFound, DOCUMENTS);
  drawVectorText(b, SCREEN_W / 2, y1 - 26, 2.4f, rgb565(200, 206, 220), false);
}

// QUANTUM PASSWORD FOUND: the slip of paper with it on, how many found
static void drawPassPanel() {
  int x0 = 410, y0 = 120, x1 = 870, y1 = 340;
  fillRect(x0, y0, x1, y1, rgb565(10, 12, 20));
  drawRectOutline(x0, y0, x1, y1, rgb565(170, 140, 255), 3);
  drawVectorText("QUANTUM PASSWORD FOUND", SCREEN_W / 2, y0 + 30, 3.0f, rgb565(190, 170, 255), true);
  int k = s2PassFound > 0 ? s2PassFound - 1 : 0;
  fillRect(SCREEN_W / 2 - 150, y0 + 60, SCREEN_W / 2 + 150, y0 + 130, rgb565(240, 236, 220));   // the slip
  fillRect(SCREEN_W / 2 - 150, y0 + 60, SCREEN_W / 2 + 150, y0 + 66, rgb565(200, 60, 60));
  drawVectorText("ACCESS CODE", SCREEN_W / 2, y0 + 80, 1.4f, rgb565(90, 90, 100), false);
  drawVectorText(s2PassCode[k], SCREEN_W / 2, y0 + 108, 3.6f, rgb565(20, 20, 40), true);
  char b[48];
  snprintf(b, sizeof(b), "+%d POINTS", PASS_SCORE);
  drawVectorText(b, SCREEN_W / 2, y1 - 62, 3.0f, rgb565(100, 240, 140), true);
  snprintf(b, sizeof(b), "%d OF %d FOUND", s2PassFound, S2_PASSWORDS);
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
  if (bodyT >= 0) {                      // stage 2: the maintenance man will be found - first, and bigger; the nuclear countdown beside it, smaller
    int bs = (int)ceilf(bodyT);
    snprintf(b, sizeof(b), "BODY FOUND IN %d:%02d", bs / 60, bs % 60);
    bool fl = bs <= 10 && fmodf(clockT, 0.5f) < 0.25f;
    bool impact = false;
    float left = s2Countdown(&impact);
    char n2[32];
    if (left >= 0) {
      int ns = (int)ceilf(left);
      snprintf(n2, sizeof(n2), "%s %d:%02d", impact ? "IMPACT" : "LAUNCH", ns / 60, ns % 60);
    } else {
      snprintf(n2, sizeof(n2), "NO MISSILES LEFT");
    }
    float w1 = vectorTextWidth(b, 2.8f), w2 = vectorTextWidth(n2, 1.9f), x = SCREEN_W / 2 - (w1 + 40 + w2) / 2;
    drawVectorText(b, x + w1 / 2, HUD_H / 2, 2.8f, fl ? rgb565(120, 30, 24) : rgb565(255, 70, 50), true);
    fillRect((int)(x + w1 + 19), HUD_H / 2 - 10, (int)(x + w1 + 20), HUD_H / 2 + 10, rgb565(90, 100, 120));
    drawVectorText(n2, x + w1 + 40 + w2 / 2, HUD_H / 2, 1.9f, left < 0 ? rgb565(100, 240, 140) : left <= 60 ? rgb565(255, 90, 70) : rgb565(255, 190, 70), false);
  } else {
  bool impact = false;
  float left = stage == 2 ? s2Countdown(&impact) : timeLeft;
  if (stage == 2 && left < 0) {
    drawVectorText("NO MISSILES LEFT", SCREEN_W / 2, HUD_H / 2, 2.6f, rgb565(100, 240, 140), true);
  } else {
  int secs = (int)ceilf(left > 0 ? left : 0);
  snprintf(b, sizeof(b), "%d:%02d", secs / 60, secs % 60);
  bool lastMin = secs <= 60, flash = secs <= 10 && fmodf(clockT, 0.5f) < 0.25f;
  uint16_t tc = flash ? rgb565(120, 30, 24) : (lastMin ? rgb565(255, 70, 50) : rgb565(255, 190, 70));
  if (stage == 2) {                          // NUCLEAR LAUNCH 4:32 / NUCLEAR IMPACT 2:05
    const char *lab = impact ? "NUCLEAR IMPACT" : "NUCLEAR LAUNCH";
    float w1 = vectorTextWidth(lab, 2.2f), w2 = vectorTextWidth(b, 3.2f), x = SCREEN_W / 2 - (w1 + 16 + w2) / 2;
    drawVectorText(lab, x + w1 / 2, HUD_H / 2, 2.2f, impact ? rgb565(255, 90, 70) : rgb565(255, 190, 70), true);
    drawVectorText(b, x + w1 + 16 + w2 / 2, HUD_H / 2, 3.2f, tc, true);
  } else {
    drawVectorText(b, SCREEN_W / 2, HUD_H / 2, 3.2f, tc, true);
  }
  }
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


static void drawDestructTerm() {
  drawTermCase("NUCLEAR SELF DESTRUCT");
  uint16_t g = rgb565(100, 255, 140), gd = rgb565(40, 140, 70), dark = rgb565(4, 18, 9), y = rgb565(255, 220, 80), red = rgb565(255, 90, 70);
  bool cur = ((int)(termT * 2.5f) & 1);
  float lx = TX0 + 30, mid = (TX0 + TX1) / 2.0f;
  char b[80];
  drawTextLeft("MISSILE SELF DESTRUCT LINK", lx, TY0 + 26, 2.2f, gd, false);
  if (s2NukeCode) {
    snprintf(b, sizeof(b), "CODE: %s", nukeCodeText);
    drawTextLeft(b, TX1 - 30 - vectorTextWidth(b, 2.0f), TY0 + 26, 2.0f, y, true);
  }
  fillRect(TX0 + 18, TY0 + 46, TX1 - 18, TY0 + 47, gd);
  int list[NUKE_SITES], n = flightList(list);
  switch (termPage) {
    case DS_MENU:
      drawTextLeft("DESTROY A MISSILE IN FLIGHT:", lx, TY0 + 84, 2.4f, g, false);
      for (int i = 0; i <= n; i++) {
        int yy = TY0 + 132 + i * 44;
        if (i < n) snprintf(b, sizeof(b), "%d  DESTROY: %s", i + 1, siteName[list[i]]);
        else snprintf(b, sizeof(b), "%d  LOG OFF", i + 1);
        if (i == termSel) {
          fillRect(TX0 + 20, yy - 18, TX1 - 20, yy + 18, rgb565(70, 220, 120));
          drawTextLeft(b, lx + 12, yy, 2.8f, dark, true);
        } else {
          drawTextLeft(b, lx + 12, yy, 2.8f, i < n ? y : g, true);
        }
        if (i < n) {                           // how near it is to landing
          int s2 = list[i];
          float k = landTotal[s2] > 0 ? clampv(1.0f - landLeft[s2] / landTotal[s2], 0.0f, 1.0f) : 0;
          int bx0 = TX1 - 250, bx1 = TX1 - 40;
          fillRect(bx0, yy - 8, bx1, yy + 8, rgb565(20, 40, 26));
          fillRect(bx0, yy - 8, bx0 + (int)((bx1 - bx0) * k), yy + 8, k > 0.8f ? red : rgb565(250, 140, 30));
          drawRectOutline(bx0, yy - 8, bx1, yy + 8, gd, 1);
        }
      }
      drawTextLeft("THE CODE WORKS ONCE", lx, TY1 - 52, 1.8f, red, false);
      drawTextLeft("UP / DOWN TO CHOOSE    X TO SELECT", lx, TY1 - 22, 1.8f, gd, false);
      break;
    case DS_SENDING: {
      float k = fminf(1.0f, termT / DS_SEND_SECONDS);
      snprintf(b, sizeof(b), "SENDING SELF DESTRUCT TO %s MISSILE...", siteName[dsSite]);
      drawTextLeft(b, lx, TY0 + 100, 2.4f, g, true);
      static const char *const steps[4] = { "SATELLITE UPLINK", "MISSILE HANDSHAKE", "CODE ACCEPTED", "WARHEAD DESTRUCT" };
      for (int f = 0; f < 4; f++)
        if (k > f * 0.25f) {
          snprintf(b, sizeof(b), "%-24s %s", steps[f], k >= (f + 1) * 0.25f ? "OK" : "...");
          drawTextLeft(b, lx + 20, TY0 + 150 + f * 34, 2.4f, g, false);
        }
      drawRectOutline((int)lx, TY0 + 320, (int)lx + 604, TY0 + 344, g, 1);
      fillRect((int)lx + 2, TY0 + 322, (int)lx + 2 + (int)(600 * k), TY0 + 342, g);
      break;
    }
    case DS_DONE:
      drawVectorText("MISSILE DESTROYED", mid, TY0 + 100, 4.4f, g, true);
      drawVectorText(siteName[dsSite], mid, TY0 + 156, 3.2f, y, true);
      if (congratsDone) {
        drawVectorText("CONGRATULATIONS", mid, TY0 + 220, 3.4f, y, true);
        drawVectorText(congratsAll ? "YOU HAVE STOPPED ALL THE NUCLEAR MISSILES" : "YOU HAVE DONE THE BEST YOU COULD", mid, TY0 + 262, 2.3f, g, true);
      } else {
        drawVectorText("YOUR CODE HAS BEEN USED", mid, TY0 + 226, 2.6f, gd, false);
        drawVectorText(n > 0 ? "GET ANOTHER CODE FROM THE ROOM ABOVE" : "NO MORE MISSILES IN THE AIR", mid, TY0 + 266, 2.2f, g, false);
      }
      if (cur) drawTextLeft("PRESS X TO LOG OFF", lx, TY1 - 30, 2.2f, gd, false);
      break;
    case DS_LATE:
      drawVectorText("TOO LATE", mid, TY0 + 120, 4.4f, red, true);
      snprintf(b, sizeof(b), "THE %s MISSILE HAS LANDED", siteName[dsSite]);
      drawVectorText(b, mid, TY0 + 190, 2.6f, red, true);
      drawVectorText("YOUR CODE WAS NOT USED", mid, TY0 + 250, 2.4f, g, false);
      if (cur) drawTextLeft("PRESS X TO RETURN", lx, TY1 - 30, 2.2f, gd, false);
      break;
    case DS_NOCODE:
      drawVectorText("ACCESS DENIED", mid, TY0 + 120, 4.4f, red, true);
      drawVectorText("A NUKE SELF DESTRUCT CODE IS NEEDED", mid, TY0 + 190, 2.6f, y, true);
      drawVectorText("GET ONE FROM THE CODE ROOM ABOVE", mid, TY0 + 240, 2.4f, g, false);
      if (cur) drawTextLeft("PRESS X TO LOG OFF", lx, TY1 - 30, 2.2f, gd, false);
      break;
    default:   // DS_NONE
      drawVectorText("NO MISSILES IN FLIGHT", mid, TY0 + 120, 4.0f, y, true);
      drawVectorText("NOTHING TO DESTROY - YOUR CODE WAS NOT USED", mid, TY0 + 190, 2.2f, g, false);
      if (cur) drawTextLeft("PRESS X TO LOG OFF", lx, TY1 - 30, 2.2f, gd, false);
      break;
  }
}

// CONGRATULATIONS: the last missile stopped (all of them, or the best he could)
static void drawCongrats() {
  float k = fminf(1.0f, (8.0f - congratsT) / 0.4f) * fminf(1.0f, congratsT / 0.5f);
  blendRect(0, 220, SCREEN_W - 1, 400, 0, 0, 0, 0.75f * k);
  if (k < 0.5f) return;
  drawVectorTextW("CONGRATULATIONS", SCREEN_W / 2, 270, 6.0f, rgb565(20, 90, 40), 7.0f);
  drawVectorTextW("CONGRATULATIONS", SCREEN_W / 2, 270, 6.0f, rgb565(100, 255, 140), 2.6f);
  drawVectorText(congratsAll ? "YOU HAVE STOPPED ALL THE NUCLEAR MISSILES" : "YOU HAVE DONE THE BEST YOU COULD", SCREEN_W / 2, 330, 3.0f, rgb565(240, 244, 250), true);
  drawVectorText("NOW FIND DR. VOSS - LIFT F", SCREEN_W / 2, 372, 2.2f, rgb565(255, 220, 120), false);
}

static void drawServerNode() {
  char b[64];
  snprintf(b, sizeof(b), "QUANTUM NETWORK NODE %d", curNode + 1);
  drawTermCase(b);
  uint16_t g = rgb565(100, 255, 140), gd = rgb565(40, 140, 70), dark = rgb565(4, 18, 9), y = rgb565(255, 220, 80), red = rgb565(255, 90, 70);
  bool cur = ((int)(termT * 2.5f) & 1);
  float lx = TX0 + 30, mid = (TX0 + TX1) / 2.0f;
  snprintf(b, sizeof(b), "SERVER ROOM   NODE %d", curNode + 1);
  drawTextLeft(b, lx, TY0 + 26, 2.2f, gd, false);
  drawTextLeft("HAZARDS DISABLED WHILE LOGGED IN", TX1 - 30 - vectorTextWidth("HAZARDS DISABLED WHILE LOGGED IN", 2.0f), TY0 + 26, 2.0f, y, true);
  fillRect(TX0 + 18, TY0 + 46, TX1 - 18, TY0 + 47, gd);
  int list[NUKE_SITES], n = preventList(list);
  switch (termPage) {
    case SN_LOGIN: {
      snprintf(b, sizeof(b), "NODE:        %d", curNode + 1);
      drawTextLeft(b, lx, TY0 + 84, 2.8f, g, true);
      for (int p = 0; p < PASS_PER_LOGIN; p++) {                  // the four passwords, typed in one after another
        const char *pass = s2PassCode[clampv(passUsed + p, 0, S2_PASSWORDS - 1)];
        float t0 = 0.3f + p * 0.45f;
        if (termT <= t0) break;
        int n2 = clampv((int)((termT - t0) * 20), 0, (int)strlen(pass));
        snprintf(b, sizeof(b), "PASSWORD %d:  %.*s", p + 1, n2, pass);
        drawTextLeft(b, lx, TY0 + 124 + p * 34, 2.6f, g, true);
      }
      if (termT > 2.2f) drawTextLeft("4 ONE-TIME PASSWORDS ACCEPTED", lx, TY0 + 280, 3.2f, y, true);
      break;
    }
    case SN_MENU:
      drawTextLeft("LAUNCH ABORT - SELECT A HACKED SITE:", lx, TY0 + 84, 2.4f, g, false);
      for (int i = 0; i <= n; i++) {
        int yy = TY0 + 132 + i * 44;
        if (i < n) snprintf(b, sizeof(b), "%d  PREVENT LAUNCH: %s", i + 1, siteName[list[i]]);
        else snprintf(b, sizeof(b), "%d  LOG OFF", i + 1);
        if (i == termSel) {
          fillRect(TX0 + 20, yy - 18, TX1 - 20, yy + 18, rgb565(70, 220, 120));
          drawTextLeft(b, lx + 12, yy, 2.8f, dark, true);
        } else {
          drawTextLeft(b, lx + 12, yy, 2.8f, i < n ? y : g, true);
        }
      }
      drawTextLeft("THIS NODE AND ITS 4 PASSWORDS WORK ONCE ONLY", lx, TY1 - 52, 1.8f, red, false);
      drawTextLeft("UP / DOWN TO CHOOSE    X TO SELECT", lx, TY1 - 22, 1.8f, gd, false);
      break;
    case SN_SENDING: {
      float k = fminf(1.0f, termT / SN_SEND_SECONDS);
      snprintf(b, sizeof(b), "SENDING ABORT CODE TO %s...", siteName[snSite]);
      drawTextLeft(b, lx, TY0 + 100, 2.6f, g, true);
      static const char *const steps[4] = { "QUANTUM LINK", "SITE HANDSHAKE", "OVERRIDE LAUNCH CONTROL", "SAFE THE MISSILE" };
      for (int f = 0; f < 4; f++)
        if (k > f * 0.25f) {
          snprintf(b, sizeof(b), "%-24s %s", steps[f], k >= (f + 1) * 0.25f ? "OK" : "...");
          drawTextLeft(b, lx + 20, TY0 + 150 + f * 34, 2.4f, g, false);
        }
      drawRectOutline((int)lx, TY0 + 320, (int)lx + 604, TY0 + 344, g, 1);
      fillRect((int)lx + 2, TY0 + 322, (int)lx + 2 + (int)(600 * k), TY0 + 342, g);
      break;
    }
    case SN_DONE:
      drawVectorText("LAUNCH PREVENTED", mid, TY0 + 110, 4.4f, g, true);
      drawVectorText(siteName[snSite], mid, TY0 + 170, 3.4f, y, true);
      drawVectorText("THIS NODE IS NOW LOCKED", mid, TY0 + 240, 2.6f, gd, false);
      {
        int nodesLeft = 0;
        for (int k = 0; k < 4; k++) nodesLeft += !nodeUsed[k];
        snprintf(b, sizeof(b), "PASSWORDS LEFT: %d   NODES LEFT: %d", s2PassFound - passUsed, nodesLeft);
      }
      drawVectorText(b, mid, TY0 + 290, 2.4f, g, false);
      if (cur) drawTextLeft("PRESS X TO LOG OFF", lx, TY1 - 30, 2.2f, gd, false);
      break;
    case SN_LATE:
      drawVectorText("TOO LATE", mid, TY0 + 120, 4.4f, red, true);
      snprintf(b, sizeof(b), "%s HAS LAUNCHED", siteName[snSite]);
      drawVectorText(b, mid, TY0 + 190, 3.0f, red, true);
      drawVectorText("YOUR PASSWORD WAS NOT USED", mid, TY0 + 250, 2.4f, g, false);
      if (cur) drawTextLeft("PRESS X TO RETURN", lx, TY1 - 30, 2.2f, gd, false);
      break;
    case SN_USED:
      drawVectorText("NODE LOCKED", mid, TY0 + 120, 4.4f, red, true);
      drawVectorText("THIS NODE'S ONE-TIME ACCESS HAS BEEN USED", mid, TY0 + 190, 2.4f, g, false);
      drawVectorText("USE ANOTHER NODE", mid, TY0 + 236, 2.6f, y, true);
      if (cur) drawTextLeft("PRESS X TO LOG OFF", lx, TY1 - 30, 2.2f, gd, false);
      break;
    case SN_NOPASS:
      drawVectorText("ACCESS DENIED", mid, TY0 + 120, 4.4f, red, true);
      drawVectorText("4 QUANTUM PASSWORDS ARE NEEDED", mid, TY0 + 190, 2.8f, y, true);
      snprintf(b, sizeof(b), "UNUSED PASSWORDS: %d OF 4   (FOUND %d, USED %d)", s2PassFound - passUsed, s2PassFound, passUsed);
      drawVectorText(b, mid, TY0 + 240, 2.4f, g, false);
      if (cur) drawTextLeft("PRESS X TO LOG OFF", lx, TY1 - 30, 2.2f, gd, false);
      break;
    default:   // SN_NOSITES
      drawVectorText("NO LAUNCHES TO STOP", mid, TY0 + 120, 4.0f, y, true);
      drawVectorText(sitesHacked == 0 ? "NO SITES HAVE BEEN HACKED YET" : "EVERY HACKED SITE HAS LAUNCHED OR BEEN STOPPED", mid, TY0 + 190, 2.2f, g, false);
      drawVectorText("YOUR PASSWORD WAS NOT USED", mid, TY0 + 240, 2.4f, g, false);
      if (cur) drawTextLeft("PRESS X TO LOG OFF", lx, TY1 - 30, 2.2f, gd, false);
      break;
  }
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
    line("ROOMS", b, PANEL_Y + 32, rgb565(240, 240, 245));
    if (s2AllAccess) snprintf(b, sizeof(b), "ALL");
    else if (s2Card > 0) snprintf(b, sizeof(b), "LEVEL %d", s2Card);
    else snprintf(b, sizeof(b), "NONE");
    line("ACCESS CARD", b, PANEL_Y + 58, rgb565(90, 220, 255));
    line("DISGUISE", s2Disguised ? "OVERALLS" : "NONE", PANEL_Y + 84, rgb565(255, 220, 80));
    snprintf(b, sizeof(b), "%d", aid);
    line("FIRST AID LEFT", b, PANEL_Y + 110, rgb565(120, 230, 140));
    {
      const char *w = hasFlamer ? "FX-5" : hasXGun ? "QX-9" : hasGun ? "PISTOL" : hasKnife ? "KNIFE" : NULL;
      snprintf(b, sizeof(b), "%s%s", w ? w : hasVest ? "" : "NONE", hasVest ? (w ? " VEST" : "VEST") : "");
    }
    line("GEAR", b, PANEL_Y + 136, rgb565(240, 240, 245));
    snprintf(b, sizeof(b), "%d/%d", s2PassFound, S2_PASSWORDS);
    line("PASSWORDS", b, PANEL_Y + 162, rgb565(190, 170, 255));
    snprintf(b, sizeof(b), "V%s  FPS %d", GAME_VERSION, (int)lroundf(fpsShown));
    drawVectorText(b, (lx + SCREEN_W - 44) / 2, PANEL_Y + 186, 1.5f, rgb565(150, 160, 186), false);
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
// The end of the game's background: the cities hit (the landing, once, then the city burning, and the list of
// strikes), or with none landed and Dr. Voss dead, green hills, flowers, forest and birds
enum GoBg : uint8_t { GB_NONE, GB_NUKE, GB_PEACE };
static GoBg goBg = GB_NONE;
static int goPage = 0;                       // (GB_NUKE: 0 the strikes, 1 the bonus and score)
static float goPageT0 = 0;
static bool goSceneOn = false;
static void drawScaledScene(int cropY, uint32_t bright);
#define GO_CROP_Y 120                        // (the burning city: the cloud in, the ground's edge out)
static void goStartScene() {                 // the landing: the city with the most dead
  int best = -1;
  for (int i = 0; i < NUKE_SITES; i++) {
    if (prevented[i] || nukeDestroyed[i] || (lockdown && i >= sitesHacked)) continue;
    if (best < 0 || landDeaths[i] > landDeaths[best]) best = i;
  }
  launchOn = false;
  launchQueued = 0;
  if (best < 0) return;
  launchOn = true;
  lsKind = 1;
  startLandScene(best);
  goSceneOn = true;
}
static void drawPeaceScene(float t) {        // the world, safe
  for (int y = 0; y < 440; y += 6) {         // the sky: deep blue to pale
    float k = y / 440.0f;
    fillRect(0, y, SCREEN_W - 1, y + 5, rgb565((int)(70 + 110 * k), (int)(140 + 80 * k), (int)(230 + 20 * k)));
  }
  glowDisc(1040, 120, 170, 255, 240, 170, 0.35f);   // the sun
  fillCircle(1040, 120, 44, rgb565(255, 246, 200));
  for (int c = 0; c < 5; c++) {              // clouds drifting
    float x = fmodf(c * 330 + t * 14, 1640.0f) - 180, y = 70 + (c * 53) % 130;
    uint16_t w = rgb565(250, 250, 255);
    fillCircle((int)x, (int)y, 26, w);
    fillCircle((int)x + 30, (int)y - 12, 32, w);
    fillCircle((int)x + 64, (int)y, 24, w);
    fillRect((int)x - 4, (int)y, (int)x + 70, (int)y + 20, w);
  }
  auto hill = [](float base, float a1, float f1, float a2, float f2, float ph, uint16_t c) {
    for (int x = 0; x < SCREEN_W; x += 8) {
      int y = (int)(base - a1 * sinf(x * f1 + ph) - a2 * sinf(x * f2 + ph * 1.7f));
      fillRect(x, y, x + 7, SCREEN_H - 1, c);
    }
  };
  hill(360, 40, 0.004f, 18, 0.011f, 0.5f, rgb565(120, 170, 160));   // far hills, misty
  hill(420, 46, 0.006f, 14, 0.017f, 2.1f, rgb565(80, 150, 90));     // the middle hills
  for (int i = 0; i < 46; i++) {             // the forest along them
    int x = (i * 29 + (i * 7) % 13) % SCREEN_W;
    float hy = 420 - 46 * sinf(x * 0.006f + 2.1f) - 14 * sinf(x * 0.017f + 3.57f);
    int h = 34 + (i * 11) % 26;
    fillRect(x - 2, (int)hy - 8, x + 2, (int)hy + 4, rgb565(80, 56, 36));
    if (i % 3) {                             // pines
      int px[3] = { x - 14, x + 14, x }, py[3] = { (int)hy - 4, (int)hy - 4, (int)hy - h };
      fillPolygon(px, py, 3, i % 2 ? rgb565(30, 100, 50) : rgb565(36, 112, 58));
    } else {                                 // round ones
      fillCircle(x, (int)hy - h / 2, h / 3 + 4, rgb565(50, 130, 60));
      fillCircle(x - 6, (int)hy - h / 2 + 6, h / 4 + 2, rgb565(60, 146, 70));
    }
  }
  hill(560, 26, 0.005f, 10, 0.02f, 4.0f, rgb565(110, 190, 90));     // the meadow
  for (int i = 0; i < 120; i++) {            // flowers, swaying
    int x = (i * 53 + (i * i) % 37) % SCREEN_W;
    float base = 560 - 26 * sinf(x * 0.005f + 4.0f) - 10 * sinf(x * 0.02f + 6.8f);
    int y = (int)(base + 14 + (i * 31) % (SCREEN_H - (int)base - 20));
    float sw = 3 * sinf(t * 1.8f + i);
    drawLineF((float)x, (float)y + 10, x + sw, (float)y, rgb565(60, 140, 50), 1.5f);
    static const uint16_t fc[5] = { rgb565(255, 90, 90), rgb565(255, 220, 70), rgb565(250, 250, 250), rgb565(200, 120, 255), rgb565(255, 150, 200) };
    fillCircle((int)(x + sw), y, 4, fc[i % 5]);
    fillCircle((int)(x + sw), y, 1, rgb565(255, 230, 120));
  }
  for (int i = 0; i < 9; i++) {              // birds, in a loose V, flapping across
    float bx = fmodf(t * 70 + 200, 1700.0f) - 200 - (i / 2) * 34 * 1.0f, by = 170 + (i % 2 ? 1 : -1) * (i / 2) * 18 + 6 * sinf(t + i);
    float flap = sinf(t * 9 + i * 0.7f) * 7;
    uint16_t bc = rgb565(40, 44, 60);
    drawLineF(bx - 12, by - flap, bx, by, bc, 2.0f);
    drawLineF(bx, by, bx + 12, by - flap, bc, 2.0f);
  }
}
// The strikes, listed over the burning city (the first page of a game that ended with missiles landed)
static void goHeader(const char **title, bool *good, const char **reason) {
  *good = false;
  if (gameOver == END_WIN) { *title = "MISSION COMPLETE"; *good = true; *reason = "DR. VOSS IS DEAD"; }
  else if (gameOver == END_TIME) { *title = "MISSION FAILED"; *reason = "POWER FAILURE - THE GENERATORS HAVE SHUT DOWN"; }
  else if (gameOver == END_BODY) { *title = "MISSION FAILED"; *reason = "THE MAINTENANCE MAN'S BODY WAS FOUND"; }
  else if (gameOver == END_NUKES) { *title = "MISSION FAILED"; *reason = "THE NUCLEAR MISSILES HAVE LANDED"; }
  else { *title = "GAME OVER"; *reason = "OUT OF HEALTH"; }
}
static void drawStrikesPage(float t) {
  const char *title, *reason;
  bool good;
  goHeader(&title, &good, &reason);
  float b = t - LD_FALL;                      // (since the detonation)
  if (b < 0.6f) return;
  blendRect(0, 66, SCREEN_W - 1, 196, 0, 0, 0, 0.55f);
  drawVectorTextW(title, SCREEN_W / 2, 112, 7.0f, good ? rgb565(20, 90, 40) : rgb565(120, 20, 14), 8.0f);
  drawVectorTextW(title, SCREEN_W / 2, 112, 7.0f, good ? rgb565(100, 255, 140) : rgb565(255, 70, 50), 3.0f);
  drawVectorText(reason, SCREEN_W / 2, 166, 2.8f, rgb565(240, 240, 245), true);
  if (b < 1.6f) return;
  int list[NUKE_SITES], n = 0;
  for (int i = 0; i < NUKE_SITES; i++)
    if (!prevented[i] && !nukeDestroyed[i] && !(lockdown && i >= sitesHacked)) list[n++] = i;
  int y0 = 230, rowH = 42, y1 = y0 + 40 + n * rowH + 64;
  blendRect(260, y0 - 22, SCREEN_W - 261, y1, 0, 0, 0, 0.62f);
  drawVectorText(n == 1 ? "NUCLEAR STRIKE" : "NUCLEAR STRIKES", SCREEN_W / 2, (float)y0, 2.8f, rgb565(255, 190, 70), true);
  int shown = (int)((b - 1.6f) / 0.6f) + 1;  // (one by one)
  char num[24], line[64];
  for (int k = 0; k < n && k < shown; k++) {
    const City &ct = cities[landCity[list[k]]];
    int y = y0 + 44 + k * rowH;
    snprintf(line, sizeof(line), "%s, %s", ct.name, ct.country);
    drawTextLeft(line, 300, (float)y, 2.6f, rgb565(240, 230, 220), true);
    commas(num, sizeof(num), landDeaths[list[k]]);
    drawTextLeft(num, SCREEN_W - 300 - vectorTextWidth(num, 2.8f), (float)y, 2.8f, rgb565(255, 90, 70), true);
  }
  if (shown > n) {
    int y = y0 + 44 + n * rowH;
    fillRect(300, y - 18, SCREEN_W - 300, y - 17, rgb565(150, 90, 80));
    commas(num, sizeof(num), finalDeaths);
    drawTextLeft("TOTAL DEAD", 300, (float)y + 14, 3.0f, rgb565(255, 190, 70), true);
    drawTextLeft(num, SCREEN_W - 300 - vectorTextWidth(num, 3.6f), (float)y + 14, 3.6f, rgb565(255, 60, 40), true);
    if (fmodf(clockT, 1.0f) < 0.7f) drawVectorText("PRESS X", SCREEN_W / 2, (float)SCREEN_H - 40, 3.0f, rgb565(240, 240, 245), true);
  }
}
static void goChoose(int why) {               // (at the end: which background)
  goBg = finalHits > 0 ? GB_NUKE : GB_PEACE;   // (none landed: the green hills, however it ended)
  goPage = 0;
  goPageT0 = 0;
  goSceneOn = false;
  launchOn = false;                          // (no cut scene left playing beside the game)
  launchQueued = 0;
}
static bool goStrikesDone() {                // (every line shown: X goes on)
  int n = finalHits;
  return lsT - LD_FALL > 1.6f + n * 0.6f + 0.6f;
}

static float gameOverShownT() { return gameOverT - (gameOver == END_TIME ? POWER_FAIL_PANEL : 0.0f); }   // (the power failing first)
static void drawGameOver() {
  float gameOverT = scene == SC_STAGE_CLEAR ? sceneT : gameOverShownT();
  if (gameOverT < 0) return;
  float k = gameOverT / 1.0f;
  if (k > 1) k = 1;
  if (scene != SC_STAGE_CLEAR && goBg != GB_NONE) {   // the end of the game's background, over everything
    gfxClipAll();
    if (goBg == GB_NUKE) {
      if (goSceneOn) drawScaledScene(GO_CROP_Y, 170);
      else pxRect(0, 0, FB_W - 1, FB_H - 1, 0);
      if (k < 1) blendRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 0, 0, 0, 1 - k);
      if (goPage == 0) { drawStrikesPage(lsT); return; }
      gameOverT -= goPageT0;
      if (gameOverT < 0) return;
      k = fminf(1.0f, gameOverT);
      blendRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 0, 0, 0, 0.5f * k);
    } else {
      drawPeaceScene(gameOverT);
      if (k < 1) blendRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 0, 0, 0, 1 - k);
      blendRect(240, 70, SCREEN_W - 241, SCREEN_H - 70, 0, 0, 0, 0.5f * k);
    }
  } else {
    blendRect(0, HUD_H, SCREEN_W - 1, SCREEN_H - 1, 0, 0, 0, 0.65f * k);
  }
  if (gameOverT < 0.4f) return;
  if (scene == SC_STAGE_CLEAR) {
    drawVectorTextW("STAGE 1 COMPLETE", SCREEN_W / 2, 120, 7.0f, rgb565(20, 90, 40), 8.0f);
    drawVectorTextW("STAGE 1 COMPLETE", SCREEN_W / 2, 120, 7.0f, rgb565(100, 255, 140), 3.0f);
    drawVectorText("YOU REACHED THE QUANTUMLOOP", SCREEN_W / 2, 186, 3.2f, rgb565(240, 240, 245), true);
  } else if (gameOver == END_WIN && stage == 2) {   // THE PROFESSOR dead: how the missiles went
    const char *t2 = finalHits ? "MISSION COMPLETE" : "MISSION SUCCESS";
    drawVectorTextW(t2, SCREEN_W / 2, 120, 7.0f, rgb565(20, 90, 40), 8.0f);
    drawVectorTextW(t2, SCREEN_W / 2, 120, 7.0f, rgb565(100, 255, 140), 3.0f);
    drawVectorText("DR. VOSS IS DEAD", SCREEN_W / 2, 172, 3.0f, rgb565(240, 240, 245), true);
    {
      char num[24], line[96];
      commas(num, sizeof(num), finalDeaths);
      if (finalHits) snprintf(line, sizeof(line), "%d NUCLEAR MISSILE%s LANDED   -   %s DEAD", finalHits, finalHits == 1 ? "" : "S", num);
      else snprintf(line, sizeof(line), "NO NUCLEAR MISSILES LANDED - THE WORLD IS SAFE");
      drawVectorText(line, SCREEN_W / 2, 204, 2.4f, finalHits ? rgb565(255, 90, 70) : rgb565(100, 255, 140), true);
    }
  } else if (gameOver == END_WIN) {
    drawVectorTextW("MISSION COMPLETE", SCREEN_W / 2, 120, 7.0f, rgb565(20, 90, 40), 8.0f);
    drawVectorTextW("MISSION COMPLETE", SCREEN_W / 2, 120, 7.0f, rgb565(100, 255, 140), 3.0f);
    drawVectorText("YOU REACHED THE SUB BASEMENT", SCREEN_W / 2, 186, 3.2f, rgb565(240, 240, 245), true);
  } else if (gameOver == END_TIME) {
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(120, 20, 14), 8.0f);
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(255, 70, 50), 3.0f);
    drawVectorText("POWER FAILURE - THE GENERATORS HAVE SHUT DOWN", SCREEN_W / 2, 172, 3.0f, rgb565(240, 240, 245), true);
    {
      char num[24], line[96];
      commas(num, sizeof(num), finalDeaths);
      if (finalHits) snprintf(line, sizeof(line), "%d NUCLEAR MISSILE%s LANDED   -   %s DEAD", finalHits, finalHits == 1 ? "" : "S", num);
      else snprintf(line, sizeof(line), "NO NUCLEAR MISSILES LANDED");
      drawVectorText(line, SCREEN_W / 2, 204, 2.4f, finalHits ? rgb565(255, 90, 70) : rgb565(100, 255, 140), true);
    }
  } else if (gameOver == END_NUKES) {
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(120, 20, 14), 8.0f);
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(255, 70, 50), 3.0f);
    drawVectorText("THE NUCLEAR MISSILES HAVE LANDED", SCREEN_W / 2, 172, 3.0f, rgb565(240, 240, 245), true);
    {
      char num[24], line[96];
      commas(num, sizeof(num), finalDeaths);
      if (finalHits) snprintf(line, sizeof(line), "%d NUCLEAR MISSILE%s LANDED   -   %s DEAD", finalHits, finalHits == 1 ? "" : "S", num);
      else snprintf(line, sizeof(line), "NO NUCLEAR MISSILES LANDED");
      drawVectorText(line, SCREEN_W / 2, 204, 2.4f, finalHits ? rgb565(255, 90, 70) : rgb565(100, 255, 140), true);
    }
  } else if (gameOver == END_BODY) {
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(120, 20, 14), 8.0f);
    drawVectorTextW("MISSION FAILED", SCREEN_W / 2, 120, 7.0f, rgb565(255, 70, 50), 3.0f);
    drawVectorText("THE MAINTENANCE MAN'S BODY WAS FOUND", SCREEN_W / 2, 172, 3.0f, rgb565(240, 240, 245), true);
    {
      char num[24], line[96];
      commas(num, sizeof(num), finalDeaths);
      if (finalHits) snprintf(line, sizeof(line), "%d NUCLEAR MISSILE%s LANDED   -   %s DEAD", finalHits, finalHits == 1 ? "" : "S", num);
      else snprintf(line, sizeof(line), "NO NUCLEAR MISSILES LANDED");
      drawVectorText(line, SCREEN_W / 2, 204, 2.4f, finalHits ? rgb565(255, 90, 70) : rgb565(100, 255, 140), true);
    }
  } else {
    drawVectorTextW("GAME OVER", SCREEN_W / 2, 120, 8.0f, rgb565(120, 20, 14), 8.0f);
    drawVectorTextW("GAME OVER", SCREEN_W / 2, 120, 8.0f, rgb565(255, 70, 50), 3.0f);
    drawVectorText("OUT OF HEALTH", SCREEN_W / 2, 172, 3.2f, rgb565(240, 240, 245), true);
    {
      char num[24], line[96];
      commas(num, sizeof(num), finalDeaths);
      if (finalHits) snprintf(line, sizeof(line), "%d NUCLEAR MISSILE%s LANDED   -   %s DEAD", finalHits, finalHits == 1 ? "" : "S", num);
      else snprintf(line, sizeof(line), "NO NUCLEAR MISSILES LANDED");
      drawVectorText(line, SCREEN_W / 2, 204, 2.4f, finalHits ? rgb565(255, 90, 70) : rgb565(100, 255, 140), true);
    }
  }
  // the end of game bonuses, one line at a time
  char b[80];
  int mins = (int)(bonusPts[0] / BONUS_PER_MINUTE);
  bool s2End = stage == 2 && scene != SC_STAGE_CLEAR;   // (stage 2: the missiles line, no electrocution line)
  const char *what[BONUS_LINES] = { "", "NO ELECTROCUTION", "NO DRONE HITS", "NO FALLS DOWN PITS", "NEVER CAUGHT ON CAMERA", "NO FIRST AID USED" };
  drawVectorText(scene == SC_STAGE_CLEAR ? "STAGE 1 BONUS" : "END OF GAME BONUS", SCREEN_W / 2, 236, 2.6f, rgb565(255, 190, 70), true);
  for (int i = 0, row = 0; i < BONUS_LINES; i++) {
    if (s2End && i == 1) continue;           // (no electrocution in stage 2)
    if (gameOverT < 0.6f + row * 0.2f) break;
    if (i == 0 && s2End) {
      long n = bonusPts[0] / MISSILE_BONUS;
      snprintf(b, sizeof(b), "MISSILES STOPPED  %ld X %d", n, MISSILE_BONUS);
    } else if (i == 0) {
      snprintf(b, sizeof(b), "TIME LEFT  %d MINUTE%s X %d", mins, mins == 1 ? "" : "S", BONUS_PER_MINUTE);
    } else {
      snprintf(b, sizeof(b), "%s", what[i]);
    }
    float y = 272 + row * 30;
    row++;
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
  drawVectorText("STOP THE NUCLEAR LAUNCHES", SCREEN_W / 2, y0 + 100, 3.4f, rgb565(230, 236, 245), true);
  drawVectorText("DESTROY THE MISSILES IN FLIGHT", SCREEN_W / 2, y0 + 146, 3.4f, rgb565(230, 236, 245), true);
  drawVectorText("FIND AND ELIMINATE DR. VOSS", SCREEN_W / 2, y0 + 192, 3.4f, rgb565(255, 90, 70), true);
  // the quantum core: a glowing heart in its orbits
  float qx = SCREEN_W / 2, qy = y0 + 330, t = clockT;
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
  struct OldEntry { char name[4]; int32_t score; int32_t secs; };   // (before version 054: no deaths)
  if (got == sizeof(OldEntry) * HI_COUNT) {
    OldEntry o[HI_COUNT];
    memcpy(o, t, sizeof(o));
    for (int i = 0; i < HI_COUNT; i++) { memcpy(t[i].name, o[i].name, 4); t[i].score = o[i].score; t[i].secs = o[i].secs; t[i].deaths = 0; }
  } else if (got != sizeof(t)) {
    return;
  }
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

static void commas(char *out, int n, uint32_t v);
static void hiLine(char *b, int n, const HiEntry &e, int rank) {
  char d[24];
  commas(d, sizeof(d), e.deaths);
  snprintf(b, n, "%d.  %.3s  %07ld  %3d:%02d  %11s", rank, e.name, (long)(e.score % 10000000), (int)(e.secs / 60 % 1000), (int)(e.secs % 60), d);
}
static void resetHiScores() {
  memcpy(hiTable, hiDefault, sizeof(hiTable));
  saveHiScores();
}

static void goToTitle();

// The game is over (or won): a place in the table? then the initials, else the title
static void afterGame() {
  goBg = GB_NONE;
  goSceneOn = false;
  launchOn = false;
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
      hiTable[entrySlot].deaths = finalDeaths;
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
  {
    char num[24], line[64];
    commas(num, sizeof(num), finalDeaths);
    snprintf(line, sizeof(line), "NUCLEAR DEATHS: %s", num);
    drawVectorText(line, SCREEN_W / 2, 176, 2.4f, finalDeaths ? rgb565(255, 90, 70) : rgb565(100, 255, 140), true);
  }
  drawVectorText("ENTER YOUR INITIALS", SCREEN_W / 2, 212, 3.0f, rgb565(180, 184, 196), false);
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
  drawVectorText("RANK  INITIALS    SCORE    TIME PLAYED       DEATHS", SCREEN_W / 2, 452, 2.2f, rgb565(255, 190, 70), false);
  for (int r = 0; r < HI_COUNT; r++) {
    HiEntry e;
    if (r == entrySlot) {
      memcpy(e.name, entryName, 4);
      e.score = (int32_t)score;
      e.secs = (int32_t)playT;
      e.deaths = finalDeaths;
    } else {
      e = hiTable[r > entrySlot ? r - 1 : r];
    }
    hiLine(b, sizeof(b), e, r + 1);
    drawVectorText(b, SCREEN_W / 2, 494 + r * 40, 3.0f, r == entrySlot ? rgb565(255, 220, 120) : rgb565(170, 174, 186), r == entrySlot);
  }
}

// ============================================================ the settings (Triangle), STAGE 1's briefing
#define SETTINGS_FILE QL_DATA_DIR "/settings.bin"
#define SETTINGS_ITEMS 7
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
  if (sitesHacked < NUKE_SITES) {                    // (and, for testing the launches: all 5 nuclear sites hacked)
    sitesHacked = NUKE_SITES;
    hackT = 0;
    if (sitesLaunched == 0) launchT = 0;
  }
  showMsg("JUMPED TO THE END OF STAGE 1 - ALL 5 SITES HACKED", 2.5f);
}

static void updateSettings() {
  if (pressed & BTN_UP) { settingsSel = (settingsSel + SETTINGS_ITEMS - 1) % SETTINGS_ITEMS; quitConfirm = hiResetConfirm = false; sfxPlay(SFX_KEY); }
  if (pressed & BTN_DOWN) { settingsSel = (settingsSel + 1) % SETTINGS_ITEMS; quitConfirm = hiResetConfirm = false; sfxPlay(SFX_KEY); }
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
  } else if (settingsSel == 4 && go) {             // RESET HIGH SCORES: X twice
    if (hiResetConfirm) { resetHiScores(); hiResetConfirm = false; hiResetDoneT = 2.0f; sfxPlay(SFX_POWERUP); }
    else { hiResetConfirm = true; sfxPlay(SFX_CLICK); }
  } else if (settingsSel == 5 && go) {             // QUIT TO TITLE: X twice
    if (quitConfirm) quitToTitle();
    else quitConfirm = true;
    sfxPlay(SFX_CLICK);
  } else if (settingsSel == 6 && go) {             // RESUME
    settingsOn = false;
    sfxPlay(SFX_CLICK);
  }
}

static void drawSettings() {
  gfxClipAll();
  if (hiResetDoneT > 0) hiResetDoneT -= 1.0f / 60;
  blendRect(0, HUD_H, SCREEN_W - 1, SCREEN_H - 1, 2, 4, 8, 0.72f);
  int x0 = 330, y0 = 100, x1 = 950, y1 = 640;
  fillRect(x0, y0, x1, y1, rgb565(12, 16, 26));
  drawRectOutline(x0, y0, x1, y1, rgb565(90, 220, 255), 3);
  drawVectorText("SETTINGS", SCREEN_W / 2, y0 + 40, 4.4f, rgb565(90, 220, 255), true);
  drawVectorText("GAME PAUSED", SCREEN_W / 2, y0 + 82, 2.2f, rgb565(255, 190, 70), false);
  for (int i = 0; i < SETTINGS_ITEMS; i++) {
    int y = y0 + 136 + i * 52;
    int item = i;
    bool sel = i == settingsSel;
    if (sel) fillRect(x0 + 24, y - 22, x1 - 24, y + 22, rgb565(36, 60, 90));
    uint16_t c = sel ? rgb565(255, 230, 120) : rgb565(220, 226, 236);
    const char *label = i == 0 ? "STAGE MUSIC" : i == 1 ? "ELEVATOR MUSIC" : i == 2 ? "JUMP TO STAGE 2" : i == 3 ? "ALL ACCESS (TEST)" :
                        i == 4 ? (hiResetDoneT > 0 ? "HIGH SCORES RESET" : hiResetConfirm ? "X AGAIN TO RESET" : "RESET HIGH SCORES") :
                        i == 5 ? (quitConfirm ? "X AGAIN TO QUIT" : "QUIT TO TITLE") : "RESUME";
    if (item <= 1 || item == 3) {
      bool on = item == 3 ? s2AllAccess : musicOn[i == 0 ? 1 : 0].load();
      drawVectorText(label, x0 + 50 + vectorTextWidth(label, 3.0f) / 2, y, 3.0f, c, true);
      const char *v = on ? "ON" : "OFF";
      drawVectorText(v, x1 - 60 - vectorTextWidth(v, 3.0f) / 2, y, 3.0f, on ? rgb565(100, 240, 140) : rgb565(255, 90, 70), true);
    } else {
      drawVectorText(label, SCREEN_W / 2, y, 3.0f, (i == 5 && quitConfirm) || (i == 4 && hiResetConfirm) ? rgb565(255, 90, 70) : i == 4 && hiResetDoneT > 0 ? rgb565(100, 240, 140) : c, true);
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
  drawVectorText("RANK  INITIALS    SCORE    TIME PLAYED       DEATHS", SCREEN_W / 2, y0 + 62, 2.2f, dimmed(150, 200, 220, f), false);
  for (int r = 0; r < HI_COUNT; r++) {
    float k = clampv((t - 0.3f - r * 0.18f) / 0.3f, 0.0f, 1.0f);
    if (k <= 0) continue;
    char b[64];
    hiLine(b, sizeof(b), hiTable[r], r + 1);
    drawVectorText(b, SCREEN_W / 2, y0 + 108 + r * 44, 2.9f, r == 0 ? dimmed(255, 220, 120, k) : dimmed(236, 236, 242, k), true);
  }
}

// MISSION OBJECTIVES, in the title's panel (it takes turns with the high scores)
static void drawTitleObjectives(float t) {
  const int x0 = 196, y0 = TPANEL_Y0;
  bool blinkHead = fmodf(t, 1.2f) < 0.8f;
  drawVectorText("MISSION OBJECTIVES", SCREEN_W / 2, y0 + 16, 3.0f, blinkHead ? rgb565(255, 110, 80) : rgb565(170, 60, 44), true);
  static const char *const obj[3] = { "AVERT GLOBAL NUCLEAR ANNIHILATION", "NEUTRALISE THE QUANTUM COMPUTER", "BRING DR. VOSS TO JUSTICE" };
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
    if (i == 0) {                        // the radiation sign: yellow, three black blades round a hub
      fillCircle(ix, iy, 26, rgb565(250, 210, 40));
      for (int b = 0; b < 3; b++) {
        float a0 = -1.5708f + b * 2.0944f - 0.5236f, a1 = a0 + 1.0472f;
        int px[6], py[6];
        for (int q = 0; q < 3; q++) {
          float a = a0 + (a1 - a0) * q / 2;
          px[q] = ix + (int)(cosf(a) * 22); py[q] = iy + (int)(sinf(a) * 22);
          px[5 - q] = ix + (int)(cosf(a) * 7); py[5 - q] = iy + (int)(sinf(a) * 7);
        }
        fillPolygon(px, py, 6, rgb565(20, 20, 22));
      }
      fillCircle(ix, iy, 4, rgb565(20, 20, 22));
    } else if (i == 1) {                 // a quantum drive
      drawQuantumDrive((float)ix, (float)iy, 0.5f, fmodf(t, 0.8f) < 0.55f);
    } else {                             // a target's cross hairs, on him
      uint16_t rc = fmodf(t, 1.0f) < 0.6f ? rgb565(255, 70, 50) : rgb565(170, 40, 30);
      drawCircle(ix, iy, 22, rc);
      drawCircle(ix, iy, 21, rc);
      drawCircle(ix, iy, 9, rc);
      fillRect(ix - 30, iy - 1, ix - 12, iy + 1, rc);
      fillRect(ix + 12, iy - 1, ix + 30, iy + 1, rc);
      fillRect(ix - 1, iy - 30, ix + 1, iy - 12, rc);
      fillRect(ix - 1, iy + 12, ix + 1, iy + 30, rc);
    }
    drawVectorText(obj[i], x0 + 230 - slide + vectorTextWidth(obj[i], 3.1f) / 2, y, 3.1f, dimmed(236, 236, 242, k), true);
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

// The title's background: NUCLEAR MISSION's launch, over and over (a random site); every sixth, a landing (a random city)
static int titleLaunches = 0;
#define TS_CROP_Y 170                        // (the part of the scene's square shown: 720 wide, 405 high, filling the screen)
static void updateTitleScene(float dt) {
  sfxMute = true;
  if (!launchOn) {
    launchQueued = 0;
    if (titleLaunches % 6 == 5) {
      int c = fxRnd(0, N_CITIES);
      landCity[0] = c;
      landDeaths[0] = cities[c].lo + (uint32_t)fxRnd(0, 1000) * ((cities[c].hi - cities[c].lo) / 1000);
      deathTotal = landDeaths[0];
      startLaunchScene(8);
    } else {
      startLaunchScene(fxRnd(0, NUKE_SITES));
    }
    titleLaunches++;
  }
  updateLaunchScene(dt);
  sfxMute = false;
}
static void drawScaledScene(int cropY, uint32_t bright);
static void drawTitleScene() {
  pxRect(0, 0, FB_W - 1, FB_H - 1, 0);
  if (!launchOn) return;
  drawScaledScene(TS_CROP_Y, 154);
}
// The cut scene's square (a launch, a landing), a crop of it scaled up to fill the screen, darkened (bright/256)
static void drawScaledScene(int cropY, uint32_t bright) {
  lsNoNames = true;
  drawLaunchScene();
  lsNoNames = false;
  gfxClipAll();
  for (int y = 0; y < LS_SIZE; y++) memcpy(frameSave + y * LS_SIZE, g_frame + (LS_Y + y) * FB_W + LS_X, LS_SIZE * 4);
  const int SH = LS_SIZE * FB_H / FB_W;      // (405 rows of it, scaled up to the whole screen: bilinear, two channels at a time)
  static int16_t xs[FB_W];
  static uint16_t xw[FB_W];
  static bool xsDone = false;
  if (!xsDone) {
    for (int x = 0; x < FB_W; x++) {
      int fx = (x * LS_SIZE << 8) / FB_W;
      xs[x] = (int16_t)(fx >> 8 > LS_SIZE - 2 ? LS_SIZE - 2 : fx >> 8);
      xw[x] = (uint16_t)(fx & 255);
    }
    xsDone = true;
  }
  auto lerp = [](uint32_t p, uint32_t q, uint32_t w) -> uint32_t {   // (p..q by w/256, per channel)
    uint32_t rb = ((p & 0xFF00FF) * (256 - w) + (q & 0xFF00FF) * w) >> 8 & 0xFF00FF;
    uint32_t g = ((p & 0x00FF00) * (256 - w) + (q & 0x00FF00) * w) >> 8 & 0x00FF00;
    return rb | g;
  };
  for (int y = 0; y < FB_H; y++) {
    int fy = (y * SH << 8) / FB_H, sy = cropY + (fy >> 8), wy = fy & 255;
    if (sy > LS_SIZE - 2) sy = LS_SIZE - 2;
    const uint32_t *r0 = frameSave + sy * LS_SIZE, *r1 = r0 + LS_SIZE;
    uint32_t *d = g_frame + y * FB_W;
    for (int x = 0; x < FB_W; x++) {
      int sx = xs[x];
      uint32_t w = xw[x];
      uint32_t v = lerp(lerp(r0[sx], r0[sx + 1], w), lerp(r1[sx], r1[sx + 1], w), (uint32_t)wy);
      d[x] = lerp(0, v, bright) | (r0[sx] & 0xFF000000);   // (darkened, under the words)
    }
  }
}

static void drawTitle() {
  gfxTarget = g_frame;
  gfxClipAll();
  drawTitleScene();                          // NUCLEAR MISSION's launch (and now and then a landing) behind it all
  gfxClipAll();
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
  // the panel: the mission objectives, then the high scores, 15 seconds each (on a dark band, over the scene)
  blendRect(150, TPANEL_Y0 - 14, SCREEN_W - 151, TPANEL_Y0 + 336, 0, 0, 0, 0.55f);
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
  stage = 1;                                 // (first: the layout and the first lift stop are stage 1's, even after a game that ended in stage 2)
  scene = SC_NONE;
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
  passShowT = 0;
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
  launchOn = false;                          // (the title's own launches, from the first)
  launchQueued = 0;
  titleLaunches = 0;
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
    updateTitleScene(dt);
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
    if (goBg == GB_NUKE) {                   // the landing, then the city burning on (no end to it)
      if (!goSceneOn && gameOverShownT() >= 0) goStartScene();
      if (goSceneOn) updateLandScene(dt);
      if (goPage == 0) {
        if (goSceneOn && goStrikesDone() && (pressed & BTN_JUMP)) { goPage = 1; goPageT0 = gameOverShownT(); sfxPlay(SFX_KEY); }
        pressed = 0;
      }
    }
    if (gameOverShownT() - goPageT0 > 2.2f && (pressed & BTN_JUMP)) {
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
      if (stage != 2) timeLeft -= dt;
      updateNukes(dt);
      if (stage == 2) {                      // (the countdown: the next launch, else the next impact)
        bool impact;
        float left = s2Countdown(&impact);
        timeLeft = left >= 0 ? left : 999;
      }
      updateHealthAlerts(dt);
      updateFlyers(dt);
      updateLaunchScene(dt);
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
      if (timeLeft <= 0 && stage != 2) {
        timeLeft = 0;
        endGame(END_TIME);
      }
    }
    if (hurtFlashT > 0) hurtFlashT -= dt;
    if (msgT > 0) msgT -= dt;
    if (nothingShowT > 0) nothingShowT -= dt;
    if (itemShowT > 0) itemShowT -= dt;
    if (docShowT > 0) docShowT -= dt;
    if (passShowT > 0) passShowT -= dt;
    if (cardShowT > 0) cardShowT -= dt;
    if (shotFlashT > 0) shotFlashT -= dt;
    if (gunFlashT > 0) gunFlashT -= dt;
    updateMyShots(dt);
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
        else if (termKind == 2) updateServerNode(dt);
        else if (termKind == 3) updateDestructTerm(dt);
        else updateTerminal(dt);
      }
    } else {
      if (introT < 0) {
        if (arOn()) updateArena(dt);      // (lift D's rooms: from above)
        else if (s2Act) updateS2Act(dt); // (stage 2: knocking out, searching, binning the maintenance man)
        else if (simOn) updateSimon(dt);  // (the CODE ROOM's sequence: the controller works the board)
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
  } else if (arOn()) {
    gfxClip(HUD_H, PLAY_Y1);
    drawArena();
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
  if (scene != SC_RIDE && !arOn()) {
    drawHealthAura();
    if (stage == 2 && s2Disguised) drawPersonU(shownPose, agX, agY, agFacing, myScale, PK_MAINT);   // in the maintenance man's overalls
    else drawAgentU(shownPose, agX, agY, agFacing, myScale);
  }
  if (stage == 2 && !arOn() && (mode == MODE_ROOM || onWalkway()) && hasFlamer && scene == SC_NONE) {   // the FX-5 in his hands
    int hx, hy, mx, my;
    agentHand(shownPose, agX, agY, agFacing, myScale, hx, hy, mx, my);
    drawFlamer(hx + agFacing * 10.0f, (float)hy, 0.28f, 0.6f, agFacing);
  }
  if (stage == 2 && !arOn()) drawFlames();
  if (stage == 2 && mode == MODE_ROOM && !arOn() && (s2Act == 5 || (hasGun && !s2Act && scene == SC_NONE))) {   // his pistol (and its flash)
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
  for (int k = 0; k < MY_SHOTS; k++)            // his bullets in flight
    if (myShots[k].on) {
      const MyShot &b = myShots[k];
      if (b.ray) {                               // the QX-9's bolt: blue, glowing
        glowDisc(b.x, b.y, 22, 90, 180, 255, 0.6f);
        fillRectF(b.x - b.dir * 30.0f, b.y - 2, b.x, b.y + 2, rgb565(120, 200, 255));
        fillRectF(b.x - 4, b.y - 4, b.x + 4, b.y + 4, rgb565(230, 246, 255));
        continue;
      }
      fillRectF(b.x - b.dir * 16.0f - 1, b.y - 1, b.x + 1, b.y + 1, rgb565(255, 230, 140));
      fillRectF(b.x - 3, b.y - 2, b.x + 3, b.y + 2, rgb565(255, 255, 230));
    }
  for (int k = 0; k < 4; k++)                    // the foyer guard's bullets
    if (gShots[k].on) {
      const GShot &g = gShots[k];
      fillRectF(g.x - g.dir * 18.0f - 1, g.y - 1, g.x + 1, g.y + 1, rgb565(255, 150, 90));
      fillRectF(g.x - 3, g.y - 2, g.x + 3, g.y + 2, rgb565(255, 240, 200));
    }
  if (mode == MODE_SHAFT && stage == 2) {
    if (fgLoaded >= 0) drawNpcs();
    drawCarriedBody();
    drawS2Prompts();
  }
  if ((mode == MODE_ROOM || fgLoaded >= 0) && stage == 2 && !s2Act && agState == ST_IDLE && gameOver == END_NONE && atNpcToKill() >= 0) {
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
  if (mode == MODE_ROOM && stage == 2 && curRoom < S2_ROOMS && (s2Kind[curRoom] == LK_CLONE || s2Kind[curRoom] == LK_QLAB2)) drawGas();
  if (mode == MODE_ROOM && stage == 2 && curRoom < S2_ROOMS && s2Kind[curRoom] == LK_HAZARD) drawHazStorage();
  if (mode == MODE_ROOM && (curHazard == HZ_DRONE || curHazard == HZ_CAMDRONE)) drawDrone();
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
  if (passShowT > 0 && mode == MODE_ROOM && !mapOn && gameOver == END_NONE) drawPassPanel();
  if (stage == 2 && mode == MODE_ROOM && npcCount > 0 && !mapOn && gameOver == END_NONE && !s2NoSusp()) drawSuspicionMeter();
  if (!mapOn && !termOn) drawHealthEdges();
  if (!mapOn) drawFlyers();
  if (congratsT > 0 && !termOn && !mapOn && gameOver == END_NONE) drawCongrats();
  if (stage == 2 && s2NukeCode && !termOn && !mapOn && gameOver == END_NONE && scene == SC_NONE) {   // (holding the code: a reminder)
    blendRect(SCREEN_W - 230, HUD_H + 6, SCREEN_W - 12, HUD_H + 32, 60, 6, 6, 0.8f);
    drawVectorText("NUKE CODE HELD", SCREEN_W - 121, HUD_H + 19.0f, 1.8f, rgb565(255, 200, 80), true);
  }
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
  if (launchOn && !mapOn && !settingsOn && gameOver == END_NONE) composeLaunchFrame();   // a launch: beside the game, shrunk
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
  droneHum.store(mode == MODE_ROOM && (curHazard == HZ_DRONE || curHazard == HZ_CAMDRONE) && !mapOn && !settingsOn && gameOver == END_NONE ? 1 : 0, std::memory_order_relaxed);
  int was = liftHum.exchange(hum, std::memory_order_relaxed);
  if (was != 0 && hum == 0 && !mapOn) sfxPlay(SFX_LIFT_STOP);
  if (!(gameOver != END_NONE && goBg != GB_NONE && gameOverShownT() >= 1.0f)) drawHud();   // (the end's own background: no top bar)
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
