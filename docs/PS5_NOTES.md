# PS5 notes

What we learned bringing NUCLEAR MISSION to a real PS5 (November 2026), with
the [ps5-homebrew-starter](https://github.com/MiladRTX/ps5-homebrew-starter) kit
(PS5 payload SDK v0.42), installed with ShadowMountPlus from `/data/homebrew`.
Some of it confirmed by the PS5 Doom port's own notes (ps5-doom, "PS5 facts
learned on hardware").

## The build

- The kit compiles every `.c`/`.cpp` under the app's source folder (C11,
  C++20, `-O2`, **no exceptions, no RTTI**). Header-only stand-ins go in a
  sub-folder passed as `APP_INCLUDE_PATHS`.
- The app needs `sce_sys/param.json` (title ID `PPSAnnnnn`, a content ID
  containing it, `downloadDataSize` 256) and a 512x512 PNG `icon0.png`.
- Scripts unpacked from a zip in Windows lose their "runnable" flag:
  `chmod +x` them (or run them with `bash`). Don't build with `sudo`: the SDK is
  in your own home folder.

## Memory: the big one

- **A native app's `malloc` heap is small.** Large blocks fail quietly with
  NULL. Take anything big (picture buffers, music, anything over about
  256 KB) straight from the system with `mmap(NULL, size, PROT_READ |
  PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0)`. `ps5_platform.cpp` does this in
  `heap_caps_malloc`, with a small header so `heap_caps_free` knows to
  `munmap`.
- Symptoms before the fix: the settings menu stopped opening (its 1 MB screen
  copy failed) and the lab's lift screen was black (its 4 MB picture failed).
  Both looked like unrelated bugs.
- Big static arrays (`static uint32_t frame[1920 * 1080]`) are fine.

## Files

- `/app0` is the app's own folder (on the console `/data/homebrew/<TITLE_ID>`).
  Read assets from there. New files can be copied into it over FTP without
  rebuilding.
- **An app can't see or write the rest of `/data`.** Save to `/download0`
  (the title's own storage, sized by `downloadDataSize`).
- The sandbox refuses `opendir` (and `chdir`, `access`). Don't rely on listing
  a folder.

## Video

- VideoOut with two 1920x1080 RGBA (sRGB) buffers in direct memory (type 3,
  protection 0x33), **tiled**: 128x128 blocks of 64 KB; `tiledOffset()` in
  `ps5_platform.cpp` gives each pixel's place. Flush the CPU's writes
  (`clflush` + `mfence`) before flipping.
- Draw into a plain buffer in RAM, then copy it to the scanout buffer in one
  pass; copy only the block columns that change.
- `sceVideoOutWaitVblank` paces the loop at 60 fps.

## Controller

- `scePadRead` returns 120-byte `ScePadData` samples: buttons (u32) at 0,
  sticks at 4..7, touchpad finger count at 52, first finger's x/y (u16) at
  60/62. Bit `0x80000000` set: the system has the controller (PS menu).
- Button bits: Options 0x8, Up 0x10, Right 0x20, Down 0x40, Left 0x80, L2 0x100,
  R2 0x200, L1 0x400, R1 0x800, Triangle 0x1000, Circle 0x2000, Cross 0x4000,
  Square 0x8000, touchpad click 0x100000. (Checked on the console.)
- OR the samples since the last read so a quick tap isn't missed, and act on a
  button's press (down now, not last frame).
- Doom uses `scePadReadState` (just the latest state) instead.

## Sound

- AudioOut main port: `sceAudioOutOpen(0xff, 0, 0, 256, 48000, 1)`, 16-bit
  stereo in 256-frame grains. `sceAudioOutOutput` blocks until the last grain
  is playing, so a thread calling it in a loop needs no timer.
- 48 kHz only: the game mixes at 48000 and converts 44.1 kHz WAV clips as they
  load.

## Other

- `sceKernelSendNotificationRequest` shows a pop-up: use it to report
  failures that would otherwise be silent.
- Don't name threads (`pthread_setname_np` hangs).
- The starter's PC simulator idea works well here too: the same platform code
  built for Linux with stand-ins for the `sce*` calls, a scripted controller
  and screenshots of the scanout buffer, to test without the console.
