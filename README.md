# QUANTUM LOCKDOWN for the PS5

A new full-screen game in the style of Impossible Mission, built from
NUCLEAR MISSION's parts. Version 057: a title screen, the lift shafts, four
room designs (four levels each), a new layout of the complex every game,
equipment to search, the score, health, first aid kits, a 15 minute
countdown, the lift area's panel with its mini map, music and sound, things
on the walls (warning lights, fans, coolant tubes...), and room hazards:
electrified floors, dark rooms, security cameras and drones; 12 QUANTUM
DRIVES to find, 3 power generators, QUANTUM CONTROL with its drive bay and
main computer, the sub basement it unlocks and the QUANTUMLOOP to stage 2,
stage 2's QUANTUM LAB (six lifts, 21 rooms, card readers, the kitchen and
its chef and a knife, the cleaning room and its two cleaners, the medical
bay, the security room and its armed guards, people who get suspicious,
the maintenance man to knock out, search and hide in a bin), and a top 5 high
score table.

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

Each room is one of the ten layouts in `src/rooms_wide.h`, picked at random,
each with four levels (the floor and three above it):

- **FOUR DECKS**, **CENTRE LIFT**, **TWIN TOWERS**, **THE CHASM** (the first four)
- **THE STEPS**: ledges stepping down from each top corner into the middle; a
  lift beside each top ledge to get back up
- **THE SHAFT**: one lift up the middle stopping at every level, long shelves
  either side (drop off their ends to the floor)
- **THE BRIDGE**: a walkway across the top over a wide pit in the floor, an
  island under it, a lift at each edge of the pit
- **THE MAZE**: small ledges at every height, gaps to somersault, drops, a
  small pit, two lifts
- **TWIN PITS**: two pits in the floor; a lift in the middle up to ledges over
  them, a lift each side to the top
- **THE GALLERY**: a big open floor, a balcony right along the top reached by
  a lift at each end, two perches half way up

The facility terminal (it's tall) goes on a spot with nothing low over it
where the room has one.

## Equipment and the score

Every room has 4 or 5 pieces of equipment to search, 128 in the whole complex,
each a different kind in its room, from twelve: from NUCLEAR MISSION a filing
cabinet, safe, plant, radar, control panel, wooden desk, water cooler,
bookshelf and coat stand; new, a desk with a computer (its screen scrolling),
a steel cupboard and a computer rack (its lamps winking, tapes turning).

Each column of rooms has its own wall colour (teal, maroon, olive, navy,
purple, rust, forest green, mustard, plum - `wallColours` in `src/game.cpp`). 12 of them, each in a different room, have
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
  ridden them, your place blinking. On its left: the nuclear launch sites
  (see below). On its right: rooms visited,
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

## The nuclear launch sites (being tuned)

The facility is hacking five nuclear launch sites - NORTH ATLANTIC, EASTERN
EUROPE, PACIFIC, CENTRAL ASIA, SOUTHERN REGION - all through the game (both
stages; the clock is play time, so it stops when the game is paused, on the
map, in the settings and during the cut scenes). Each goes through three
stages:

1. **HACKED**, one after another, each time picked at random each game: the
   first after 7 to 10 minutes, then each of the other four 2 to 4 minutes
   after the one before (all five hacked after 15 to 26 minutes).
2. **LAUNCHED**: only once all five are hacked, or QUANTUM LOCKDOWN is on.
   The hacked sites launch in turn: 5 minutes, then 4, 3, 2, 1 more.
3. **LANDED**: 10 to 15 minutes (at random) after its launch. **A city is
   hit** (NUCLEAR STRIKE ON ... flies out) and **its cut scene plays** in the
   square beside the game, like a launch's (NUCLEAR IMPACT over it, the game
   still going, shrunk, on the left): the city's skyline at night, the warhead
   whistling down, a white flash, the fireball, the shock wave flattening the
   buildings into burning ruins, the mushroom cloud rising (its ring round the
   stem), the sky lit red, then the city's name typed on and its ESTIMATED
   DEATHS counting up (and the WORLD TOTAL after more than one). The city is one
   of these, a different one each time, its death toll at random between:
   Sydney, Australia 100,000-500,000; Dhaka, Bangladesh 2.5-5 million; Tokyo,
   Japan 2-3 million; New York, America 600,000-700,000; Shanghai, China
   2.9-4 million; Cairo, Egypt 2.5-5 million; Manila, Philippines 2.5-5 million;
   Seoul, South Korea 1.5-2.5 million. (Launches and landings that happen while
   one is playing wait their turn; the game ends after the last one's scene.)

When every launched missile has landed: MISSION FAILED - THE NUCLEAR MISSILES
HAVE LANDED. Each change is announced without stopping the game: red words
(NORTH ATLANTIC HACKED, ... LAUNCHED, ... IMPACT) come out of the middle of
the screen, small and faint, and fly at you, growing huge as they fade
away (2.6 seconds), while you carry on playing. A hack sounds like a heavy
digital clock ticking (as on the TV show 24), dying away over about 4
seconds; a launch has the ticking and the alarm; an impact the power
failing. The screen says how
many missiles landed and how many died in all (e.g. 3 NUCLEAR MISSILES LANDED -
8,412,903 DEAD).

**A launch** plays NUCLEAR MISSION's launch cut scene and sounds (the
klaxon, ignition, the roar, the typing) while the game carries on: the game
shrinks to 5/8 of its size on the left (still played as normal, the top bar
left full size) and the scene plays in a square on the right under NUCLEAR
LAUNCH. Inside the silo the launching site's name is on a yellow-and-black
sign on the wall; above ground it's on a sign by the fence; at the end
MISSILE LAUNCHED and the site's name type on. About 15 seconds; another
launch in the meantime waits its turn. (The map and settings show full size
over it, and pause it with the game.)

