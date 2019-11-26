#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "gameboy.h"

#define SCREEN_SCALE 2


// Stolen from:
// - https://wiki.libsdl.org/SDL_CreateWindow
// - https://wiki.libsdl.org/SDL_RenderCopy
// - https://wiki.libsdl.org/SDL_GameControllerOpen
// - https://wiki.libsdl.org/SDL_PollEvent
int main(int argc, char* argv[]) {

  // Check for arguments
  if (argc != 2) {
    assert(argc >= 1);
    fprintf(stderr, "You need to specify a ROM file path: %s <rom-file-path>\n", argv[0]);
    return 1;
  }

  // Initialize SDL2
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);

  // Search for gamecontroller
  SDL_GameController *controller = NULL;
  for (int i = 0; i < SDL_NumJoysticks(); ++i) {
    if (SDL_IsGameController(i)) {
      controller = SDL_GameControllerOpen(i);
      if (controller != NULL) {
        break;
      } else {
        fprintf(stderr, "Could not open gamecontroller %d: %s\n", i, SDL_GetError());
      }
    }
  }
  if (controller != NULL) {
    const char* name = SDL_GameControllerName(controller);
    printf("Enabled gamecontroller support for '%s'\n", name);
  } else {
    printf("Disabled gamecontroller support\n");
  }

  // Create an application window
  SDL_Window* window = SDL_CreateWindow(
    "gb-emu",                                      // window title
    SDL_WINDOWPOS_UNDEFINED,                       // initial x position
    SDL_WINDOWPOS_UNDEFINED,                       // initial y position
    GAMEBOY_SCREEN_WIDTH * SCREEN_SCALE,   // width, in pixels
    GAMEBOY_SCREEN_HEIGHT * SCREEN_SCALE,  // height, in pixels
    0                                              // flags - see below
  );
  if (window == NULL) {
    fprintf(stderr, "SDL_CreateWindow() failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(
    window,
    -1,
    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
  );
  assert(renderer != NULL); //FIXME: Error checking

  // Create surface
  SDL_Texture* texture = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_RGBA32,
    SDL_TEXTUREACCESS_STREAMING,
    GAMEBOY_SCREEN_WIDTH,
    GAMEBOY_SCREEN_HEIGHT
  );
  assert(texture != NULL); //FIXME: Error checking

  // Call initialization
  bool success = gameboy_init(argv[1]);
  if (!success) {
    return 1;
  }

  // Get access to keyboard
  const Uint8* keyboard = SDL_GetKeyboardState(NULL);

  // Mainloop
  bool exit = false;
  while(!exit) {

    // Handle events
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      switch(event.type) {
      case SDL_QUIT:
        exit = true;
        break;
      case SDL_KEYDOWN:
        if (!event.key.repeat) {
          SDL_Scancode scancode = event.key.keysym.scancode;
          switch(scancode) {

          case SDL_SCANCODE_ESCAPE:
            exit = true;
            break;

          // Case ranges are a C extension: https://gcc.gnu.org/onlinedocs/gcc/Case-Ranges.html
          // This is widely supported; but if not supported, can be worked around easily
          case SDL_SCANCODE_F1 ... SDL_SCANCODE_F12:
            gameboy_debug_hotkey(1 + (scancode - SDL_SCANCODE_F1));
            break;

          default:
            break;
          }
        }
        break;
      default:
        break;
      }
    }

    //FIXME: Update input from keyboard
    gameboy_input.start = keyboard[SDL_SCANCODE_RETURN];
    gameboy_input.select = keyboard[SDL_SCANCODE_BACKSPACE];
    gameboy_input.a = keyboard[SDL_SCANCODE_X];
    gameboy_input.b = keyboard[SDL_SCANCODE_Z];
    gameboy_input.up = keyboard[SDL_SCANCODE_UP];
    gameboy_input.down = keyboard[SDL_SCANCODE_DOWN];
    gameboy_input.left = keyboard[SDL_SCANCODE_LEFT];
    gameboy_input.right = keyboard[SDL_SCANCODE_RIGHT];

    // Optionally, add input from gamecontroller
    if (controller) {
      SDL_GameControllerUpdate();

      //FIXME: Turn X and Y button into turbo-buttons?

      gameboy_input.start |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START);
      gameboy_input.select |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK);
      gameboy_input.a |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B);
      gameboy_input.a |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y);
      gameboy_input.b |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A);
      gameboy_input.b |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X);
      gameboy_input.up |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
      gameboy_input.down |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
      gameboy_input.left |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
      gameboy_input.right |= SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

      //FIXME: Support analog sticks?
    }

    // Emulate a bit of virtual time for the gameboy
    //FIXME: Measure how much time has passed, so we can emulate the right amount of time
    gameboy_step();

    // Modify surface by converting grayscale framebuffer to RGBA32
    uint8_t* pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, (void*)&pixels, &pitch);
    for(unsigned int y = 0; y < GAMEBOY_SCREEN_HEIGHT; y++) {
      for(unsigned int x = 0; x < GAMEBOY_SCREEN_WIDTH; x++) {
        unsigned int gameboy_framebuffer_row = y * GAMEBOY_SCREEN_WIDTH;
        uint8_t gameboy_framebuffer_pixel = gameboy_framebuffer[gameboy_framebuffer_row + x];
        unsigned int texture_row = y * pitch;
        uint8_t* texture_pixel = &pixels[texture_row + x * 4];
        texture_pixel[0] = gameboy_framebuffer_pixel; // Red
        texture_pixel[1] = gameboy_framebuffer_pixel; // Green
        texture_pixel[2] = gameboy_framebuffer_pixel; // Blue
        texture_pixel[3] = 0xFF;                      // Alpha
      }
    }
    SDL_UnlockTexture(texture);

    // Render the current surface
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
  }

  // Inform the virtual gameboy that we are going to exit
  gameboy_notify_exit();

  // Clean up
  //FIXME: Cleanup texture, renderer, etc.
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
