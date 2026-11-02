// The PS5 side of the full-screen game: the TV picture, the DualSense, the loop.
//
// The game draws the whole 1920x1080 picture (g_frame, gfx.h); this copies it
// into the tiled scanout buffer each frame and flips. Video and controller
// code from NUCLEAR MISSION's ps5_platform.cpp (which follows the starter's
// screen.cpp: ps5-native-app-boilerplate's demo renderer, Copyright (C) 2026
// BlackBearReloaded; changes for Breeze, MiladRTX). No sound yet.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "game.h"
#include "gfx.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern "C" {
size_t sceKernelGetDirectMemorySize();
int sceKernelAllocateDirectMemory(int64_t search_start, int64_t search_end, size_t length,
                                  size_t alignment, int memory_type, int64_t *physical_address);
int sceKernelMapDirectMemory(void **address, size_t length, int protection, int flags,
                             int64_t physical_address, size_t alignment);
int sceKernelSendNotificationRequest(uint32_t device, void *request, size_t size, int blocking);
int sceKernelUsleep(uint32_t microseconds);
int sceSystemServiceHideSplashScreen();
int sceVideoOutOpen(int32_t user_id, int32_t bus_type, int32_t index, const void *param);
int sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
int sceVideoOutSubmitFlip(int32_t handle, int32_t buffer_index, uint32_t flip_mode, int64_t flip_argument);
int sceVideoOutWaitVblank(int32_t handle);
void sceVideoOutSetBufferAttribute2(void *attribute, uint64_t pixel_format, uint32_t tiling_mode,
                                    uint32_t width, uint32_t height, uint64_t option,
                                    uint32_t dcc_control, uint64_t dcc_clear_color);
int sceVideoOutRegisterBuffers2(int32_t handle, int32_t set_index, int32_t buffer_index_start,
                                void *buffers, int32_t buffer_count, void *attribute,
                                int32_t category, void *option);
int sceUserServiceInitialize(const void *params);
int sceUserServiceGetInitialUser(int32_t *user_id);
int scePadInit();
int scePadOpen(int32_t user_id, int32_t type, int32_t index, const void *param);
int scePadGetHandle(int32_t user_id, int32_t type, int32_t index);
int scePadRead(int32_t handle, void *data, int32_t count);
}

static uint64_t nowUs() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

