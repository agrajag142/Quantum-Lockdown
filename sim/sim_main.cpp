// The PC test build: the game with a scripted controller, saving screenshots.
//
//   g++ -O2 -I../src ../src/game.cpp ../src/gfx.cpp sim_main.cpp -o sim && ./sim script.txt out/
//
// Script lines:  hold BUTTONS FRAMES   (BUTTONS: LEFT+JUMP etc, or NONE)
//                tap BUTTONS           (one frame down, one up)
//                shot NAME             (out/NAME.ppm)
//                seed N                (before anything else)
//                # comment
#include "game.h"
#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  if (argc < 3) { fprintf(stderr, "usage: sim script.txt outdir\n"); return 2; }
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
    } else if (!strcmp(cmd, "shot")) {
      shot(argv[2], a);
    }
    printf("%6ld  %-28s %s\n", frame, line[strlen(line) - 1] == '\n' ? (line[strlen(line) - 1] = 0, line) : line, gameDebug());
  }
  return 0;
}
