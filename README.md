# QUANTUM LOCKDOWN for the PS5

A new full-screen game in the style of Impossible Mission, built from
NUCLEAR MISSION's parts. Version 023: a title screen, the lift shafts, four
room designs (four levels each), a new layout of the complex every game,
equipment to search, the score, health, first aid kits, a 15 minute
countdown, the lift area's panel with its mini map, music and sound, things
on the walls (warning lights, fans, coolant tubes...), and room hazards:
electrified floors, dark rooms, security cameras and drones; 18 QUANTUM
DRIVES to find, 3 power generators, QUANTUM CONTROL with its drive bay and
main computer, the sub basement it unlocks and the QUANTUMLOOP to stage 2,
stage 2's QUANTUM LAB (six lifts, 21 rooms, card readers, the kitchen and
its chef, the maintenance man to knock out, search and hide in a bin), and a
top 5 high score table.

## What's different from NUCLEAR MISSION's PS5 port

NUCLEAR MISSION is the Waveshare sketch running on a stand-in for the board
(a 720x720 picture scaled onto the TV; its code is in the nuclear-mission repository). This game is written for the PS5:
it draws the whole 1920x1080 picture itself.

- The game works in **units**: a 1280x720 screen drawn at 1.5x. NUCLEAR
  MISSION's 720x720 screen was also shown at 1.5x on the TV, so its shapes and
  sizes carry straight over (the agent at scale 1.3 is the same size on the
  TV); there is just a wider and taller view round him.
- `gfx.cpp` has the sketch's drawing calls with the same names and arguments
  (`fillRect`, `fillPolygon`, `fillCircle`, `drawLine`, `drawVectorText`,
  `hazardBand`...), so painters can be copied from the sketch as they are. The
  agent is drawn at full resolution.
- `agent_rig.h` and `people.h` are NUCLEAR MISSION's, unchanged.

## The complex

