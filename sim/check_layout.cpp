// Lays out 2000 complexes and checks each: 128 pieces of equipment, 4 or 5 in
// every room, 36 with something in them, 2 to 6 rooms in every column, a room
// in the top left corner, 5 first aid kits in different rooms, every room's
// pieces placed, standing on a platform clear of the lifts, none overlapping;
// the hazard rooms (6 electrified, 3 dark, 6 with a camera, 3 with two, 3
// with a drone; none in room 1), every
// electrified room with its HIGH VOLTAGE sign on the wall.
//
//   gcc -O2 -c ../src/micromod.c ../src/micromod2.c ../src/micromod3.c && g++ -std=c++20 -O2 -w -I../src check_layout.cpp ../src/gfx.cpp micromod.o micromod2.o micromod3.o -o check_layout && ./check_layout
#include "../src/game.cpp"

int main() {
  gfxInit();
  int bad = 0;
  long bigFans = 0, ornTotal = 0;
  for (uint32_t seed = 1; seed <= 2000; seed++) {
    layOut(seed);
    int eq = 0, hid = 0, colMin = 99, colMax = 0, kits = 0;
    for (int i = 0; i < ROOM_COUNT; i++) {
      eq += rooms[i].decorN;
      kits += rooms[i].kit;
      for (int k = 0; k < rooms[i].decorN; k++) hid += rooms[i].holds[k];
      if (rooms[i].decorN < 4 || rooms[i].decorN > 5) bad++;
    }
    for (int c = 0; c < COLS; c++) {
      int n = 0;
      for (int r = 0; r < ROWS; r++) n += roomAt[r][c] >= 0;
      if (c == CONTROL_COL) n -= 1;   // (QUANTUM CONTROL isn't one of the 30)
      if (n < colMin) colMin = n;
      if (n > colMax) colMax = n;
    }
    if (eq != EQUIPMENT_TOTAL || hid != ITEMS_HIDDEN || colMin < MIN_PER_COL || colMax > MAX_PER_COL || roomAt[0][0] != 0 || kits != FIRST_AID_KITS) {
      bad++;
      printf("seed %u: equipment %d, finds %d, rooms a column %d..%d, kits %d\n", seed, eq, hid, colMin, colMax, kits);
    }
    int gens = 0;
    for (int i = 0; i < ROOM_COUNT; i++) gens += rooms[i].gen;
    if (gens != GENERATORS || roomAt[rooms[CONTROL_ROOM].row][CONTROL_COL] != CONTROL_ROOM || rooms[CONTROL_ROOM].topL || rooms[CONTROL_ROOM].topR) {
      bad++;
      printf("seed %u: %d generators, QUANTUM CONTROL at row %d\n", seed, gens, rooms[CONTROL_ROOM].row);
    }
    // the lifts stop at QUANTUM CONTROL's two doors
    if (roomOff(CONTROL_COL, rooms[CONTROL_ROOM].row * 2 + 1, true) != CONTROL_ROOM || roomOff(CONTROL_COL + 1, rooms[CONTROL_ROOM].row * 2 + 1, false) != CONTROL_ROOM) {
      bad++;
      printf("seed %u: QUANTUM CONTROL's doors don't meet the lifts\n", seed);
    }
    int hz[6] = {};
    for (int i = 0; i < ROOM_COUNT; i++) hz[rooms[i].hazard]++;
    if (hz[HZ_ELECTRIC] != ELECTRIC_ROOMS || hz[HZ_DARK] != DARK_ROOMS || hz[HZ_CAMERA] != CAMERA_ROOMS ||
        hz[HZ_CAMERA2] != DUAL_CAMERA_ROOMS || hz[HZ_DRONE] != DRONE_ROOMS || rooms[0].hazard != HZ_NONE) {
      bad++;
      printf("seed %u: hazard counts %d %d %d %d %d, room 1 hazard %d\n", seed, hz[1], hz[2], hz[3], hz[4], hz[5], rooms[0].hazard);
    }
    for (int i = 0; i < ROOM_COUNT; i++) {
      buildRoom(i, true);
      int signs = 0;
      for (int k = 0; k < ornCount; k++) signs += orn[k].kind == ORN_VOLTAGE;
      if (signs != (rooms[i].hazard == HZ_ELECTRIC ? 1 : 0)) {
        bad++;
        printf("seed %u room %d: %d HIGH VOLTAGE signs\n", seed, i + 1, signs);
      }
      int shownOrn = 0;
      for (int k = 0; k < ornCount; k++) shownOrn += orn[k].kind != ORN_VOLTAGE;
      if (shownOrn > MAX_ORNAMENTS_SHOWN) {
        bad++;
        printf("seed %u room %d: %d decorations\n", seed, i + 1, shownOrn);
      }
      bigFans += ornCount > 0 && [&] { for (int k = 0; k < ornCount; k++) if (orn[k].kind == ORN_BIGFAN) return true; return false; }();
      ornTotal += ornCount;
      placeDecor(rooms[i], wideRooms[rooms[i].tpl]);
      if (decorCount != rooms[i].decorN + (rooms[i].kit ? 1 : 0) + (rooms[i].gen ? 1 : 0)) {
        bad++;
        printf("seed %u room %d: %d of %d pieces placed\n", seed, i + 1, decorCount, rooms[i].decorN);
      }
      // each piece stands on a platform, clear of the lift shafts
      const WideRoom &t = wideRooms[rooms[i].tpl];
      for (int a = 0; a < decorCount; a++) {
        int hw = decorW[decor[a].kind] / 2;
        bool on = false;
        for (int k = 0; k < t.platCount; k++)
          if (t.plats[k].y == decor[a].y && decor[a].x - hw >= t.plats[k].x0 - 4 && decor[a].x + hw <= t.plats[k].x1 + 4) on = true;
        for (int k = 0; k < t.liftCount; k++)
          if (decor[a].x + hw > t.lifts[k].x0 && decor[a].x - hw < t.lifts[k].x1) on = false;
        if (!on) {
          bad++;
          printf("%s: a piece %d wide at %d,%d hangs off its platform or over a lift\n", t.name, decorW[decor[a].kind], decor[a].x, decor[a].y);
        }
      }
      for (int a = 0; a < decorCount; a++)
        for (int b = a + 1; b < decorCount; b++)
          if (decor[a].y == decor[b].y && abs(decor[a].x - decor[b].x) < (decorW[decor[a].kind] + decorW[decor[b].kind]) / 2 + 10) {
            bad++;
            printf("seed %u room %d: pieces %d and %d overlap\n", seed, i + 1, a, b);
          }
    }
  }
  printf("wall ornaments: %.1f a room on average, a big fan in %.0f%% of rooms\n", ornTotal / 60000.0, bigFans / 600.0);
  printf("2000 layouts: %s\n", bad ? "PROBLEMS (above)" : "OK - 128 pieces (4 or 5 a room), 36 finds, 5 first aid kits, 2-6 rooms a column, no overlaps, 3 generators, QUANTUM CONTROL in the middle column, at most 2 decorations a room, 6 electrified, 3 dark, 6 one-camera, 3 two-camera, 3 drone rooms, signs up");
  return bad != 0;
}
