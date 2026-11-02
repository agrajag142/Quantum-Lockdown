// Makes sce_sys/icon0.png's picture (512x512) with the game's own drawing:
// the agent mid-somersault over a catwalk, the title in vector letters.
//   g++ -O2 -I../src ../src/gfx.cpp icon_main.cpp -o icon && ./icon icon.ppm
#include "gfx.h"
#include "agent_rig.h"
#include <stdio.h>

int main(int argc, char **argv) {
  gfxInit();
  gfxClipAll();
  const float S = 512 / UNIT_K;                       // the icon, in units (341)
  for (int y = 0; y < 342; y++) {                      // dark blue-grey, lighter at the middle
    float k = 1.0f - fabsf(y - 150) / 200.0f;
    fillRect(0, y, 341, y, rgb565(10 + (int)(22 * k), 13 + (int)(26 * k), 20 + (int)(34 * k)));
  }
  fillRect(0, 238, 341, 251, rgb565(80, 86, 98));        // a catwalk
  fillRect(0, 238, 341, 240, rgb565(150, 156, 168));
  hazardBand(0, 341, 252, 259);
  AgentPose p;
  agentSample(ANIM_JUMP, 0.42f, p);                    // tucked, turning over
  p.lift = 6;
  drawAgentU(p, 150, 226, 1, 2.0f);
  drawVectorText("MISSION", S / 2, 292, 6.2f, rgb565(255, 190, 70), true);
  drawVectorText("2", 292, 66, 9.0f, rgb565(255, 190, 70), true);
  FILE *f = fopen(argc > 1 ? argv[1] : "icon.ppm", "wb");
  if (!f) return 1;
  fprintf(f, "P6\n512 512\n255\n");
  for (int y = 0; y < 512; y++)
    for (int x = 0; x < 512; x++) {
      uint32_t c = g_frame[y * FB_W + x];
      unsigned char px[3] = { (unsigned char)(c & 255), (unsigned char)(c >> 8 & 255), (unsigned char)(c >> 16 & 255) };
      fwrite(px, 1, 3, f);
    }
  fclose(f);
  return 0;
}