30 rooms in 9 columns, laid out afresh each game: every column has at least
2 and at most 6 rooms, on any of 6 rows. A lift shaft runs down each side of
every column: 10 shafts, A to J. Every room has a door on its left and on its
right, each at the room's **top** or its **bottom** (picked at random for
each room). A door opens onto the shaft on that side, at the floor that
matches it: each row has two floor positions in the shaft (the top doors',
then the bottom doors'), 12 in all.

The lift car only stops at floors with a door. Everywhere else the shaft is
wall, and the car runs past it (picking up speed on a long run). A catwalk
leads off the car only on the side the door is.

The title screen tells the story, typed out as a classified transmission
that glitches now and then. Each new game (X on the title screen) lays out a
new complex and opens, as NUCLEAR MISSION's lab does, with him coming out of
an air-conditioning duct in the catwalk's ceiling at shaft A - the top left
corner, at its first floor with a room - landing between the lift and the
door to the first room (X skips it). The clock starts when he lands.

Each room is one of the four layouts in `src/rooms_wide.h` (FOUR DECKS,
CENTRE LIFT, TWIN TOWERS, THE CHASM), picked at random, each with four levels
(the floor and three above it).

## Equipment and the score

Every room has 4 or 5 pieces of equipment to search, 128 in the whole complex,
each a different kind in its room, from twelve: from NUCLEAR MISSION a filing
cabinet, safe, plant, radar, control panel, wooden desk, water cooler,
bookshelf and coat stand; new, a desk with a computer (its screen scrolling),
a steel cupboard and a computer rack (its lamps winking, tapes turning).

Each column of rooms has its own wall colour (teal, maroon, olive, navy,
purple, rust, forest green, mustard, plum - `wallColours` in `src/game.cpp`). 18 of them, each in a different room, have
a QUANTUM DRIVE in them, and 5 others (each in a different room) TOP SECRET
DOCUMENTS; the rest have nothing.

- Searching a piece (hold Square) scores **100**. A piece is searched once:
  after that Square there just says SEARCHED. Each piece remembers how far it
  was searched: let go half way, come back later, and it carries on from
  there (a dim bar over it shows how far).
- Finding a drive shows **QUANTUM DRIVE FOUND** (a picture of the drive) and
  scores **200** more. He carries the drives until he loads them into the
  drive bay in QUANTUM CONTROL.
- Finding **TOP SECRET DOCUMENTS** shows their panel (a folder stamped TOP
  SECRET, as NUCLEAR MISSION's) and scores bonus points: **1000, 1500, 2000,
  2500 and 5000**, in the order found. They are only for points.
- Searched pieces get a tick: green for nothing, gold for a drive, red for
  documents. On the map
  each room you've been in shows its pieces the same way (grey: not searched).

## On screen

- **The top bar:** the score on the left, the countdown in the middle, the
  health bar on the right.
- **The lift area** is laid out like Impossible Mission's: the shaft view
  across the top with the lift in the middle, and a panel under it. In the
  panel, under the lift, the **mini map** - a yellow screen in a red-striped
  frame showing the rooms you've been in and the shafts as far as you've
  ridden them, your place blinking. On its left: the lift and floor, and the
  car's up / down arrows (lit while it moves). On its right: rooms visited,
  objects found, first aid kits left, the version and frame rate.
- Nothing can be searched in the lift area.

## Sound

- **Music:** `assets/elevator.mod` while he stands in the lift car;
  `assets/stage1.mod` the rest of the time (stage 1 is the only stage so far).
  The two cross-fade, and each picks up where it left off - they are two
  separate copies of the MOD player (`micromod.c` and `micromod2.c`).
- **Title music:** `assets/title.mod` on the title screen (and while initials
  are entered), from the top each time the title comes back; a third copy of
  the player (`micromod3.c`). It fades out as the game starts.
- **The lift moving:** a low buzzing motor hum that throbs, its pitch drifting
  up as it rises and down as it falls, after Impossible Mission's (room lift
  pads hum a little quieter); a soft thump when it stops.
- **Running:** NUCLEAR MISSION's footstep, each stride.
- **Searching:** NUCLEAR MISSION's beeps while searching, its clunk for
  nothing, and for a find its chirp and rising four notes (those notes too
  when a first aid kit fills the health bar).
- **Falling down a pit:** a scream (`src/scream_sound.h`, made from
  scream.mp3: trimmed to its 2.2 seconds of scream and built into the game, so
  there is no file to copy). `SCREAM_VOLUME` sets its level.
- **Cameras and the drone:** NUCLEAR MISSION's alarm when caught; the
  drone's rotors whine quietly while it is in the room, its gun cracks, and a
  hit thuds.
- **Hazards:** NUCLEAR MISSION's electrified floor sounds (the warning buzz,
  the crackle while it is live and while he is shocked) and its power-down
  when a dark room's lights go out.
- The levels are at the top of `src/game.cpp`: `ELEVATOR_MUSIC_VOLUME`,
  `STAGE1_MUSIC_VOLUME`, `SFX_VOLUME`, `LIFT_HUM_VOLUME`.

## Health, first aid and the clock

- **Health:** a bar of 50 points in the top bar. Every minute of play costs 1. Dropping one level costs
  nothing; two levels cost 5; three (top to bottom) cost 15, and knock him
  down for a moment. Falling down a pit costs 15 health and **30 seconds** off
  the clock, and puts him back at the door he came in by. At 0 the game is over.
- **First aid kits:** 5 in the whole complex, each in a different room, on
  the wall (a white case with a red cross). Search one (hold Square) and the
  health bar is full again; each kit works once (it hangs open after), and
  not while the bar is already full. The map marks a room's unused kit.
- **The last minute:** every 10 seconds (at 1:00, 0:50 ... 0:10) an alert
  sounds and the lights flicker.
- **The clock:** 15 minutes from the start. At 0:00 the power fails: the
  generators wind down, the lights flicker and go out, his night vision comes
  on, and **MISSION FAILED - POWER FAILURE** comes up: the game is over. It is
  red in the last minute. The map (Options) pauses it, and everything else.
- **Game over:** why, the score, what was found; X goes back to the title.

## Power generators

3 rooms (at random) have a POWER GENERATOR: a yellow generator on a skid with
a lever on its control box, down (off) and a red lamp. Search it (hold
Square): the lever goes up, the lamp turns green and its engine fan starts
turning (100 points), and **5 minutes** go back on the clock (each one). All 3 must be on before the main computer in QUANTUM
CONTROL will start. The map marks a generator in a room you've been in (its
lamp red or green), and the lift area's panel counts them.

## QUANTUM CONTROL

