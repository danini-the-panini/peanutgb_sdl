/*
 * This example code creates an SDL window and renderer, and then clears the
 * window to a different color every frame, so you'll effectively get a window
 * that's smoothly fading between colors.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#include <stdio.h>
#include <string.h>
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "peanut_gb.h"

const uint8_t COLORS[4][3] = {
  { 223, 248, 209 },
  { 136, 193, 112 },
  {  52, 104,  86 },
  {   8,  24,  32 }
};

struct gb_s gb;
FILE *fptr;

SDL_Texture *lcd;

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

/* We will use this renderer to draw into this window every frame. */
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

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

    SDL_SetAppMetadata("Example Renderer Clear", "1.0", "com.example.renderer-clear");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/renderer/clear", LCD_WIDTH, LCD_HEIGHT, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

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

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    gb_run_frame(&gb);

    SDL_RenderTexture(renderer, lcd, NULL, NULL);

    /* put the newly-cleared rendering on the screen. */
    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
    fclose(fptr);
}