With nothing stopped, the timeline runs: hacked at 10, 16, 21, 25, 28
minutes; launched at 33, 37, 40, 42, 43; all landed by about 53 to 58.

**QUANTUM LOCKDOWN** is a new option on the main computer in QUANTUM CONTROL
(the 7th): it stops the hacking - the sites hacked so far are final, the rest
are SAFE - and the hacked ones start their launch countdown.

**The panel's left box** (where the lift and floor used to be) shows the five
sites: a wide box each under HACKED (yellow), LAUNCH (orange) and LANDED
(red), filling from the left as that stage goes on (half way through a hack:
half full) and full once it's done. There are no countdowns - you have to
learn how long things take. A site that can't be
hacked any more after the lockdown is dimmed, its boxes dark green; LOCKDOWN
shows at the top once it's on.

## The facility terminal (the blueprint)

One room (at random, never room 1, and never a room with a quantum drive or
a power generator) has the **FACILITY TERMINAL** (FT-7): a
tall grey console standing on its own, a green screen saying FACILITY /
LOGIN with a blinking cursor, a keyboard shelf and a blue beacon turning on
top - nothing else looks like it. Search it (hold Square) to log in: the
screen fills the view (like the main computer's), the login is typed in
(MAINTENANCE, a password, ACCESS GRANTED; X skips it), then a menu - UP /
DOWN and X:

1. **DOWNLOAD FACILITY BLUEPRINT**: the files come down one by one with a
   progress bar (about 3 seconds, 500 points), then BLUEPRINT DOWNLOADED -
   POWER GENERATOR LOCATIONS ARE NOW SHOWN ON YOUR MAP.
2. **QUANTUM DRIVE SCAN**: needs all 3 power generators on - otherwise
   ERROR: NOT ENOUGH POWER (how many are online, TURN ON ALL THE POWER
   GENERATORS). With the power, it scans the complex room by room, a room
   every half second (15 seconds in all): the last rooms scanned listed
   (CLEAR or DRIVE DETECTED), a TIME REMAINING countdown, a radar sweep with
   a blip for each find, a progress bar; a beep for each room, a ping for a
   find. Then SCAN COMPLETE - n ROOMS WITH QUANTUM DRIVES TO FIND ARE NOW
   SHOWN ON YOUR MAP. It can be run again (the clock keeps running while it
   scans).
3. **FACILITY STATUS**: generators online, time to the power failure, drives
   missing, where the main computer is.
4. **MAINTENANCE LOG**: what happened (the generators tripped, the drives
   pulled from the bay...).
5. **LOG OFF**. The first time you log off after downloading the blueprint,
   and the first time after a drive scan, the Options map opens by itself.

With the blueprint, the **Options map** shows the three generators' rooms
in **yellow** (green once that generator is on), with their generator icon,
whether you've been in them or not, and a line at the bottom saying so. The
lift area's **mini map** shows them as orange boxes with a G (green once
on). The terminal can be logged into again any time.

