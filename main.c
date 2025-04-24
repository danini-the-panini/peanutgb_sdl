/*
 * This example code creates an SDL window and renderer, and then clears the
 * window to a different color every frame, so you'll effectively get a window
 * that's smoothly fading between colors.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define ENABLE_SOUND 1

#include "minigb_apu.h"

#if defined(MINIGB_APU_AUDIO_FORMAT_S16SYS)
  #define AUDIO_SAMPLES_BYTES (AUDIO_SAMPLES*4)
#elif defined(MINIGB_APU_AUDIO_FORMAT_S32SYS)
  #define AUDIO_SAMPLES_BYTES (AUDIO_SAMPLES*8)
#endif

static struct minigb_apu_ctx apu;
static audio_sample_t *samples;

void audio_write(uint_fast32_t addr, uint8_t val)
{
  minigb_apu_audio_write(&apu, addr, val);
}

uint8_t audio_read(uint_fast32_t addr)
{
  return minigb_apu_audio_read(&apu, addr);
}

#include "peanut_gb.h"

const uint8_t COLORS[4][3] = {
  { 223, 248, 209 },
  { 136, 193, 112 },
  {  52, 104,  86 },
  {   8,  24,  32 }
};

static struct gb_s gb;
static FILE *fptr;

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *lcd = NULL;
static SDL_AudioStream *stream = NULL;

uint8_t rom_read(struct gb_s *gb, const uint_fast32_t addr)
{
  fseek(fptr, addr, SEEK_SET);
  uint8_t buffer[1];
  fread(buffer, sizeof(uint8_t), 1, fptr);
  return buffer[0];
}

uint8_t cart_ram_read(struct gb_s *gb, const uint_fast32_t addr)
{
  return 0;
}

void cart_ram_write(struct gb_s *gb, const uint_fast32_t addr, const uint8_t value)
{
}

void on_error(struct gb_s *gb, const enum gb_error_e e, const uint16_t addr)
{
  printf("ERROR: %d at %d\n", e, addr);
}

void lcd_draw_line(struct gb_s *gb, const uint8_t *pixels, const uint_fast8_t line)
{
  SDL_Rect rect = {0, line, LCD_WIDTH, 1};
  void *lcd_pixels;
  int pitch;
  SDL_LockTexture(lcd, &rect, &lcd_pixels, &pitch);
  for (int i = 0; i < LCD_WIDTH; i++) {
    memcpy(lcd_pixels + i*3, COLORS[pixels[i] & 3], 3);
  }
  SDL_UnlockTexture(lcd);
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (argc != 2) {
      SDL_Log("usage: %s path/to/rom.gb", argv[0]);
      return SDL_APP_FAILURE;
    }

    fptr = fopen(argv[1], "r");
    if (fptr == NULL) {
      SDL_Log("Could not open file");
      return SDL_APP_FAILURE;
    }

    SDL_AudioSpec spec;

    SDL_SetAppMetadata("PeanutGB", "1.0", "dev.danini.peanutgb");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/renderer/clear", LCD_WIDTH, LCD_HEIGHT, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    spec.channels = 2;
    #if defined(MINIGB_APU_AUDIO_FORMAT_S16SYS)
      spec.format = SDL_AUDIO_S16;
    #elif defined(MINIGB_APU_AUDIO_FORMAT_S32SYS)
      spec.format = SDL_AUDIO_S32;
    #endif
    spec.freq = AUDIO_SAMPLE_RATE;
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    samples = malloc(AUDIO_SAMPLES_BYTES);
    memset(samples, 0, AUDIO_SAMPLES_BYTES);
    if (!stream) {
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* SDL_OpenAudioDeviceStream starts the device paused. You have to tell it to start! */
    SDL_ResumeAudioStreamDevice(stream);

    lcd = SDL_CreateTexture(renderer,
       SDL_PIXELFORMAT_RGB24,
       SDL_TEXTUREACCESS_STREAMING, 
       LCD_WIDTH,
       LCD_HEIGHT);

    enum gb_init_error_e e = gb_init(&gb,
      rom_read,
      cart_ram_read,
      cart_ram_write,
      on_error,
      NULL
    );

    if (e != GB_INIT_NO_ERROR) {
      printf("Error initializing gb: %d\n", e);
      return SDL_APP_FAILURE;
    }

    gb_init_lcd(&gb, lcd_draw_line);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

void handleKey(SDL_Keycode key, bool pressed) {
  switch (key) {
  case SDLK_Z:         gb.direct.joypad_bits.a      = !pressed; break;
  case SDLK_X:         gb.direct.joypad_bits.b      = !pressed; break;
  case SDLK_BACKSPACE: gb.direct.joypad_bits.select = !pressed; break;
  case SDLK_RETURN:    gb.direct.joypad_bits.start  = !pressed; break;
  case SDLK_RIGHT:     gb.direct.joypad_bits.right  = !pressed; break;
  case SDLK_LEFT:      gb.direct.joypad_bits.left   = !pressed; break;
  case SDLK_UP:        gb.direct.joypad_bits.up     = !pressed; break;
  case SDLK_DOWN:      gb.direct.joypad_bits.down   = !pressed; break;
  }
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_QUIT:
      return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    case SDL_EVENT_KEY_DOWN:
      handleKey(event->key.key, true);
      break;
    case SDL_EVENT_KEY_UP:
      handleKey(event->key.key, false);
      break;
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    Uint64 start = SDL_GetPerformanceCounter();

    gb_run_frame(&gb);

    if (!gb.direct.frame_skip) {
      minigb_apu_audio_callback(&apu, samples);
      SDL_PutAudioStreamData(stream, samples, AUDIO_SAMPLES_BYTES);
    }

    SDL_RenderTexture(renderer, lcd, NULL, NULL);

    /* put the newly-cleared rendering on the screen. */
    SDL_RenderPresent(renderer);

    Uint64 end = SDL_GetPerformanceCounter();
    float elapsed = (end - start) / (float)SDL_GetPerformanceFrequency();
    usleep(fmax(0.0f, (1.0f / VERTICAL_SYNC - elapsed) * 1000000.0f));

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
    fclose(fptr);
    free(samples);
}
