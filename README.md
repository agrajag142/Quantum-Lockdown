# Mission 2 (working title) for the PS5

A new full-screen game in the style of Impossible Mission, built from
NUCLEAR MISSION's parts. Version 001: the lift shafts, one room design and
the layout of the complex. No sound yet.

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

30 rooms (6 across, 5 down) between 7 lift shafts, A to G. Every room has a
door on its left and on its right, each at the room's **top** or its
**bottom** (picked at random for each room, each game). A door opens onto the
lift shaft on that side, at the floor that matches it: each shaft has two
floors for each row of rooms (the top doors' floor, then the bottom doors'),
10 floors in all. You start in the car of shaft D, at floor 1.

Each room is one of the layouts in `src/rooms_wide.h` (one so far: TWIN LIFTS)
with three pieces of equipment to search, picked at random from five kinds.

## Controls

| DualSense | |
| --- | --- |
| D-pad or left stick | run left / right; up / down rides the lift car or a lift pad |
| Cross | somersault |
| Square or Circle | hold to search equipment |
| Options or touchpad | the map (shows every room and where its doors lead) |
| Triangle, on the map | lay out a new complex |

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
mission2/build.sh sim my_script.txt       # your own script (see sim/sim_main.cpp for the commands)
```

It runs the same game code with a scripted controller, so a change can be
checked on the PC before it goes on the console.

## What's in here

| | |
| --- | --- |
| `src/game.cpp` | the game: the complex, the shafts, the rooms, the agent, searching, the map |
| `src/rooms_wide.h` | the room layouts (1280 units wide) |
| `src/gfx.cpp`, `gfx.h` | drawing: the 1920x1080 picture, unit-space calls as in the sketch, vector letters |
| `src/ps5_platform.cpp` | the PS5 side: the TV picture, the DualSense, the loop |
| `src/agent_rig.h`, `people.h` | NUCLEAR MISSION's agent and people, unchanged |
| `sim/` | the PC test build and the icon maker (not compiled into the app) |
| `sce_sys/` | `param.json` (PPSA99903) and the home-screen icon |
| `assets/` | files to copy into the app (none yet) |
| `docs/PS5_NOTES.md` | what we learned getting NUCLEAR MISSION running on a real PS5 |
