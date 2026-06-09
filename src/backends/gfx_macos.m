#include "simplegfx.h"
#if defined(GFX_MACOS) && defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>
#include <time.h>

#define FB_SIZE (WINDOW_WIDTH * WINDOW_HEIGHT)

static uint8_t g_fb[FB_SIZE * 4];
static uint8_t g_r = 255, g_g = 255, g_b = 255;
static float g_fps = GFX_FPS;
double gfx_volume = 0.2;

// Audio state — callback-driven, same pattern as SDL backend
static AudioQueueRef audioQueue = NULL;
static AudioStreamBasicDescription audioFormat;
static double audioPhase = 0.0;
static int audioPlaying = 0;
static int audioSamples = 0;
static int audioSampleCount = 0;
static int audioFrequency = 0;
static int audioSampleRate = 16000;
static const int fadeSamples = 256; // 16ms (matches SDL)

void audio_output_callback(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
  (void)inUserData;
  (void)inAQ;
  int16_t *buf = (int16_t *)inBuffer->mAudioData;
  int len = inBuffer->mAudioDataByteSize / sizeof(int16_t);
  // Initialize all samples to silence
  memset(buf, 0, inBuffer->mAudioDataByteSize);
  if (!audioPlaying) {
    AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    return;
  }
  int genlen = len;
  if (audioSampleCount + genlen > audioSamples) {
    genlen = audioSamples - audioSampleCount;
    if (genlen < 0) genlen = 0;
  }
  if (audioFrequency > 0) {
    for (int i = 0; i < genlen; i++) {
      double s = sin(2.0 * M_PI * audioPhase) * gfx_volume;
      int si = audioSampleCount;
      if (si < fadeSamples) {
        s *= (double)si / fadeSamples;
      } else if (audioSamples - si <= fadeSamples) {
        s *= (double)(audioSamples - si) / fadeSamples;
      }
      int16_t v = (int16_t)(s * 32767.0);
      if (v > 32767) v = 32767;
      if (v < -32768) v = -32768;
      buf[i] = v;
      audioPhase += (double)audioFrequency / audioSampleRate;
      if (audioPhase >= 1.0) audioPhase -= 1.0;
      audioSampleCount++;
    }
  }
  if (audioSamples > 0 && audioSampleCount >= audioSamples) {
    audioPlaying = 0;
  }
  AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
}

void init_audio() {
  if (audioQueue != NULL) return;
  audioFormat.mSampleRate = audioSampleRate;
  audioFormat.mFormatID = kAudioFormatLinearPCM;
  audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
  audioFormat.mChannelsPerFrame = 1;
  audioFormat.mFramesPerPacket = 1;
  audioFormat.mBitsPerChannel = 16;
  audioFormat.mBytesPerFrame = audioFormat.mChannelsPerFrame * audioFormat.mBitsPerChannel / 8;
  audioFormat.mBytesPerPacket = audioFormat.mBytesPerFrame;
  AudioQueueNewOutput(&audioFormat, audio_output_callback, NULL, NULL, NULL, 0, &audioQueue);

  // Prime 2 buffers so the callback keeps running
  for (int i = 0; i < 2; i++) {
    AudioQueueBufferRef buf;
    AudioQueueAllocateBuffer(audioQueue, 512, &buf);
    buf->mAudioDataByteSize = 512;
    AudioQueueEnqueueBuffer(audioQueue, buf, 0, NULL);
  }
  AudioQueueStart(audioQueue, NULL);
}

void cleanup_audio() {
  if (audioQueue != NULL) {
    AudioQueueStop(audioQueue, true);
    AudioQueueDispose(audioQueue, true);
    audioQueue = NULL;
  }
}

void gfx_beep(int freq, int ms) {
  if (ms <= 0) return;
  init_audio();
  audioFrequency = freq;
  audioSamples = (ms > 0) ? audioSampleRate * ms / 1000 : 0;
  audioSampleCount = 0;
  audioPhase = 0.0;
  audioPlaying = 1;
  // Wait for callback to finish (same polling pattern as SDL)
  while (audioPlaying) {
    gfx_delay(1);
  }
}

void gfx_set_color(int r, int g, int b) {
  g_r = (uint8_t)r;
  g_g = (uint8_t)g;
  g_b = (uint8_t)b;
}

void gfx_point(int x, int y) {
  if (x < 0 || y < 0 || x >= WINDOW_WIDTH || y >= WINDOW_HEIGHT) return;
  int i = (y * WINDOW_WIDTH + x) * 4;
  g_fb[i+0] = g_r;
  g_fb[i+1] = g_g;
  g_fb[i+2] = g_b;
  g_fb[i+3] = 255;
  elm++;
}

void gfx_fill_rect(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
  int x1 = (x+w-1 >= WINDOW_WIDTH) ? WINDOW_WIDTH-1 : x+w-1;
  int y1 = (y+h-1 >= WINDOW_HEIGHT) ? WINDOW_HEIGHT-1 : y+h-1;
  for (int py = y0; py <= y1; py++) {
    int base = (py * WINDOW_WIDTH + x0) * 4;
    for (int px = x0; px <= x1; px++, base += 4) {
      g_fb[base+0] = g_r;
      g_fb[base+1] = g_g;
      g_fb[base+2] = g_b;
      g_fb[base+3] = 255;
    }
  }
  elm++;
}

void gfx_clear(void) {
  memset(g_fb, 0, sizeof(g_fb));
  for (int i = 3; i < FB_SIZE * 4; i += 4) g_fb[i] = 255;
  elm = 0;
}