After the drive scan, the rooms that still have a quantum drive to find show
in **blue** on the Options map (with a drive icon; a yellow strip as well if
it's a generator's room too) and as blue boxes with a Q on the mini map. A
room drops off as soon as its drive is found.

Once you've been in the terminal's room, it shows in **purple** on the
Options map (with a little terminal icon) and as a purple box with a T on the
mini map, so you can find your way back to it.

## QUANTUM CONTROL

A room of its own in the middle column of the map (between lifts E and F), on
a random row, one level high, with a door at the bottom on each side. Its
equipment is NUCLEAR MISSION's LCC, with the new **QUANTUM DRIVE BAY** (12
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
    computer). It boots, checks the drive bay, and with fewer than 12 drives
    loaded stops at **UNABLE TO BOOT** (X logs off).
  - With all 12 loaded it boots to a menu: UP / DOWN to choose, X or Square to
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
   for stage 2, and stage 2's countdown starts again at 15:00.
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
stay open. Every room in a column needs the same level: lift A's rooms
level 1, B's level 2, C's 3, D's 4, E's 5 - and F's 5 too (there's no
level 6 card).

**Access cards**: finding one puts up **ACCESS CARD LEVEL n FOUND** with a
picture of the card, for a few seconds (the maintenance man's level 1 card
now; the same for any others later).

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

1. **Square**: search him - his **level 1 access card** (ACCESS CARD LEVEL 1
   FOUND) and his overalls: you're disguised as maintenance (blue overalls,
   hi-vis band, cap).
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
away.

**The cleaning room**: three levels, reached by the lift pad on the left.
The floor: the front counter (CLEANING SERVICES on its front, a bell, a
notice board behind), two steel shelf units of cleaning products (spray
bottles, bleach, toilet rolls, detergent), buckets and the cleaning trolley.
The middle: two tall cupboards, a desk with its computer and chair, a filing
cabinet, boxes. The top: crates and boxes, and mops and brooms hanging on the
wall. **Two cleaners** (teal tunics, mops) walk their paths: one on the floor,
one on the middle level - both walking away from you as you come in.

**The medical bay**: two levels (lift pad on the left). Downstairs: visitors'
seats, the NURSE STATION (its front is drawn over the nurse, who walks a short
path behind it), and behind it a bed with an unconscious patient, a
ventilator and a heart monitor. Upstairs, the ward: four beds, each with an
unconscious patient under a blanket, a heart monitor (its trace sweeping) and
a drip, and the medicine cabinet. The doctor (white coat) walks the ward.
One of the five patients, at random, has the **level 2 access card** (search
him: ACCESS CARD LEVEL 2 FOUND).

**The store room**: three levels (the lift at the back, on the right), packed
with variety - tall cupboards, grey double-door cabinets, lockers, racks of
boxes, tins, sacks and jars, crates, a shrink-wrapped pallet, a wooden
cupboard, stacks of boxes. Nobody in it: 16 things to search.

**Lift B's rooms** (access level 2): its five rooms, at random levels, are the
clone lab, the super soldier lab, the server room, QUANTUM LAB 1 and QUANTUM
LAB 2. Everything in them can be searched.

**16 QUANTUM PASSWORDS** (500 points each) are hidden in lift A's, lift B's
and lift C's rooms (never in the knife's, the pistol's or the level 2 card's
place, the armories' gun cases, the level 4 card's place, nor in the server
room's terminals). Each one shows the QUANTUM PASSWORD FOUND panel
with its access code (e.g. QX7-R41K) and n OF 16 FOUND; the panel's PASSWORDS
line counts them. **A server room terminal takes 4 of them to log in.**

**Quantum Lab 1**: NUCLEAR MISSION's lab equipment, two levels (a lift pad in the
middle up to a deck on the right). On the floor: a fridge of samples, mouse
cages, a bench of test tubes, an old computer terminal (a beige monitor,
Q-TERM LOGIN), and under the deck the glove box (lit inside), a microscope and
a centrifuge. On the deck: the silver fridge, an incubator, an isolation box,
a nitrogen tank and a filing cabinet. **Three scientists**: one working at the
glove box, one walking the floor, one walking the deck - they get suspicious
like everyone else. No hazards: 12 things to search.

