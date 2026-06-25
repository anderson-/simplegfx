#include "simplegfx.h"
#if defined(GFX_MACOS) && defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>

#define FB_SIZE (WINDOW_WIDTH * WINDOW_HEIGHT)

static uint8_t g_fb[FB_SIZE * 4];
static uint8_t g_r = 255, g_g = 255, g_b = 255;

static AudioQueueRef audioQueue = NULL;
static AudioStreamBasicDescription audioFormat;
static audio_stream_t osx_fn = NULL;
static void *osx_user = NULL;
static int _gfx_osx_playing = 0;
static int audioSampleRate = GFXA_SAMPLE_RATE;

void audio_output_callback(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
  (void)inUserData;
  (void)inAQ;
  int16_t *buf = (int16_t *)inBuffer->mAudioData;
  int len = inBuffer->mAudioDataByteSize / sizeof(int16_t);
  if (!_gfx_osx_playing || !osx_fn) {
    memset(buf, 0, inBuffer->mAudioDataByteSize);
    AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    return;
  }
  int written = osx_fn(buf, len, osx_user);
  if (written <= 0) {
    _gfx_osx_playing = 0;
    memset(buf, 0, inBuffer->mAudioDataByteSize);
  } else {
    for (int i = written; i < len; i++) buf[i] = 0;
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
    AudioQueueAllocateBuffer(audioQueue, GFXA_BUF_SIZE * sizeof(int16_t), &buf);
    buf->mAudioDataByteSize = GFXA_BUF_SIZE * sizeof(int16_t);
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

void gfxa_raw_stream(audio_stream_t fn) {
  init_audio();
  osx_fn = fn;
  _gfx_osx_playing = 1;
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

uint16_t* gfx_get_frame_buffer(void) {
  return (uint16_t*)g_fb;
}

@interface PixelView : NSView @end
@interface AppDelegate : NSObject <NSApplicationDelegate> {
  BOOL _shouldExit;
}
- (void)setShouldExit:(BOOL)shouldExit;
- (BOOL)shouldExit;
@end

static void process_key_event(NSEvent *ev) {
  if ([ev type] == NSEventTypeKeyDown || [ev type] == NSEventTypeKeyUp) {
    NSString *chars = [ev characters];
    char key = ([chars length] > 0) ? (char)[chars characterAtIndex:0] : (char)[ev keyCode];
    int down = ([ev type] == NSEventTypeKeyDown) ? 1 : 0;
    AppDelegate *del = (AppDelegate *)[NSApp delegate];
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
- (BOOL)shouldExit { return _shouldExit; }
- (void)applicationDidFinishLaunching:(NSNotification *)n {
  (void)n;
  _shouldExit = NO;
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

  gfx_set_font(&font5x7);
  gfx_app(1);
  gfx_run();
  gfx_app(0);
  [NSApp terminate:nil];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)s { (void)s; return YES; }
@end

int gfx_setup(void) { return 0; }

int gfx_poll(void) {
  NSEvent *e;
  int exit = 0;
  while ((e = [NSApp nextEventMatchingMask:NSEventMaskAny
                                 untilDate:[NSDate distantPast]
                                    inMode:NSDefaultRunLoopMode
                                   dequeue:YES])) {
    [NSApp sendEvent:e];
    AppDelegate *del = (AppDelegate *)[NSApp delegate];
    if ([del shouldExit]) exit = 1;
  }
  return exit;
}

void gfx_flip(void) {
  [g_view setNeedsDisplay:YES];
  [g_view displayIfNeeded];
}

void gfx_cleanup(void) {
  cleanup_audio();
  gfx_clear_text_buffer();
}

int main(void) {
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp setDelegate:[[AppDelegate alloc] init]];
  [NSApp run];
  return 0;
}

#endif