void gfx_delay(int ms) {
  struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
  nanosleep(&ts, NULL);
}

uint16_t* gfx_get_frame_buffer(void) {
  return (uint16_t*)g_fb;
}

@interface PixelView : NSView @end
@interface AppDelegate : NSObject <NSApplicationDelegate> {
  NSTimer *_timer;
  uint64_t _last_ns;
  BOOL _shouldExit;
  int _powerEscConsecutiveCount;
  BOOL _powerEscPressed;
}
- (void)setShouldExit:(BOOL)shouldExit;
- (void)handlePowerEscPress:(BOOL)isPressed;
- (void)resetPowerEscCount;
@end

static void process_key_event(NSEvent *ev) {
  if ([ev type] == NSEventTypeKeyDown || [ev type] == NSEventTypeKeyUp) {
    NSString *chars = [ev characters];
    char key = ([chars length] > 0) ? (char)[chars characterAtIndex:0] : (char)[ev keyCode];
    int down = ([ev type] == NSEventTypeKeyDown) ? 1 : 0;
    AppDelegate *del = (AppDelegate *)[NSApp delegate];
    if (key != BTN_GP_POWER_ESC && down) [del resetPowerEscCount];
    if (key == BTN_GP_POWER_ESC) [del handlePowerEscPress:down];
    if (gfx_on_key(key, down)) [del setShouldExit:YES];
  }
}

@implementation PixelView
- (BOOL)isOpaque { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)canBecomeKeyView { return YES; }
- (void)keyDown:(NSEvent *)e { process_key_event(e); }
- (void)keyUp:(NSEvent *)e { process_key_event(e); }
- (void)drawRect:(NSRect)r {
  (void)r;
  CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
  CGContextRef bmp = CGBitmapContextCreate(g_fb, WINDOW_WIDTH, WINDOW_HEIGHT, 8, WINDOW_WIDTH * 4, cs,
                                           kCGImageAlphaNoneSkipLast | kCGBitmapByteOrderDefault);
  CGImageRef img = CGBitmapContextCreateImage(bmp);
  CGContextDrawImage([[NSGraphicsContext currentContext] CGContext], CGRectMake(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT), img);
  CGImageRelease(img);
  CGContextRelease(bmp);
  CGColorSpaceRelease(cs);
}
@end

static PixelView *g_view = nil;

@implementation AppDelegate
- (void)setShouldExit:(BOOL)s { _shouldExit = s; }
- (void)handlePowerEscPress:(BOOL)p {
  if (p) {
    _powerEscPressed = YES;
  } else if (_powerEscPressed) {
    _powerEscConsecutiveCount++;
    _powerEscPressed = NO;
    if (_powerEscConsecutiveCount >= 3) [self setShouldExit:YES];
  }
}
- (void)resetPowerEscCount { _powerEscConsecutiveCount = 0; _powerEscPressed = NO; }
static uint64_t mono_ns(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
}
- (void)applicationDidFinishLaunching:(NSNotification *)n {
  (void)n;
  _shouldExit = NO;
  _powerEscConsecutiveCount = 0;
  _powerEscPressed = NO;
  NSRect f = NSMakeRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  NSWindowStyleMask mask = NSWindowStyleMaskTitled
                         | NSWindowStyleMaskClosable
                         | NSWindowStyleMaskMiniaturizable
                         | NSWindowStyleMaskResizable;
  NSWindow *w = [[NSWindow alloc] initWithContentRect:f
                                            styleMask:mask
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
  g_view = [[PixelView alloc] initWithFrame:f];
  [w setContentView:g_view];
  [w setTitle:@"simplegfx"];
  [w center];
  [w makeKeyAndOrderFront:nil];
  [w makeFirstResponder:g_view];
  [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
  gfx_app(1);
  _last_ns = mono_ns();
  _timer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / GFX_FPS)
                                            target:self
                                          selector:@selector(tick:)
                                          userInfo:nil
                                           repeats:YES];
}
- (void)tick:(NSTimer *)t {
  (void)t;
  uint64_t now = mono_ns();
  float el = (now - _last_ns) / 1e9f;
  _last_ns = now;
  if (el > 0) g_fps = 0.1f * (1.0f / el) + 0.9f * g_fps;
  if (gfx_draw(g_fps)) {
    [g_view setNeedsDisplay:YES];
    [g_view displayIfNeeded];
    gfx_clear();
  }
  if (_shouldExit) [NSApp terminate:nil];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)s { (void)s; return YES; }
- (void)applicationWillTerminate:(NSNotification *)n { (void)n; [_timer invalidate]; gfx_app(0); }
@end

int gfx_setup(void) { return 0; }

static void loop(void) {
  NSEvent *e;
  while ((e = [NSApp nextEventMatchingMask:NSEventMaskAny
                                 untilDate:nil
                                    inMode:NSDefaultRunLoopMode
                                   dequeue:YES])) {
    [NSApp sendEvent:e];
  }
}

void gfx_run(void) {
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp setDelegate:[[AppDelegate alloc] init]];
  gfx_yeld = loop;
  [NSApp run];
}

void gfx_cleanup(void) {
  cleanup_audio();
  gfx_clear_text_buffer();
}

int main(void) {
  gfx_setup();
  gfx_set_font(&font5x7);
  gfx_run();
  gfx_cleanup();
  return 0;
}

#endif
