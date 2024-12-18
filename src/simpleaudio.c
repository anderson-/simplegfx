#include <simpleaudio.h>

static double phase = 0.0;
static int playing = 0;
static int samples = 0;
static int samplesc = 0;
static int frequency = 0;
static int sr = 16000;
double volume = 0.2;
static int fadein = 256;
static int fadeout = 256;

int audio_setup(void) {
  SDL_AudioSpec spec;
  SDL_memset(&spec, 0, sizeof(spec));
  spec.freq = sr;
  spec.format = AUDIO_S16;
  spec.channels = 1;
  spec.samples = 2048;
  spec.callback = audio_callback;
  spec.userdata = NULL;
  if (SDL_OpenAudio(&spec, NULL) < 0) {
    fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
    return 1;
  }
  SDL_PauseAudio(0);
  return 0;
}

void audio_cleanup(void) {
  SDL_CloseAudio();
}

void beep(int freq, int ms) {
  SDL_LockAudio();
  frequency = (double)freq;
  samples = (int)((ms / 1000.0) * sr);
  if (samples < fadein + fadeout + 10) {
    samples = fadein + fadeout + 10;
  }
  if (!playing) {
    phase = 0.0;
    samplesc = 0;
    playing = 1;
    SDL_PauseAudio(0);
  } else {
    samplesc = fadein;
  }
  SDL_UnlockAudio();
}

void audio_callback(void * userdata, Uint8 * stream, int len) {
  int16_t * buffer = (int16_t *)stream;
  int genlen = len / sizeof(int16_t);
  for (int i = 0; i < genlen; i++) {
    buffer[i] = 0;
  }
  if (!playing) {
    SDL_PauseAudio(1);
    return;
  }
  if (samplesc + genlen > samples) {
    genlen = samples - samplesc;
    if (genlen < 0) genlen = 0;
  }
  for (int i = 0; i < genlen; i++) {
    double vfact = 1.0;
    if (samplesc < fadein) {
      vfact = (double)samplesc / (double)fadein;
    } else if (samples - samplesc < fadeout) {
      vfact = (double)(samples - samplesc) / (double)fadeout;
    }
    double s = sin(2.0 * M_PI * phase) * (volume * vfact);
    double amp = s * 32767.0;
    if (amp > 32767.0) amp = 32767.0;
    if (amp < -32768.0) amp = -32768.0;
    buffer[i] = (int16_t) amp;
    phase += frequency * 1.0 / sr;
    if (phase >= 1.0) phase -= 1.0;
    samplesc++;
  }
  if (samples > 0 && samplesc >= samples) {
    playing = 0;
  }
}