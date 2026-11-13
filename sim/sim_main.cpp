// The PC test build: the game with a scripted controller, saving screenshots.
//
//   gcc -O2 -c ../src/micromod.c ../src/micromod2.c ../src/micromod3.c ../src/micromod4.c ../src/micromod5.c ../src/micromod6.c && g++ -O2 -I../src ../src/game.cpp ../src/gfx.cpp sim_main.cpp micromod.o micromod2.o micromod3.o micromod4.o micromod5.o micromod6.o -o sim
//   ./sim script.txt out/ [elevator.mod [stage1.mod [title.mod [stage2.mod]]]]
//
// Script lines:  hold BUTTONS FRAMES   (BUTTONS: LEFT+JUMP etc, or NONE)
//                tap BUTTONS           (one frame down, one up)
//                shot NAME             (out/NAME.ppm)
//                seed N                (before anything else)
//                walkto X              run left or right until he is at x = X (units)
//                waitidle              until he stands still (a lift arrived, a landing)
//                room N DOORS [H]      straight into layout N (DOORS: TT, TB, BT or BB - left, right;
//                                      H: E an electrified floor, D a dark room, C a camera,
//                                      2 two cameras, R a drone)
//                control D G           into QUANTUM CONTROL with D drives in hand and G generators on
//                shaft S F             into lift shaft S (0 = A; -1 the sub basement's) at floor F (-1 its lowest)
//                gen N                 into the room with power generator N (0..2)
//                wav SECONDS NAME      the sound for the next SECONDS (the game running): out/NAME.wav
//                # comment
#include "game.h"
#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint32_t parseButtons(const char *s) {
  uint32_t b = 0;
  char tmp[128];
  snprintf(tmp, sizeof(tmp), "%s", s);
  for (char *t = strtok(tmp, "+"); t; t = strtok(nullptr, "+")) {
    if (!strcmp(t, "LEFT")) b |= BTN_LEFT;
    else if (!strcmp(t, "RIGHT")) b |= BTN_RIGHT;
    else if (!strcmp(t, "UP")) b |= BTN_UP;
    else if (!strcmp(t, "DOWN")) b |= BTN_DOWN;
    else if (!strcmp(t, "JUMP")) b |= BTN_JUMP;
    else if (!strcmp(t, "USE")) b |= BTN_USE;
    else if (!strcmp(t, "MAP")) b |= BTN_MAP;
    else if (!strcmp(t, "NEW")) b |= BTN_NEW;
    else if (!strcmp(t, "FIRE")) b |= BTN_FIRE;
  }
  return b;
}

