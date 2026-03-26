#include "simplegfx.h"

#if defined(GFX_MACOS) && defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>
#include <time.h>

#define FB_SIZE (WINDOW_WIDTH * WINDOW_HEIGHT)
static uint8_t  g_fb[FB_SIZE * 4];
static uint8_t  g_r = 255, g_g = 255, g_b = 255;
static float    g_fps = GFX_FPS;

double volume = 0.2;

static AudioQueueRef audioQueue = NULL;
static AudioStreamBasicDescription audioFormat;

void audio_output_callback(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
  (void)inUserData;
  (void)inAQ;
  (void)inBuffer;
}

void init_audio() {
  if (audioQueue != NULL) return;

  audioFormat.mSampleRate = 44100.0;
  audioFormat.mFormatID = kAudioFormatLinearPCM;
  audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
  audioFormat.mChannelsPerFrame = 1;
  audioFormat.mFramesPerPacket = 1;
  audioFormat.mBitsPerChannel = 16;
  audioFormat.mBytesPerFrame = audioFormat.mChannelsPerFrame * audioFormat.mBitsPerChannel / 8;
  audioFormat.mBytesPerPacket = audioFormat.mBytesPerFrame;

  AudioQueueNewOutput(&audioFormat, audio_output_callback, NULL, NULL, NULL, 0, &audioQueue);
}

void cleanup_audio() {
  if (audioQueue != NULL) {
    AudioQueueDispose(audioQueue, true);
    audioQueue = NULL;
  }
}

void beep(int freq, int ms) {
  if (freq <= 0 || ms <= 0) return;

  init_audio();

  int sampleRate = 44100;
  int durationSamples = (sampleRate * ms) / 1000;
  int bufferSize = durationSamples * sizeof(int16_t);
  int16_t *buffer = malloc(bufferSize);

  if (buffer == NULL) return;

  for (int i = 0; i < durationSamples; i++) {
    float angle = 2.0 * M_PI * freq * i / sampleRate;
    buffer[i] = (int16_t)(volume * 32767 * sin(angle));
  }

  AudioQueueBufferRef queueBuffer;
  AudioQueueAllocateBuffer(audioQueue, bufferSize, &queueBuffer);
  memcpy(queueBuffer->mAudioData, buffer, bufferSize);
  queueBuffer->mAudioDataByteSize = bufferSize;

  AudioQueueEnqueueBuffer(audioQueue, queueBuffer, 0, NULL);
  AudioQueueStart(audioQueue, NULL);

  AudioQueueFlush(audioQueue);
  AudioQueueStop(audioQueue, false);

  free(buffer);
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
  int x0 = x < 0 ? 0 : x;
  int x1 = (x+w-1 >= WINDOW_WIDTH)  ? WINDOW_WIDTH-1  : x+w-1;
  int y0 = y < 0 ? 0 : y;
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

void gfx_screenshot(const char *filename) { (void)filename; }

void gfx_delay(int ms) {
  struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
  nanosleep(&ts, NULL);
}

uint16_t* gfx_get_frame_buffer(void) {
  return (uint16_t*)g_fb;
}

@interface PixelView : NSView
@end

@interface AppDelegate : NSObject <NSApplicationDelegate> {
  NSTimer  *_timer;
  uint64_t  _last_ns;
  BOOL      _shouldExit;
}
- (void)setShouldExit:(BOOL)shouldExit;
@end

static void process_key_event(NSEvent *ev) {
  if ([ev type] == NSEventTypeKeyDown || [ev type] == NSEventTypeKeyUp) {
    NSString *chars = [ev characters];
    char key = 0;
    if ([chars length] > 0) {
      key = (char)[chars characterAtIndex:0];
    } else {
      unsigned short kc = [ev keyCode];
      switch (kc) {
        case 126: key = 82; break;
        case 125: key = 81; break;
        case 124: key = 79; break;
        case 123: key = 80; break;
        case 36:  key = 13; break;
        case 48:  key = 32; break;
        case 49:  key = ' '; break;
        case 51:  key = 127; break;
        case 53:  key = 27; break;
        default: key = (char)kc; break;
      }
    }
    int down = ([ev type] == NSEventTypeKeyDown) ? 1 : 0;
    if (gfx_on_key(key, down)) {
      AppDelegate *del = (AppDelegate *)[NSApp delegate];
      [del setShouldExit:YES];
    }
  }
}

@implementation PixelView
- (BOOL)isOpaque { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)canBecomeKeyView { return YES; }

- (void)keyDown:(NSEvent *)event {
  process_key_event(event);
}

- (void)keyUp:(NSEvent *)event {
  process_key_event(event);
}

- (void)drawRect:(NSRect)rect {
  (void)rect;
  CGColorSpaceRef cs  = CGColorSpaceCreateDeviceRGB();
  CGContextRef    bmp = CGBitmapContextCreate(
    g_fb, WINDOW_WIDTH, WINDOW_HEIGHT, 8, WINDOW_WIDTH * 4, cs,
    kCGImageAlphaNoneSkipLast | kCGBitmapByteOrderDefault);
  CGImageRef img = CGBitmapContextCreateImage(bmp);
  CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
  CGContextDrawImage(ctx, CGRectMake(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT), img);
  CGImageRelease(img);
  CGContextRelease(bmp);
  CGColorSpaceRelease(cs);
}
@end

static PixelView *g_view = nil;

@implementation AppDelegate

- (void)setShouldExit:(BOOL)shouldExit {
  _shouldExit = shouldExit;
}

static uint64_t mono_ns(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notif {
  (void)notif;
  _shouldExit = NO;
  NSRect frame = NSMakeRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

  NSWindow *win = [[NSWindow alloc]
    initWithContentRect:frame
              styleMask:NSWindowStyleMaskTitled
                       | NSWindowStyleMaskClosable
                       | NSWindowStyleMaskMiniaturizable
                       | NSWindowStyleMaskResizable
              backing:NSBackingStoreBuffered
                defer:NO];

  g_view = [[PixelView alloc] initWithFrame:frame];
  [win setContentView:g_view];
  [win setTitle:@"simplegfx"];
  [win center];
  [win makeKeyAndOrderFront:nil];
  [win makeFirstResponder:g_view];
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
  uint64_t now   = mono_ns();
  float elapsed  = (now - _last_ns) / 1e9f;
  _last_ns       = now;
  if (elapsed > 0) {
    float cur = 1.0f / elapsed;
    g_fps = 0.1f * cur + 0.9f * g_fps;
  }

  gfx_clear();
  gfx_draw(g_fps);

  [g_view setNeedsDisplay:YES];
  [g_view displayIfNeeded];

  if (_shouldExit) {
    [NSApp terminate:nil];
  }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
  (void)sender;
  return YES;
}

- (void)applicationWillTerminate:(NSNotification *)notif {
  (void)notif;
  [_timer invalidate];
  gfx_app(0);
}

@end

int gfx_setup(void) {
  return 0;
}

void gfx_run(void) {
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  AppDelegate *del = [[AppDelegate alloc] init];
  [NSApp setDelegate:del];
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
  return 0;
}

#endif