**Quantum Lab 2**: the same kind of lab after an accident. **Two holes in the
floor** (taped off, jump them: fall in and it's FELL DOWN A PIT!, 30 seconds
and health), a ceiling panel hanging down, two loose cables **sparking** (with a
crackle), the glove box's window smashed and sparking, the terminal's screen
cracked and sparking, soot and cracks on the walls, a DANGER - UNSAFE FLOOR
sign, a light hanging off one wire, the lights stuttering now and then. Nobody
in it, no hazards: 7 things to search.

**The clone lab** (EXPERIMENTAL CLONE LAB): NUCLEAR MISSION's, wider. Two
levels, a lift pad by the door up to the deck. Up on the deck are **five
cloning tanks**, each with a suited clone floating in green fluid with bubbles
rising (SUBJECT 01..05). Below are the test tube bench, the DNA and body-scan
monitors, the green machine feeding the tanks, three gas cylinders, a gurney
with a clone under a sheet (a tag on his toe) and the orange robot arm. 12
things to search.

**Its hazard: hot gas, on the bottom level only.** A HOT GAS duct runs along
under the deck with five nozzles. They blast hot gas down to the floor **two at
a time, in a sequence of three you can learn**: vents 1 and 3, then 2 and 4,
then 3 and 5 (counting from the left), then a short pause and round again.
Before each blast the two nozzles' red lamps flash and wisps drip out (0.9
seconds), then the gas roars down (1.3 seconds). Caught in it (SCALDED BY HOT
GAS): a full health bar drops to under half - a second hit and he's dead. The
deck is safe.

**The super soldier lab**: one level, under **two security cameras** (as in
NUCLEAR MISSION). The big glowing tank in the middle holds SUBJECT X. Around it
are the two side tanks (DNA, a molecule), the two consoles with VITALS screens
above them (heart traces), the desk with its body-scan monitor, and the
syringe cabinet. For the wider room there is also a SERUM fridge of vials and a
stand of body armour with a helmet. 9 things to search.

**The server room**: one level, with **a camera and a drone** together (hard).
The walls are shiny silver metal tiles. A row of seven cooling fans (turning)
runs along the top, with the big QUANTUM NETWORK screen below them (a wire
globe, bars, a scan line). On the floor are four computer terminals (NODE 1..4)
and two metal cupboards either side. The 4 cupboards can be searched.

**The server room's terminals (NODE 1..4) stop launches.** Stand at one and hold
Square to log in (NODE n - HOLD SQUARE: LOG IN). **The camera and the drone wait
while you're logged in** (the screen says HAZARDS DISABLED WHILE LOGGED IN), but
**you can't log in while the drone's light is red** - it's firing at you (UNDER
FIRE - CAN'T LOG IN). Logging in types in your next 4 unused QUANTUM PASSWORDS;
then pick a hacked site that hasn't launched yet (PREVENT LAUNCH: PACIFIC...),
the abort code goes (3 seconds) and that site never launches: LAUNCH PREVENTED,
2000 points, the site's name flying out in green when you log off, STOPPED in
its LAUNCH box on the panel, and the next site's launch countdown starts. Each
terminal and each password works once: that terminal then shows LOCKED. With
fewer than 4 unused passwords it's ACCESS DENIED; with nothing to stop (no site hacked, or all
launched or stopped) it's NO LAUNCHES TO STOP and the password isn't used;
logging off from the list doesn't use it either. If the site launches while the
abort is going, it's TOO LATE (the password isn't used). The clock and the
sites keep going while you're logged in. Four terminals, 16 passwords: four
launches can be stopped, so with all five sites hacked one always gets through.
The game ends when every site that launched has landed.

**Quantum Lab 2's hazard: hot gas up out of its two holes**, one at a time in a
sequence of three: the left hole, the left again, then the right (then a pause
and round again). Before each blast the hole glows and wisps rise (0.9
seconds); then a column of gas roars up to the ceiling (1.3 seconds). Caught in
it - jumping across, or right at the hole - is the same as the clone lab's gas:
a full bar to under half, twice and he's dead.