static void shot(const char *dir, const char *name) {
  char path[256];
  snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", FB_W, FB_H);
  static unsigned char row[FB_W * 3];
  for (int y = 0; y < FB_H; y++) {
    for (int x = 0; x < FB_W; x++) {
      uint32_t p = g_frame[y * FB_W + x];
      row[x * 3] = p & 255;
      row[x * 3 + 1] = (p >> 8) & 255;
      row[x * 3 + 2] = (p >> 16) & 255;
    }
    fwrite(row, 1, sizeof(row), f);
  }
  fclose(f);
}

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: sim script.txt outdir [elevator.mod [stage1.mod [title.mod [stage2.mod [gameovergood.mod [gameoverbad.mod]]]]]]\n"); return 2; }
  for (int k = 0; k < 6 && argc > 3 + k; k++) {   // the tunes, as the PS5 build loads them: elevator, stage1, title, stage2, gameovergood, gameoverbad
    FILE *mf = fopen(argv[3 + k], "rb");
    if (!mf) continue;
    fseek(mf, 0, SEEK_END);
    long len = ftell(mf);
    fseek(mf, 0, SEEK_SET);
    signed char *data = (signed char *)malloc(len);
    if (data && fread(data, 1, len, mf) == (size_t)len) gameSetTune(k, data, len);
    fclose(mf);
  }
  FILE *sf = fopen(argv[1], "r");
  if (!sf) return 2;
  char line[256];
  uint32_t seed = 1, prev = 0;
  bool started = false;
  long frame = 0;
  const float dt = 1.0f / 60.0f;
  auto step = [&](uint32_t b) {
    gameFrame(dt, b, b & ~prev);
    prev = b;
    frame++;
  };
  while (fgets(line, sizeof(line), sf)) {
    char cmd[32] = "", a[128] = "";
    int n = 0;
    if (line[0] == '#' || sscanf(line, "%31s %127s %d", cmd, a, &n) < 1) continue;
    if (!strcmp(cmd, "seed")) { seed = (uint32_t)atoi(a); continue; }
    if (!started) { gameInit(seed); started = true; }
    if (!strcmp(cmd, "hold")) {
      uint32_t b = strcmp(a, "NONE") ? parseButtons(a) : 0;
      for (int i = 0; i < n; i++) step(b);
    } else if (!strcmp(cmd, "tap")) {
      step(parseButtons(a));
      step(0);
    } else if (!strcmp(cmd, "walkto")) {
      float target = (float)atof(a), x, y;
      int st;
      for (int i = 0; i < 900; i++) {
        gameDebugPos(&x, &y, &st);
        if (fabsf(x - target) < 3) break;
        step(x < target ? BTN_RIGHT : BTN_LEFT);
      }
      step(0);
    } else if (!strcmp(cmd, "waitidle")) {
      float x, y;
      int st = -1;
      step(0);
      for (int i = 0; i < 900; i++) {
        gameDebugPos(&x, &y, &st);
        if (st == 0) break;
        step(0);
      }
    } else if (!strcmp(cmd, "wav")) {   // the game runs on at 60 fps, its sound saved (48 kHz stereo)
      float secs = (float)atof(a);
      char name[64] = "sound";
      sscanf(line, "%*s %*s %63s", name);
      char path[256];
      snprintf(path, sizeof(path), "%s/%s.wav", argv[2], name);
      FILE *wf = fopen(path, "wb");
      int total = (int)(secs * 48000), per = 800;   // 800 frames: one 60th of a second
      unsigned hdr[11] = { 0x46464952u, (unsigned)(36 + total * 4), 0x45564157u, 0x20746d66u, 16, 0x00020001u, 48000, 48000 * 4, 0x00100004u, 0x61746164u, (unsigned)(total * 4) };
      if (wf) fwrite(hdr, 4, 11, wf);
      static short buf[800 * 2];
      for (int done = 0; done < total; done += per) {
        step(prev);
        gameAudio(buf, per);
        if (wf) fwrite(buf, 4, per, wf);
      }
      if (wf) fclose(wf);
    } else if (!strcmp(cmd, "room")) {
      char doors[8] = "BB", hz[8] = "";
      sscanf(line, "%*s %*s %7s %7s", doors, hz);
      gameDebugRoom(atoi(a), doors[0] == 'T', doors[1] == 'T', hz[0] == 'E' ? 1 : hz[0] == 'D' ? 2 : hz[0] == 'C' ? 3 : hz[0] == '2' ? 4 : hz[0] == 'R' ? 5 : 0);
      step(0);
    } else if (!strcmp(cmd, "control")) {   // control DRIVES GENERATORS: into QUANTUM CONTROL with them
      gameDebugControl(atoi(a), n);
      step(0);
    } else if (!strcmp(cmd, "shaft")) {     // shaft S F: into lift shaft S (0 = A; -1 the sub basement's) at floor F (-1 its lowest)
      gameDebugShaft(atoi(a), n);
      step(0);
    } else if (!strcmp(cmd, "lab")) {       // lab S F W: stage 2's lab, lift S (0 = A) at floor F (0 the top level, 6 the QUANTUMLOOP's), the maintenance man on floor W (0 the top level; -1 random)
      int s = 0, f = 6, w = -1;
      sscanf(line, "%*s %d %d %d", &s, &f, &w);
      gameDebugStage2(s, f, w);
      step(0);
    } else if (!strcmp(cmd, "gen")) {       // gen N: the room with power generator N (0..2)
      gameDebugGenerator(atoi(a));
      step(0);
    } else if (!strcmp(cmd, "shot")) {
      shot(argv[2], a);
    }
    printf("%6ld  %-28s %s\n", frame, line[strlen(line) - 1] == '\n' ? (line[strlen(line) - 1] = 0, line) : line, gameDebug());
  }
  return 0;
}
