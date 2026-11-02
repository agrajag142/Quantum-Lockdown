# QUANTUM LOCKDOWN for the PS5

A new full-screen game in the style of Impossible Mission, built from
NUCLEAR MISSION's parts. Version 003: a title screen, the lift shafts, four
room designs (four levels each), and a new layout of the complex every game.
No sound yet.

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

Each new game (X on the title screen) lays out a new complex. You start in
the car of shaft A - the top left corner - at its first floor with a room.

Each room is one of the four layouts in `src/rooms_wide.h` (FOUR DECKS,
CENTRE LIFT, TWIN TOWERS, THE CHASM), picked at random, each with four levels
(the floor and three above it).

## Equipment and the score

Every room has 4 or 5 pieces of equipment to search (cabinet, safe, plant,
radar, control panel): 128 in the whole complex. 36 of them, anywhere, have
something in them; the other 92 have nothing.

- Searching a piece (hold Square) scores **50**, the first time.
- Finding something in it shows **FOUND OBJECT** and scores **200** more.
- Searched pieces get a tick: green for nothing, gold for a find. On the map
  each room you've been in shows its pieces the same way (grey: not searched).

`sim/check_layout.cpp` lays out 2000 complexes and checks the counts (128
pieces, 4 or 5 a room, 36 finds, 2-6 rooms a column) and that no two pieces
overlap. `sim/check_rooms.cpp` checks that every
layout can be crossed with every door combination and every spot reached:
run it after adding or changing a layout.

## Controls

| DualSense | |
| --- | --- |
| X (Cross), on the title | start a new game |
| D-pad or left stick | run left / right; up / down rides the lift car or a lift pad, or calls a pad to you |
| Cross | somersault |
| Square or Circle | hold to search equipment |
| Options or touchpad | the map (shows every room and where its doors lead) |
| Triangle, on the map | lay out a new complex |

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
mission2/build.sh sim                     # plays sim/tour.txt, screenshots in build/mission2-sim/shots
g++ -O2 -Imission2/src mission2/sim/check_rooms.cpp -o check && ./check   # checks the room layouts
mission2/build.sh sim my_script.txt       # your own script (see sim/sim_main.cpp for the commands)
```

It runs the same game code with a scripted controller, so a change can be
checked on the PC before it goes on the console.

## What's in here

| | |
| --- | --- |
| `src/game.cpp` | the game: the complex, the shafts, the rooms, the agent, searching, the map (settings at the top: columns, rows, rooms, rooms per column) |
| `src/rooms_wide.h` | the room layouts (1280 units wide, four levels) |
| `src/gfx.cpp`, `gfx.h` | drawing: the 1920x1080 picture, unit-space calls as in the sketch, vector letters |
| `src/ps5_platform.cpp` | the PS5 side: the TV picture, the DualSense, the loop |
| `src/agent_rig.h`, `people.h` | NUCLEAR MISSION's agent and people, unchanged |
| `sim/` | the PC test build, the room layout checker and the icon maker (not compiled into the app) |
| `sce_sys/` | `param.json` (PPSA99903) and the home-screen icon |
| `assets/` | files to copy into the app (none yet) |
| `docs/PS5_NOTES.md` | what we learned getting NUCLEAR MISSION running on a real PS5 |