A room of its own in the middle column of the map (between lifts E and F), on
a random row, one level high, with a door at the bottom on each side. Its
equipment is NUCLEAR MISSION's LCC, with the new **QUANTUM DRIVE BAY** (18
slots and a counter) on the left; the main computer and its printer in the
middle, a display over it (the countdown, once there is power), and on the
right the reel to reel tape units, the fan and three computer racks. The map always shows it (in
purple).

- **The drive bay:** stand at it and press Square: every drive he is carrying
  goes in, one by one (the slots light up).
- **The main computer:** stand at it and press Square.
  - Without all 3 generators on: a NO POWER panel, with a picture of a
    generator and how many are on.
  - With power: the close-up of the computer (as NUCLEAR MISSION's launch
    computer). It boots, checks the drive bay, and with fewer than 18 drives
    loaded stops at **UNABLE TO BOOT** (X logs off).
  - With all 18 loaded it boots to a menu: UP / DOWN to choose, X or Square to
    pick. Most items only show a page (core status, temporal field, personnel
    files...). **UNLOCK SUB BASEMENT LIFT ACCESS** names a lift shaft, picked
    at random from those that go down to the bottom floor. LOG OFF to leave.
    The clock keeps running while he is at the computer.

## The sub basement, and on to stage 2

Once unlocked, that shaft's car goes one level further down, below the
complex's bottom floor (the panel says FLOOR SB). The mini map doesn't show
it; the Options map does, under the bottom row. There the lift area has a
wall on the left and, on the right, a sign - **QUANTUMLOOP** and an arrow -
and the door into the QUANTUMLOOP station.

Going in ends stage 1:

1. **STAGE 1 COMPLETE**: the stage 1 bonus, worked out as at the end of the
   game (minutes left x 500, 1000 each for no electrocution, drone hits, pit
   falls, camera catches, first aid). The incidents are then counted afresh
   for stage 2; the clock carries on from where it is.
2. **STAGE 2**: GET TO THE QUANTUM LAB AND DISABLE THE QUANTUM CORE (and
   `assets/stage2.mod` starts, if STAGE MUSIC is on). X to play.
