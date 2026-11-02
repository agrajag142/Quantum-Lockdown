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
// The music: the platform hands over elevator.mod's bytes once (they must stay put),
// then calls gameAudio from its sound thread for stereo 16-bit samples at 48 kHz.
void gameSetMusic(signed char *data, long len);
void gameAudio(short *out, int frames);
const char *gameDebug();   // (the PC test build: where he is)
void gameDebugPos(float *x, float *y, int *state);
void gameDebugRoom(int tpl, bool topL, bool topR);   // (the PC test build: straight into a layout)

#endif // GAME_H
