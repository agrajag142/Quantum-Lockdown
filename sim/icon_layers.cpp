// The home-screen icon's layers, drawn with the game's own code (its title letters, the agent running), for
// sim/make_icon.py to put together into sce_sys/icon0.png:
//   g++ -std=c++20 -O2 -I../src icon_layers.cpp ../src/gfx.cpp micromod*.o -o icon_layers && ./icon_layers
//   python3 make_icon.py . ../sce_sys/icon0.png
// (the .o files: as the sim's build makes them)
#include "../src/game.cpp"
#define OUT "."
static void dumpRegion(const char *fn, int x0, int y0, int w, int h) {
  FILE *f = fopen(fn, "wb"); fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { uint32_t c = g_frame[(y0 + y) * FB_W + x0 + x]; unsigned char px[3] = { (unsigned char)(c & 255), (unsigned char)(c >> 8 & 255), (unsigned char)(c >> 16 & 255) }; fwrite(px, 1, 3, f); }
  fclose(f);
}
int main() {
  gameInit(7);
  gfxTarget = g_frame;
  launchSite = 2; startLaunchScene(2); launchQueued = 0;
  float times[] = { 3.6f, 4.4f, 5.0f, 6.0f, 8.0f, 10.0f };
  float t = 0; int k = 0;
  while (k < 6) {
    updateLaunchScene(1 / 60.0f); t += 1 / 60.0f;
    if (t >= times[k]) { pxRect(0, 0, FB_W - 1, FB_H - 1, 0); lsNoNames = true; drawLaunchScene(); lsNoNames = false; gfxClipAll(); char fn[128]; snprintf(fn, 128, OUT "/launch_%d.ppm", k); dumpRegion(fn, LS_X, LS_Y, LS_SIZE, LS_SIZE); printf("%d lsScene %d\n", k, lsScene); k++; }
  }
  pxRect(0, 0, FB_W - 1, FB_H - 1, 0);
  drawVectorTextW("QUANTUM", 640, 120, 14.0f, rgb565(255, 255, 255), 4.6f);
  drawVectorTextW("LOCKDOWN", 640, 300, 14.0f, rgb565(255, 255, 255), 4.6f);
  dumpRegion(OUT "/title.ppm", 0, 0, FB_W, 640);
  pxRect(0, 0, FB_W - 1, FB_H - 1, 0);
  AgentPose p; agentSample(ANIM_RUN, 0.3f, p); drawAgentU(p, 320, 600, 1, 4.0f);
  AgentPose q; agentSample(ANIM_JUMP, 0.42f, q); q.lift = 6; drawAgentU(q, 900, 600, 1, 4.0f);
  dumpRegion(OUT "/agent.ppm", 0, 0, FB_W, FB_H);
  return 0;
}
