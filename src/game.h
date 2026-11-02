// game.h - what the platform (PS5, or the PC test build) and the game say to each other

#ifndef GAME_H
#define GAME_H

#include <stdint.h>

// The controls, as the game sees them (the platform maps the DualSense onto these)
enum : uint32_t {
  BTN_LEFT = 0x01, BTN_RIGHT = 0x02, BTN_UP = 0x04, BTN_DOWN = 0x08,
  BTN_JUMP = 0x10,    // Cross: the somersault
  BTN_USE = 0x20,     // Square / Circle: hold to search
  BTN_MAP = 0x40,     // Options / touchpad: the map
  BTN_NEW = 0x80,     // Triangle: on the map, lays out a new complex
};

void gameInit(uint32_t seed);
// One frame: dt in seconds, held = buttons down now, pressed = went down this frame.
// Draws the whole picture into g_frame (gfx.h).
void gameFrame(float dt, uint32_t held, uint32_t pressed);
const char *gameDebug();   // (the PC test build: where he is)
void gameDebugPos(float *x, float *y, int *state);

#endif // GAME_H