3. **The station** (after the picture: QUANTUMLOOP TRANSPORT SYSTEM, the
   departure board, benches, the portal and the transport's tube): the
   **QUANTUMLOOP POWER** switch on the left - stand at it, Square: the
   portal's light arcs, the tube's strip, the sign and the board come on in
   turn, with a rising whine and a chime (NEXT DEPARTURE STAGE 2, NOW
   BOARDING). Then the **doors** (Square at them): they slide open with a hiss.
   The way back up is shut once stage 2 has begun.
4. **On board** (the clock waits): he walks in, fade to black, then the
   cabin: him on a seat, the tunnel rushing past the windows, the display
   counting down to arrival, a deep rumble - about 20 seconds
   (`RIDE_SECONDS`). Fade to black again.
5. **The arrival station**: the same, its board saying ARRIVED STAGE 2 and
   QUANTUM LAB with an arrow. He steps out, the doors close: head left, into
   lift A at its lowest stop (LEVEL Q).

MISSION COMPLETE isn't reached any more for now; the game ends with the
clock, health, the maintenance man being found, or QUIT TO TITLE.

## Stage 2: the QUANTUM LAB

The lift areas have NUCLEAR MISSION's LAB look: white walls in panels, the
air-conditioning duct with the red and black stripe under it, glass rails
along white walkways, and a glass-backed white lift car with a blue light.

**The layout** (always the same; `layOutStage2`): six lifts side by side,
A to F. Each has a room on its right at each of its levels, counted from the
top level down - lift A 6 rooms (levels 6 to 1), B 5, C 4, D 3, E 2, F 1:

```
 level 6   A[ ]B[ ]C[ ]D[ ]E[ ]F[ ]
 level 5   A[ ]B[ ]C[ ]D[ ]E[ ]
 level 4   A[ ]B[ ]C[ ]D[ ]
 level 3   A[ ]B[ ]C[ ]
 level 2   A[ ]B[ ]
 level 1   A[ ]
 level Q   A[QUANTUMLOOP]
```

A room's left door is at the end of its lift's right walkway; its right door
(where the next lift comes down that far) at the end of the next lift's left
walkway - so the bottom room of each column is a dead end. Rooms are one
level and empty for now (they'll be built up one at a time), except the
kitchen.

**Card readers**: every door into a room has a reader beside it, with its
access level (L1 to L5) under it - red lamp and a shut white door with a red
band while his card isn't good enough (walking into it: ACCESS LEVEL n
REQUIRED), green and open once it is. Once he has been in a room its doors
stay open. Lift A's rooms are level 1 (the kitchen) and 2 to 5 at random;
lift B's level 2, C's 3, D's 4, E's and F's 5.

**Signs**: each door has a sign beside it saying what's through it. Lift A's
six rooms are, in a random order each game: KITCHEN, MEDICAL BAY, SECURITY,
ADMIN, CLEANING and STORE ROOM (the sixth name, for now). The rest say LAB
and where they are (LAB B4...).

**On every left walkway**: a green **first aid cabinet** (Square under it:
full health, once - then it hangs open, empty). **Lift A's** left walkways
also have a **bin** big enough for a man, and lift A's left walkways are dead
ends (no door).

**The maintenance man** waits for lift A at one of its six levels (random),
on the left walkway by the call buttons. When the car stops at his level he
starts, and runs for the end of the walkway, where he cowers. He's slower
than you: get close and press **X** to knock him out. The clock in the
middle of the top bar becomes **BODY FOUND IN 1:00**, counting down (red;
flashing and beeping in the last ten seconds); at zero: MISSION FAILED, the
body was found. Then, beside him:

1. **Square**: search him - his **level 1 access card** and his overalls:
   you're disguised as maintenance (blue overalls, hi-vis band, cap).
2. **Square** again: the bin - you lift him over your shoulder, carry him to
   the bin, lid up, tip him in, lid down. The top bar shows the mission's
   countdown again.

Points: 500 for the knock out, 1000 for the search, 500 for the bin.

**The kitchen** (access level 1): two levels - the cooking on the floor (a
double fridge, the range with its ovens, pans and extractor hood, a steel
prep table with utensils hanging over it, the sink and a shelf of plates, a
white-tiled wall) and the stores on a deck above (racks of boxes, tins,
sacks of flour and rice, jars, a pallet of bottled water), reached by the
lift pad on the left. **The chef** walks one of two paths back and forth -
the floor (fridge to sink) or the deck (along the racks) - pausing at each
end to work. Coming in, he's on one of them, with his back to you, walking
away. He doesn't notice you yet (for testing).

**The panel** in stage 2: LIFT A..F and the level; ROOMS (of 21), ACCESS
CARD, DISGUISE, FIRST AID LEFT, SCORE. The mini map and the Options map show
the lifts and rooms (filled once visited; the Options map with the names,
access levels and which are locked).

## End of game bonus

However the game ends, the score gets a bonus, shown line by line on the
final screen:

| | |
| --- | --- |
| each whole minute left on the clock | 500 |
| never electrocuted | 1000 |
| never hit by a drone's bullet | 1000 |
| never fell down a pit | 1000 |
| never caught on camera | 1000 |
| no first aid kit used | 1000 |

The amounts are at the top of `src/game.cpp` (`BONUS_PER_MINUTE`, `BONUS_CLEAN`).

## The high scores

When the game ends (time, health, or MISSION COMPLETE), a score good enough
for the top 5 asks for initials (as NUCLEAR MISSION's): UP / DOWN to change
the letter, X (or Square, or RIGHT) for the next, LEFT to go back. The table
has the initials, the score and the time played. The title screen's panel
takes turns, 15 seconds each: MISSION OBJECTIVES, then TOP 5 AGENTS. On the PS5 the table is kept in
`/download0/quantum-lockdown/hiscores.bin` (the title's own storage).

## On the walls

Each room has up to 2 things on its walls that are only to look at, placed
where they keep clear of the platforms, lifts, equipment, doors and the room's
number:

- **Red warning lights** (NUCLEAR MISSION's): pulsing red with a glow, faster
  in the last minute.
- **Wall fans**, spinning behind their guards, and a **big extractor fan**
  (half the time) where a room has a big open space between its floors.
- **Coolant tubes** with bubbles rising, **pressure gauges** with twitching
  needles, and the odd **notice** (RESTRICTED AREA, WARNING TIME FIELD...).

## Room hazards

None in room 1, where he starts. The map marks the ones he has been in.

- **Electrified floor** (6 rooms, 20%; from NUCLEAR MISSION): a HIGH VOLTAGE
  sign on the wall. Every 4 to 7 seconds the lights flicker and the floor
  crackles with a buzz (the warning, 1.6 seconds), then the floor is live
  with blue arcs for 2.6 seconds. Be off it: up on a ledge, on a lift pad
  above the floor, or in the air (a somersault timed over it). Caught on it,
  he is shocked: 10 health and **30 seconds** off the clock lost, and he wakes
  at the door he came in by. The
  shock trips the power, so it stays safe for a while after. The floor's
  timing settings are at the top of `src/game.cpp` (`ELEC_...`).
- **Dark room** (3 rooms, 10%; NUCLEAR MISSION's blackout): the lights flicker
  and die a moment after he comes in, and his night vision comes on. Only what
  the goggles light up is seen, in green: a cone the way he faces and a little
  round him. The EXIT lamps, red lights and lift pad lamps still glow.

- **Security cameras** (6 rooms with one, 20%; 3 rooms with two, 10%): hung
  from the ceiling in a top corner (or both), each sweeping its beam across
  the room and back every 6 seconds (with two, the second takes 7.1, so the
  beams drift apart). The beam goes through everything:
  walls, floors, equipment. Standing still in it is safe (searching too);
  moving in it (running, jumping, falling, riding a lift pad) is not. The beam
  goes amber while it is on him, red when it catches him: an alarm,
  "CAUGHT ON CAMERA! -30 SECONDS", 30 seconds off the clock, hands up, and back
  at the door he came in by. Each camera starts its sweep at the far end, and
  gives him a moment after he comes in.
- **Drone** (3 rooms, 10%): it starts at the far side of the room, its narrow
  red scanning beam turning right round it (through anything). The beam sees
  him whether he moves or not. When it does, the beam goes off ("THE DRONE
  HAS SEEN YOU" the first time) and the drone flies to the spot where it saw
  him (just over his head) - not after him - and there it scans again, and so
  on. It flies anywhere in the room (through anything), slowly - he runs twice as fast. Its light: green
  out of range, yellow getting close, red (blinking) in range, and then it
  fires, whether it is scanning or flying (it carries on to where it was
  going): a bullet every 0.8 seconds, 5 health each, aimed where he is when it
  fires, so a moving target is hard to hit. Run, put distance between, or leave
  the room. It starts again from the far side whenever he comes in.

The settings for these are at the top of `src/game.cpp` (`CAM_...`, `DRONE_...`,
`ELEC_...`, the numbers of rooms). One hazard at most in a room; 9 rooms (30%, room 1 among them) have none.

`sim/check_layout.cpp` lays out 2000 complexes and checks the counts (128
pieces, 4 or 5 a room, 18 drives, 5 documents, 5 first aid kits, 2-6 rooms a column, the
hazard rooms), that every piece stands on its platform clear
of the lifts, that no two overlap, and that every electrified room has its sign. `sim/check_rooms.cpp` checks that every
layout can be crossed with every door combination and every spot reached:
run it after adding or changing a layout.

## The title screen, the briefing

The title sits at the top (title.mod playing), with the quantum core turning behind it (three
orbits, a particle on each), motes drifting up, a camera's beam sweeping the
dark, sparks along the floor, and now and then the name glitches red and
cyan. Under it, open on the background (no box), these take turns every 15
seconds:

- **MISSION OBJECTIVES:** 1. POWER ON THE GENERATORS 2. FIND ALL QUANTUM
  DRIVES 3. UNLOCK SUB BASEMENT ACCESS
- **TOP 5 AGENTS:** the high scores

After X and the opening (out of the duct), the **STAGE 1** briefing: ABANDONED
FACILITY, COUNTDOWN TO POWER FAILURE 15:00, RESTORE 3 GENERATORS, with
pictures of the generators. The clock waits; X again to start playing.

## Settings

**Triangle** (any time in play, not on the map) opens the settings, and
pauses the game and the clock:

- **STAGE MUSIC** ON / OFF
- **ELEVATOR MUSIC** ON / OFF (both remembered: `settings.bin` beside the
  high scores)
- **JUMP TO STAGE 2** (for testing): straight to the sub basement's lift
  area at the end of stage 1 (unlocked), ready to go into the station
- **ALL ACCESS (TEST)** ON / OFF (for testing stage 2): every card reader
  opens (the panel's ACCESS CARD says ALL (TEST))
- **QUIT TO TITLE** (X twice): drops the game there and then, no score
- **RESUME** (or Triangle again)

## Controls

| DualSense | |
| --- | --- |
| X (Cross), on the title | start a new game |
| D-pad or left stick | run left / right; up / down rides the lift car or a lift pad, or calls a pad to you |
| Cross | somersault (stage 2, close to the maintenance man: knock him out) |
| Square or Circle | hold to search equipment (stage 2: search the maintenance man, put him in the bin, a first aid cabinet) |
| Options or touchpad | the map (shows every room and where its doors lead), and pauses the game |
| Triangle | the settings (pauses the game); on the map: lay out a new complex |

Lift pads start at the stop nearest the door he came in by. Standing beside a
pad's shaft at one of its stops, UP or DOWN calls the pad to him.

## Build and install

In WSL or Linux, put this repository inside the starter project folder
(ps5-homebrew-starter, after its `tools/setup.sh`), beside `kit/` - as
NUCLEAR MISSION's `silo/` sits there. Any folder name works; `mission2` here:

```bash
cd ~/ps5-homebrew-starter
git clone https://github.com/agrajag142/quantum-lockdown mission2   # (or unzip it there)
chmod +x mission2/build.sh          # (if it was unpacked in Windows)
mission2/build.sh                   # build/mission2/PPSA99903 and its ZIP
mission2/build.sh deploy <PS5 IP>   # or build and copy it to /data/homebrew over FTP
```

Later versions: `git -C mission2 pull`, then build again.

Then put it on the home screen with ShadowMountPlus. Title ID `PPSA99903`, so
it sits beside Hello PS5 and NUCLEAR MISSION.

## The PC test build

```bash
mission2/build.sh sim                     # plays sim/tour.txt, screenshots in build/mission2-sim/shots (and elevator.mod, if it's in assets)
g++ -O2 -Imission2/src mission2/sim/check_rooms.cpp -o check && ./check   # checks the room layouts
mission2/build.sh sim mission2/sim/hazards.txt   # an electrified floor (and a shock), a dark room, the walls
mission2/build.sh sim mission2/sim/cameras_drone.txt   # caught on camera, two cameras, the drone
mission2/build.sh sim mission2/sim/control.txt         # a generator, QUANTUM CONTROL, the computer, the sub basement
mission2/build.sh sim mission2/sim/stage2.txt          # JUMP TO STAGE 2, the station, the QUANTUMLOOP, stage 2's lift area
mission2/build.sh sim mission2/sim/lab.txt             # the lab: the maintenance man runs, knocked out, searched, binned
mission2/build.sh sim my_script.txt       # your own script (see sim/sim_main.cpp for the commands)
```

It runs the same game code with a scripted controller, so a change can be
checked on the PC before it goes on the console.

## What's in here

| | |
| --- | --- |
| `src/game.cpp` | the game: the complex, the shafts, the rooms, the agent, searching, the map (settings at the top: columns, rows, rooms, rooms per column) |
| `src/scream_sound.h` | the pit scream, as samples (24 kHz mono) |
| `src/rooms_wide.h` | the room layouts (1280 units wide, four levels) |
| `src/gfx.cpp`, `gfx.h` | drawing: the 1920x1080 picture, unit-space calls as in the sketch, vector letters |
| `src/ps5_platform.cpp` | the PS5 side: the TV picture, the DualSense, the sound, the loop |
| `src/micromod.c`, `.h` | the MOD player (Martin Cameron's Micromod, as in NUCLEAR MISSION); `micromod2.c`, `micromod3.c`, `micromod4.c` more copies of it (one a tune) |
| `src/agent_rig.h`, `people.h` | NUCLEAR MISSION's agent and people, unchanged |
| `sim/` | the PC test build, the room layout checker and the icon maker (not compiled into the app) |
| `sce_sys/` | `param.json` (PPSA99903) and the home-screen icon |
| `assets/` | files copied into the app: put `elevator.mod` here |
| `docs/PS5_NOTES.md` | what we learned getting NUCLEAR MISSION running on a real PS5 |