// A system notification (the pop-up in the corner)
static void notify(const char *msg) {
  static struct { uint8_t reserved[45]; char message[3075]; } req;
  memset(&req, 0, sizeof(req));
  snprintf(req.message, sizeof(req.message), "%s", msg);
  sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

[[noreturn]] static void halt(const char *msg) {
  notify(msg);
  for (;;) sceKernelUsleep(1000000);
}

// ============================================================ the TV picture
static const int kW = FB_W, kH = FB_H;
static const int kBlock = 128, kBlocksX = kW / kBlock, kBlocksY = (kH + kBlock - 1) / kBlock;
static const size_t kFrameBytes = 0x1000000, kMemBytes = kFrameBytes * 2, kMemAlign = 0x200000;
static const size_t kUsedBytes = (size_t)kBlocksX * kBlocksY << 16;
static void *scanout[2];
static int video = -1;

// Inside a 128x128 block the pixels are swizzled (tiledOffset in NUCLEAR
// MISSION's platform). blockOrder lists the block's pixels (y << 7 | x) in the
// order they are stored, so the copy writes the scanout memory front to back.
static uint16_t blockOrder[kBlock * kBlock];

static inline uint32_t tiledInBlock(unsigned x, unsigned y) {
  return ((y << 4) & 0x70U) ^ ((y << 5) & 0xf00U) ^ ((y << 9) & 0x1000U) ^ ((y << 8) & 0x4000U) ^
         ((x << 2) & 0xcU) ^ ((x << 5) & 0x380U) ^ ((x << 4) & 0x400U) ^ ((x << 6) & 0x800U) ^ ((x << 9) & 0xa000U);
}

static void makeBlockOrder() {
  for (unsigned y = 0; y < (unsigned)kBlock; y++)
    for (unsigned x = 0; x < (unsigned)kBlock; x++) blockOrder[tiledInBlock(x, y) >> 2] = (uint16_t)(y << 7 | x);
}

static void copyFrame(void *dst) {
  uint32_t *out = (uint32_t *)dst;
  for (int by = 0; by < kBlocksY; ++by) {
    bool partial = (by + 1) * kBlock > kH;
    for (int bx = 0; bx < kBlocksX; ++bx) {
      const uint32_t *src = g_frame + (size_t)by * kBlock * kW + bx * kBlock;
      uint32_t *blk = out + ((size_t)(by * kBlocksX + bx) << 14);
      if (!partial) {
        for (int k = 0; k < kBlock * kBlock; k++) {
          unsigned p = blockOrder[k];
          blk[k] = src[(p >> 7) * kW + (p & 127)];
        }
      } else {
        int rows = kH - by * kBlock;
        for (int k = 0; k < kBlock * kBlock; k++) {
          unsigned p = blockOrder[k];
          if ((int)(p >> 7) < rows) blk[k] = src[(p >> 7) * kW + (p & 127)];
        }
      }
    }
  }
}

static void flushRange(void *address, size_t length) {
#if defined(__x86_64__)
  uint8_t *at = (uint8_t *)address, *end = at + length;
  for (; at < end; at += 64) __asm__ volatile("clflush (%0)" : : "r"(at) : "memory");
  __asm__ volatile("mfence" ::: "memory");
#else
  (void)address;
  (void)length;
#endif
}

static void videoInit() {
  video = sceVideoOutOpen(0xff, 0, 0, nullptr);
  if (video < 0) halt("Mission 2: sceVideoOutOpen failed");
  size_t pool = sceKernelGetDirectMemorySize();
  if (pool < kMemBytes) halt("Mission 2: not enough direct memory");
  int64_t phys = 0;
  if (sceKernelAllocateDirectMemory(0, (int64_t)pool, kMemBytes, kMemAlign, 3 /* wc garlic */, &phys) < 0)
    halt("Mission 2: direct-memory allocation failed");
  void *mapped = nullptr;
  if (sceKernelMapDirectMemory(&mapped, kMemBytes, 0x33, 0, phys, kMemAlign) < 0)
    halt("Mission 2: direct-memory mapping failed");
  scanout[0] = mapped;
  scanout[1] = (uint8_t *)mapped + kFrameBytes;
  struct { void *data, *metadata, *r0, *r1; } bufs[2] = { { scanout[0], nullptr, nullptr, nullptr },
                                                         { scanout[1], nullptr, nullptr, nullptr } };
  alignas(16) uint8_t attribute[80] = {};
  sceVideoOutSetFlipRate(video, 0);
  sceVideoOutSetBufferAttribute2(attribute, UINT64_C(0x8000000022000000) /* rgba8 srgb */, 0, kW, kH, 0, 0, 0);
  if (sceVideoOutRegisterBuffers2(video, 0, 0, bufs, 2, attribute, 0, nullptr) < 0)
    halt("Mission 2: buffer registration failed");
  makeBlockOrder();
}

// ============================================================ the controller
static int padHandle = -1;
static const int kSample = 120, kSamples = 16;
// (a buffer far bigger than the samples should need, on its own: if this
// firmware's pad data is bigger than 120 bytes a sample it can't spill over)
static uint8_t (*padSamples)[kSample] = nullptr;
static uint32_t padHeld = 0;           // buttons down (any sample since the last read)
static uint8_t padLX = 128, padLY = 128;

enum : uint32_t {
  RAW_OPTIONS = 0x0008, RAW_UP = 0x0010, RAW_RIGHT = 0x0020, RAW_DOWN = 0x0040, RAW_LEFT = 0x0080,
  RAW_L2 = 0x0100, RAW_R2 = 0x0200, RAW_L1 = 0x0400, RAW_R1 = 0x0800, RAW_TRIANGLE = 0x1000,
  RAW_CIRCLE = 0x2000, RAW_CROSS = 0x4000, RAW_SQUARE = 0x8000, RAW_TOUCHPAD = 0x00100000,
  RAW_INTERCEPTED = 0x80000000u,
};

static void padInit() {
  sceUserServiceInitialize(nullptr);
  int32_t user = -1;
  if (sceUserServiceGetInitialUser(&user) < 0 || user < 0 || scePadInit() < 0) return;
  padSamples = (uint8_t (*)[kSample])malloc(64 * 1024);
  if (!padSamples) return;
  memset(padSamples, 0, 64 * 1024);
  padHandle = scePadOpen(user, 0, 0, nullptr);
  if (padHandle < 0) padHandle = scePadGetHandle(user, 0, 0);
}

static void padRead() {
  if (padHandle < 0 || !padSamples) return;
  int n = scePadRead(padHandle, padSamples, kSamples);
  if (n <= 0) return;                  // nothing new: as it was
  uint32_t any = 0;
  for (int i = 0; i < n; i++) {
    const uint8_t *s = padSamples[i];
    uint32_t raw = (uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24;
    if (raw & RAW_INTERCEPTED) {       // the system has the controller (the PS menu)
      any = 0;
      padLX = padLY = 128;
      continue;
    }
    any |= raw;
    padLX = s[4];
    padLY = s[5];
  }
  padHeld = any;
}

// The DualSense as the game's buttons
static uint32_t gameButtons() {
  uint32_t raw = padHeld, b = 0;
  float lx = (padLX - 128) / 127.0f, ly = (padLY - 128) / 127.0f;
  if ((raw & RAW_LEFT) || lx < -0.5f) b |= BTN_LEFT;
  if ((raw & RAW_RIGHT) || lx > 0.5f) b |= BTN_RIGHT;
  if ((raw & RAW_UP) || ly < -0.5f) b |= BTN_UP;
  if ((raw & RAW_DOWN) || ly > 0.5f) b |= BTN_DOWN;
  if (raw & RAW_CROSS) b |= BTN_JUMP;
  if (raw & (RAW_SQUARE | RAW_CIRCLE)) b |= BTN_USE;
  if (raw & (RAW_OPTIONS | RAW_TOUCHPAD)) b |= BTN_MAP;
  if (raw & RAW_TRIANGLE) b |= BTN_NEW;
  return b;
}

// ============================================================ the program
int main() {
  padInit();
  videoInit();
  timespec wall{};
  clock_gettime(CLOCK_REALTIME, &wall);
  gameInit((uint32_t)(wall.tv_sec ^ (wall.tv_nsec >> 10)));
  // the first picture, then the splash screen can go
  gameFrame(1 / 60.0f, 0, 0);
  copyFrame(scanout[0]);
  flushRange(scanout[0], kUsedBytes);
  sceVideoOutSubmitFlip(video, 0, 1, 1);
  sceVideoOutWaitVblank(video);
  sceSystemServiceHideSplashScreen();

  int shown = 0;
  uint32_t prev = 0;
  uint64_t last = nowUs();
  for (int64_t flip = 2;; ++flip) {
    sceVideoOutWaitVblank(video);
    padRead();
    uint64_t now = nowUs();
    float dt = (now - last) / 1e6f;
    last = now;
    uint32_t b = gameButtons();
    gameFrame(dt, b, b & ~prev);
    prev = b;
    int back = shown ^ 1;
    copyFrame(scanout[back]);
    flushRange(scanout[back], kUsedBytes);
    if (sceVideoOutSubmitFlip(video, back, 1, flip) >= 0) shown = back;
  }
}
