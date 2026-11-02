// Checks every room layout in rooms_wide.h: with each of the four door
// combinations (top / bottom on each side), coming in by either door, can he
// get out by the other door and reach every equipment spot?
//
//   g++ -O2 -I../src check_rooms.cpp -o check && ./check
//
// The moves, as the game has them (game.cpp):
//   walk     between surfaces at the same height that meet (each has an 8 unit grace)
//   jump     the somersault: about 195 units across, landing at the same height
//   drop     running off an edge: he drifts a little as he falls; more than 200 units knocks him out
//   lift     a pad goes between all its stops, and comes when called (UP / DOWN beside it)
// It errs on the careful side (jumps counted up to 175 units of gap).
#include "rooms_wide.h"
#include <math.h>
#include <stdio.h>
#include <vector>

struct Surf { float x0, x1, y; };
static const float GRACE = 8, JUMP_GAP = 175, SAFE = 200, W = 1280;

static bool covers(const Surf &s, float x) { return x >= s.x0 - GRACE && x <= s.x1 + GRACE; }

int main() {
  int bad = 0;
  for (int t = 0; t < WIDE_ROOM_COUNT; t++) {
    const WideRoom &r = wideRooms[t];
    // the surfaces: platforms, each lift at each of its stops, each lift's bottom (the shaft floor)
    std::vector<Surf> s;
    for (int i = 0; i < r.platCount; i++) s.push_back({ (float)r.plats[i].x0, (float)r.plats[i].x1, (float)r.plats[i].y });
    int liftBase = (int)s.size();
    for (int i = 0; i < r.liftCount; i++)
      for (int k = 0; k < r.lifts[i].n; k++) s.push_back({ (float)r.lifts[i].x0, (float)r.lifts[i].x1, (float)r.lifts[i].stops[k] });
    int n = (int)s.size();
    std::vector<std::vector<int>> next(n);
    // (a pad may be at another stop, so a fall lands only on platforms or a shaft's floor)
    std::vector<bool> padStop(n, false);
    for (int i = 0, base = liftBase; i < r.liftCount; base += r.lifts[i].n, i++)
      for (int k = 0; k + 1 < r.lifts[i].n; k++) padStop[base + k] = true;
    auto lowestUnder = [&](float x, float y) {   // the first surface below y covering x, or -1
      int best = -1;
      for (int j = 0; j < n; j++)
        if (!padStop[j] && s[j].y > y && covers(s[j], x) && (best < 0 || s[j].y < s[best].y)) best = j;
      return best;
    };
    for (int a = 0; a < n; a++) {
      for (int b = 0; b < n; b++) {
        if (a == b || s[a].y != s[b].y) continue;
        float gap = fmaxf(s[b].x0 - s[a].x1, s[a].x0 - s[b].x1);
        if (gap <= 2 * GRACE || gap <= JUMP_GAP) next[a].push_back(b);   // walk, or somersault
      }
      // drop off either end
      for (int dir = -1; dir <= 1; dir += 2) {
        float edge = dir > 0 ? s[a].x1 + GRACE + 1 : s[a].x0 - GRACE - 1;
        if (edge < 0 || edge > W) continue;
        int j = lowestUnder(edge, s[a].y);
        if (j < 0) continue;
        float h = s[j].y - s[a].y;
        float drift = 102.0f * sqrtf(2.0f * h / 1700.0f);
        float land = edge + dir * drift;
        int k = lowestUnder(land, s[a].y);
        if (k >= 0 && s[k].y - s[a].y <= SAFE && covers(s[k], land)) next[a].push_back(k);
      }
    }
    // a lift joins all its stops
    for (int i = 0, base = liftBase; i < r.liftCount; base += r.lifts[i].n, i++)
      for (int k = 0; k < r.lifts[i].n; k++)
        for (int m = 0; m < r.lifts[i].n; m++)
          if (k != m) next[base + k].push_back(base + m);
    auto doorSurf = [&](bool right, float y) {
      for (int j = 0; j < liftBase; j++)
        if (s[j].y == y && (right ? s[j].x1 >= W : s[j].x0 <= 0)) return j;
      return -1;
    };
    bool ok = true;
    for (int combo = 0; combo < 4; combo++) {
      float yl = (combo & 1) ? LEVEL_TOP : ROOM_FLOOR_Y, yr = (combo & 2) ? LEVEL_TOP : ROOM_FLOOR_Y;
      int dl = doorSurf(false, yl), dr = doorSurf(true, yr);
      if (dl < 0 || dr < 0) {
        printf("%s: no ledge for a door (left %s, right %s)\n", r.name, yl == LEVEL_TOP ? "top" : "bottom", yr == LEVEL_TOP ? "top" : "bottom");
        ok = false;
        continue;
      }
      for (int from = 0; from < 2; from++) {
        std::vector<bool> seen(n, false);
        std::vector<int> todo = { from ? dr : dl };
        seen[todo[0]] = true;
        while (!todo.empty()) {
          int a = todo.back();
          todo.pop_back();
          for (int b : next[a])
            if (!seen[b]) { seen[b] = true; todo.push_back(b); }
        }
        const char *tag = from ? "in by the right door" : "in by the left door";
        int other = from ? dl : dr;
        if (!seen[other]) {
          printf("%s: doors L-%s R-%s, %s: can't reach the other door\n", r.name, yl == LEVEL_TOP ? "top" : "bottom",
                 yr == LEVEL_TOP ? "top" : "bottom", tag);
          ok = false;
        }
        for (int k = 0; k < r.spotCount; k++) {
          bool reached = false;
          for (int j = 0; j < liftBase; j++)
            if (seen[j] && s[j].y == r.spots[k].y && r.spots[k].x >= s[j].x0 && r.spots[k].x <= s[j].x1) reached = true;
          if (!reached) {
            printf("%s: doors L-%s R-%s, %s: can't reach spot %d (%d, %d)\n", r.name, yl == LEVEL_TOP ? "top" : "bottom",
                   yr == LEVEL_TOP ? "top" : "bottom", tag, k, r.spots[k].x, r.spots[k].y);
            ok = false;
          }
        }
      }
    }
    // headroom: one level (160) between a platform and the one over it, and the ceiling
    for (int i = 0; i < r.platCount; i++)
      for (int j = 0; j < r.platCount; j++) {
        const Plat &p = r.plats[i], &q = r.plats[j];
        if (q.y < p.y && q.x1 > p.x0 && q.x0 < p.x1 && p.y - q.y < 160) {
          printf("%s: platform %d is under platform %d with less than one level between\n", r.name, i, j);
          ok = false;
        }
      }
    printf("%-14s %s\n", r.name, ok ? "OK: every door combination, every spot" : "PROBLEMS (above)");
    if (!ok) bad++;
  }
  return bad ? 1 : 0;
}