**Lift C's foyer guards**: on the walkway left of lift C, at each of its 4
levels (where you come in from lift B's rooms), a soldier stands guard
(NUCLEAR MISSION's: camouflage, helmet, rifle). Like NUCLEAR MISSION's
soldiers, once you're on his walkway he turns to you, waits half a second,
aims (0.55 seconds) and fires a bullet you can see, then again every 1 to 1.8
seconds: each hit is 10 health (5 with the vest). Jump and it misses. Kill
him with the knife, the pistol or the QX-9. **One of the four has the ACCESS
CARD LEVEL 3**: search his body (Square) - the others have nothing on them.

**Lift D's foyer guards**: two soldiers on the walkway left of lift D at each
of its 3 levels (where you come in from lift C's rooms), firing the same way
(the second a moment after the first). None of them has a card.

**Lift C's rooms** (access level 3): three of its four, at random:

**EXPERIMENTAL ARMORY 2** (the fourth): like the first, in red light - under
the glass the **FX-5**, a flamethrower (red fuel tank, a blue pilot light), and
two scientists walking. Search the case (EXPERIMENTAL FLAMETHROWER FOUND): it
replaces whatever you carry - knife, pistol or QX-9 - and nothing replaces it.
**Hold R1 / R2** (or Square) and flames pour out (unlimited fuel, for now; a
roaring whoosh). Anyone they reach catches fire: burning on his feet, falling
charred black, crumbling to ash - and then nothing left (a kill, as any).

**HAZARDOUS STORAGE** (one level, no people): drums of green waste along a
shelf high on the wall, three of them split and leaking - **a drip from each in
turn, left, middle, right**, then a short pause - and **a puddle of it on the
floor** in the middle to jump over. A drip that lands on you costs 26% of a full
health bar (TOXIC DRIP!); stepping in the puddle 51% (BURNT BY TOXIC WASTE!,
and back out the way you came). **The fumes eat at you steadily just for
being in there**: 2 health a second (a full bar in 25 seconds), more with
any drip or puddle (TOXIC FUMES - GET OUT AND FIND FIRST AID at a quarter). So get out, use a
walkway's first aid, and come back. **6 things to search** (a hazmat locker, a
chemical cabinet, barrels on a pallet, a lead container, a spill kit cart, a
decontamination shower), and **one of them has the ACCESS CARD LEVEL 4**
(lift D).

**The EXPERIMENTAL ARMORY**: dark gunmetal walls; in the middle, on a plinth
under glass and a blue spotlight, the prototype **QX-9** turning slowly. Around
it: a rack of prototypes, a workbench (a vice, parts, a laptop), an energy cell
charger, a block of ballistic gel, ammo crates and a firing booth with a
target. Two scientists walk either side of the case (they get suspicious like
anyone). **Search the case and the QX-9 is yours** (EXPERIMENTAL GUN FOUND): it
replaces your knife or pistol (one weapon at a time; you won't swap it for
either again). Fire it like the pistol (R1 / R2, or Square with someone in
line): a blue bolt. **Each hit halves someone's size** - shot twice they're a
quarter size and gone (a kill: +5:00, as any) - but **1 shot in 10 doubles them
instead** (up to 4 times the size). It works on the door guard's vest too.

**The TEST CHAMBER**: a basic room to try the QX-9 out in - a TEST CHAMBER sign,
an observation window, two height charts on the wall, a bench, a laundry bin
of jumpsuits, the scales, a gurney, a trolley of syringes and sample jars, a
water cooler and a locker (7 things to search) and **three test subjects** in orange jumpsuits
walking about.

**Lift D's rooms: SECURITY ZONES, seen from above** (all three, the same
room). A different kind of play: the room from above, you walking any of 8
ways (the D-pad / stick), from the IN door at the bottom left to the EXIT at the
top right (out to lift E's walkway; or back the way you came). Desks, crates,
cabinets, a sofa, pillars and a table are in the way - cover for you and them;
shots and flames stop at them. Three soldiers hold the room; when you've got
them down to one, **three more come in, in a V, through one of two doors** -
the top one or the bottom one, at random (MORE SOLDIERS - THE TOP DOOR! / THE
BOTTOM DOOR!) - and take up a spot; you can't go through either door yourself - **15 in all**, then no more (SOLDIERS LEFT
counts down; ALL 15 DOWN - THE ROOM IS CLEAR). Each one that can see you aims
(a red sight line, 0.55 seconds, NUCLEAR MISSION's), fires, and again every 1 to
1.8 seconds, stepping aside now and then; a hit is 10 health (5 with the vest).
You fire the way you face with R1 / R2 or Square: the pistol, the QX-9 (it
halves them, 1 in 10 doubles them), or the FX-5's flames (they burn to ash and
disappear); X with the knife up close (a soldier beside you comes first). Each is 250 points (no extra time here).
Leave and come back and the ones you've killed stay dead; the rest are back at
their posts. The bottom room's top right door is SEALED (no lift beyond it).

All 11 pieces of furniture in each room can be searched: stand beside one
(X: SEARCH) and hold X - in these rooms X searches, not Square (a progress bar; a green tick when it's done).
**One of the three rooms has the ACCESS CARD LEVEL 5** in one of them.

Each game, the three rooms are dealt one extra hazard each:
- **A drone** (one room): it flies a loop of the room, its scanning beam
  sweeping round. When the beam finds you (THE DRONE HAS SEEN YOU) it turns red,
  heads for you (a little slower than you), keeps close and fires every 0.8 seconds (5 health a hit). It
  can't be hurt. Desks, crates and the sofa are below it; only a pillar or a
  cabinet hides you. Lose it for 2.5 seconds and it goes back to its round.
- **A wall turret** (one room): on the top wall, its launcher following you.
  Every 10 seconds (its light flashes for the last one, and a warning sound)
  it fires a **homing missile** (MISSILE - RUN!). The missile turns after you
  but is a little slower than you, and is spent after 4 seconds (IT RAN OUT):
  outrun it. It hits for 15, or blows up on a wall, a pillar or a cabinet
  (15 if you're close); it kills any soldier it hits.
- **Nothing extra** (the third room): just the soldiers.

**The admin room**: two levels, most of it on the floor - the ADMINISTRATION
front counter by the door (ENQUIRIES board, an in-tray, a bell), a desk with
someone sitting at it (white shirt and tie, typing, facing the counter; the
desk's front hides his legs), a meeting table with chairs, a filing cabinet,
a water cooler, a plant, a clock and pictures; at the back a lift up to a
deck of archive shelves, a cupboard and boxes. A second office worker walks
the floor. 8 things to search.

**The security room**: one level. A wall of 24 CCTV screens high up (people
walking corridors, scan lines, recording lamps), two gun racks of rifles,
lockers, the SECURITY desk (NUCLEAR MISSION's GR1 style, its front drawn over
the guard behind it), a filing cabinet and seats. **Three guards** with
pistols: one standing **just inside the door you came in by**, facing into
the room (he sees the whole room, so the gun racks can't be searched under
his nose, and a bullet won't stop him - he wears a vest: only the knife),
one at the desk (turning to look one way, then the other, every few seconds)
and one walking the room. Come in behind the door guard and knife him: the
other two put up a red ! and open fire **7 seconds** later (back in the room
after knifing him, they're waiting: 7 seconds again). One of the two gun
racks (at random) has a **pistol** (PISTOL FOUND). As in NUCLEAR MISSION
you see it in his hand and see the bullets fly: **R1 or R2** fires, and so
does **Square** whenever someone alive is in line on your level the way you
face (otherwise Square still searches); hold it to keep firing. The first
person a bullet reaches goes down (500 points); the door guard's vest stops
it.

**In lift A's six rooms** (at random, never in the knife's, the pistol's or
the level 2 card's place) is a **BULLET-PROOF VEST** (500 points; he wears
it: a guard's shot then only does half damage and he stays on his feet - the
guard keeps firing every half second - until he's nearly out of health, when
the next shot knocks him down as usual) and, across lift A's and lift B's rooms, **15 TOP SECRET DOCUMENTS**
(1000 points each, the TOP SECRET DOCUMENTS panel with n OF 15 FOUND).

**Low health warnings** (both stages): when your health falls to 40%, 30%, 20%
and 10%, a red "40% HEALTH" (and so on) flies out at you from the middle of
the screen like a site's HACKED, with two falling warning tones - once for each
mark until first aid takes you back above it. From 40% down a red glow pulses
around you with your heartbeat, quicker and stronger the lower you get; from
20% down you hear the heartbeat and the edges of the screen pulse red.

**Gunfire brings a drone**: in a room with no hazard of its own (the kitchen,
cleaning room, medical bay, security room, admin room, Quantum Lab 1), once
all its people are dead and at least one of them was shot, a security drone
comes down from the ceiling on the far side (GUNFIRE HEARD - SECURITY DRONE
DISPATCHED), hovers for 5 seconds, then scans for him like any drone. It stays
in that room. Kill them all with the knife and no drone comes.

**Each person he kills** (knife or pistol) puts **5 minutes** back on the
clock (+5:00 ON THE CLOCK).

**One weapon at a time**: he keeps the stronger. Finding the pistol drops the
knife (KNIFE DROPPED - ONE WEAPON AT A TIME); finding the knife with the
pistol leaves the knife (A KNIFE - YOU KEEP YOUR PISTOL). The panel's GEAR
line shows his weapon and the vest. Guards get suspicious twice as fast (a
point every 0.35 seconds) and at one point short of five they draw and
**shoot**: 10 health, 30 seconds, knocked down - you come round in the lift
area outside the door you came in by.

**The knife**: in one of the kitchen's 8 things, at random (KNIFE FOUND).
With it, X close to someone in a lab room (on your level, within 80 units;
the prompt says X: KNIFE) kills them: they fall and stay down where they fell,
can't see you any more, and are still there when you come back. 500 points.
(See one weapon at a time, below.)

**Searching**: everything in the kitchen (8 things), the cleaning room (16),
the medical bay (9, the patients included) and the security room (6) can be
searched like stage 1's equipment - hold Square; 100 points the first time.
Apart from the knife and the level 2 card, there's nothing in them yet
(NOTHING).

**Suspicion** (as in NUCLEAR MISSION): someone who is facing you, on your
level and within 330 units gets suspicious if you're looking at them, or if
they see you searching. They stop where they are and stare, with a **?** over
their head going redder, and the **SUSPICION** meter at the top left (five
pips, in any room with people in it) fills: a point every 0.7 seconds (the
guards 0.35), twice as fast while you search. Look away (face the other way) and stop
searching, and it goes back to nothing and they carry on. Five points and
you're **RECOGNISED**: hands up, 30 seconds off the clock, and you're put
out in the lift area by the door you came in by. (The disguise doesn't
change this, for now.)

**Lift E's top room: the CODE ROOM.** Two soldiers, **twice the size**,
stand guard (NUCLEAR MISSION's soldier, as in the foyers: they see you, aim,
fire - 10 a hit, 5 with the vest; jump the shots). Three quarters of the wall
is an **8 x 4 board of squares**, with a terminal under its middle. Hold Square
at the terminal to play Impossible Mission's sequence: squares light up one
after another, **each with its own note** - 3 in the first round, then 4,
then 5 - and you repeat each exactly (the D-pad moves the cursor, X picks a
square, Square walks away). Get one wrong and you're shown a new sequence of
the same length. All three rounds: a **NUKE SELF DESTRUCT CODE** (+1000). You
can hold only one at a time (NUKE CODE HELD shows top right). On the right of
the room, the **missiles' display**: a turning hologram of the earth (the
cities hit glowing red), each site's missile - NOT LAUNCHED, IN FLIGHT (a bar
filling as it nears landing, flashing red near the end), LANDED, STOPPED,
DESTROYED - and how many have landed and the dead.

**Lift E's lower room: SELF DESTRUCT.** Two more double-sized soldiers. The
**self destruct console**: with a code, hold Square at it and pick one of the
missiles in the air (each with how near it is to landing); the code destroys
it (+2000, its news flies out in green) and is used up - go back up for
another. Without a code: ACCESS DENIED. The console, filing cabinets, a
cupboard and a desk: **the ACCESS CARD LEVEL 5** is in one of the four.
When the last missile is dealt with and at least one was destroyed:
**CONGRATULATIONS - YOU HAVE STOPPED ALL THE NUCLEAR MISSILES** (none
landed) or **YOU HAVE DONE THE BEST YOU COULD** (some did), and you carry on.
(If none were destroyed and they all land, it's still MISSION FAILED.)

**Lift F: DR. VOSS** (the Professor) - the end. His office (DR. VOSS on the sign) (a world map screen,
bookshelves, his desk, a globe) and him: a black coat, wild white hair,
glasses, twice the size, walking back and forth, no weapon for now. Kill him
(DR. VOSS IS DEAD) and the game ends: MISSION COMPLETE, with how many
missiles landed and the dead, and the end of game bonus.

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
has the initials, the score, the time played and the **DEATHS**: the nuclear
toll of that game (see below; the initials screen shows it too). The title screen's panel
takes turns, 15 seconds each: MISSION OBJECTIVES, then TOP 5 AGENTS. On the PS5 the table is kept in
`/download0/quantum-lockdown/hiscores.bin` (the title's own storage; a table
from before version 054 is kept, its deaths 0). The settings' RESET HIGH
SCORES puts the table back to its five made-up agents.

**The death toll.** At the start of each game, each of the 5 nuclear sites is
given its city and its toll (from the cities' ranges). However the game ends
- time, health, the body found, the missiles, or the Professor dead - every
missile that wasn't stopped (a server room terminal) or destroyed in flight
(the self destruct code) counts as landed: the ones that landed, the ones in
the air, the ones still to launch and, unless QUANTUM LOCKDOWN was set, the
sites not yet hacked. So running out of time in stage 1 is all 5. The end
screen shows "N NUCLEAR MISSILES LANDED - X DEAD" (or NO NUCLEAR MISSILES
LANDED) under the reason, and X goes in the table.

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
pieces, 4 or 5 a room, one facility terminal, 12 drives, 5 documents, 5 first aid kits, 2-6 rooms a column, the
hazard rooms), that every piece stands on its platform clear
of the lifts, that no two overlap, and that every electrified room has its sign. `sim/check_rooms.cpp` checks that every
layout can be crossed with every door combination and every spot reached:
run it after adding or changing a layout.

## The title screen, the briefing

The title sits at the top (title.mod playing), and now and then the name
glitches red and cyan. Behind it all, filling the screen (darkened, silent
under the tune), NUCLEAR MISSION's **missile launch** plays over and over (no site
names shown); after every 5 launches, the **nuclear landing** cut
scene (a random city), then the launches again. Under the name, on a dark
band, these take turns every 15 seconds:

- **MISSION OBJECTIVES:** 1. AVERT GLOBAL NUCLEAR ANNIHILATION (a radiation
  sign) 2. NEUTRALISE THE QUANTUM COMPUTER (a quantum drive) 3. BRING DR. VOSS
  TO JUSTICE (cross hairs)
- **TOP 5 AGENTS:** the high scores (with the deaths)

After X and the opening (out of the duct), the **STAGE 1** briefing: ABANDONED
FACILITY, COUNTDOWN TO POWER FAILURE 15:00, RESTORE 3 GENERATORS, with
pictures of the generators. The clock waits; X again to start playing.

## Settings

**Triangle** (any time in play, not on the map) opens the settings, and
pauses the game and the clock:

- **STAGE MUSIC** ON / OFF
- **ELEVATOR MUSIC** ON / OFF (both remembered: `settings.bin` beside the
  high scores). With the elevator music off, the stage music (if it's on)
  carries on playing in the lift car instead of going quiet.
- **JUMP TO STAGE 2** (for testing): straight to the sub basement's lift
  area at the end of stage 1 (unlocked), ready to go into the station, with
  all 5 nuclear sites hacked (so the launch countdowns start straight away)
- **ALL ACCESS (TEST)** ON / OFF (for testing stage 2): every card reader
  opens (the panel's ACCESS CARD says ALL)
- **RESET HIGH SCORES** (X twice): the table back to its defaults (saved)
- **QUIT TO TITLE** (X twice): drops the game there and then, no score
- **RESUME** (or Triangle again)

## Controls

| DualSense | |
| --- | --- |
| X (Cross), on the title | start a new game |
| D-pad or left stick | run left / right; up / down rides the lift car or a lift pad, or calls a pad to you |
| Cross | somersault (stage 2, close to the maintenance man: knock him out) |
| R1 / R2 | fire the pistol (stage 2, once found; Square too with someone in line) |
| Square or Circle | hold to search equipment (stage 2: search the maintenance man, put him in the bin, a first aid cabinet) |
| Options or touchpad | the map (shows every room and where its doors lead), and pauses the game |
| Triangle | the settings (pauses the game) |

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
