// Lays out 2000 complexes and checks each: 128 pieces of equipment, 4 or 5 in
// every room, 36 with something in them, 2 to 6 rooms in every column, a room
// in the top left corner, 5 first aid kits in different rooms, every room's
// pieces placed and none overlapping.
//
//   gcc -O2 -c ../src/micromod.c && g++ -std=c++20 -O2 -w -I../src check_layout.cpp ../src/gfx.cpp micromod.o -o check_layout && ./check_layout
#include "../src/game.cpp"

int main() {
  gfxInit();
  int bad = 0;
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
      if (n < colMin) colMin = n;
      if (n > colMax) colMax = n;
    }
    if (eq != EQUIPMENT_TOTAL || hid != ITEMS_HIDDEN || colMin < MIN_PER_COL || colMax > MAX_PER_COL || roomAt[0][0] != 0 || kits != FIRST_AID_KITS) {
      bad++;
      printf("seed %u: equipment %d, finds %d, rooms a column %d..%d, kits %d\n", seed, eq, hid, colMin, colMax, kits);
    }
    for (int i = 0; i < ROOM_COUNT; i++) {
      placeDecor(rooms[i], wideRooms[rooms[i].tpl]);
      if (decorCount != rooms[i].decorN + (rooms[i].kit ? 1 : 0)) {
        bad++;
        printf("seed %u room %d: %d of %d pieces placed\n", seed, i + 1, decorCount, rooms[i].decorN);
      }
      for (int a = 0; a < decorCount; a++)
        for (int b = a + 1; b < decorCount; b++)
          if (decor[a].y == decor[b].y && abs(decor[a].x - decor[b].x) < (decorW[decor[a].kind] + decorW[decor[b].kind]) / 2 + 10) {
            bad++;
            printf("seed %u room %d: pieces %d and %d overlap\n", seed, i + 1, a, b);
          }
    }
  }
  printf("2000 layouts: %s\n", bad ? "PROBLEMS (above)" : "OK - 128 pieces (4 or 5 a room), 36 finds, 5 first aid kits, 2-6 rooms a column, no overlaps");
  return bad != 0;
}
