#include "ALL.H"
#include <SDL.h>
#include "scancodes_windows.h"
#include <chrono>

#ifdef __APPLE__
#include "metal_renderer.h"
#include <SDL_syswm.h>
#endif

SDL_Window* SDLWindow;
SDL_Surface* SDLSurfaceMain;
SDL_Surface* SDLSurfacePaletted;

#ifdef __APPLE__
static bool use_metal = true;
static Uint32 CurrentPalette[256];
#endif

// FPS tracking
static Uint32 fps_frame_count = 0;
static Uint32 fps_last_time = 0;
static double fps_current = 0.0;

// Ha igaz, alulrol indul y (kirajz320 allitja igazba):
int Locky0_alul = 0;

static bool LeftMouseDown = false;
static bool RightMouseDown = false;

int sdl_init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        hiba((char*)SDL_GetError());
        return 1;
    }

    SDLWindow = SDL_CreateWindow("Elasto Mania", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!SDLWindow) {
        hiba((char*)SDL_GetError());
        return 1;
    }

    SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
    SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);

#ifdef __APPLE__
    // Initialize Metal renderer for GPU palette lookup
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(SDLWindow, &wmInfo)) {
        void* nsWindow = (void*)wmInfo.info.cocoa.window;
        if (metal_init(nsWindow, SCREEN_WIDTH, SCREEN_HEIGHT) == 0) {
            use_metal = true;
            printf("Metal renderer initialized successfully!\n");
            
            // Initialize default palette
            for (int i = 0; i < 256; i++) {
                CurrentPalette[i] = 0xFF000000 | (i << 16) | (i << 8) | i;
            }
            metal_update_palette(CurrentPalette);
        } else {
            use_metal = false;
            printf("Metal renderer initialization failed, falling back to SDL\n");
        }
    } else {
        use_metal = false;
        printf("Failed to get window info, using SDL renderer\n");
    }
    
    if (!use_metal)
#endif
    {
        SDLSurfaceMain = SDL_GetWindowSurface(SDLWindow);
        if (!SDLSurfaceMain) {
            hiba((char*)SDL_GetError());
            return 1;
        }

        SDL_LockSurface(SDLSurfaceMain);
        for (int i = 0; i < (SDLSurfaceMain->w * SDLSurfaceMain->h * 4); i++) {
            ((unsigned char*)SDLSurfaceMain->pixels)[i] = 0xFF;
        }
        SDL_UnlockSurface(SDLSurfaceMain);
        SDL_UpdateWindowSurface(SDLWindow);
    }

    SDLSurfacePaletted = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0,
                                                        SDL_PIXELFORMAT_INDEX8);
    if (!SDLSurfacePaletted) {
        hiba((char*)SDL_GetError());
        return 1;
    }

    // Initialize FPS counter
    fps_last_time = SDL_GetTicks();

    return 0;
}

static unsigned char* Mutbuffer[SCREEN_HEIGHT];
static int Bufflocked = 0;

unsigned char** lockbackbuffer() {
    if (Bufflocked) {
        hiba("lockbackbuffer-ben Bufflocked!");
    }
    Bufflocked = 1;

    unsigned char* mut = (unsigned char*)SDLSurfacePaletted->pixels;
    if (Locky0_alul) {
        // Set the row buffer bottom-down
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            Mutbuffer[SCREEN_HEIGHT - 1 - y] = mut;
            mut += SDLSurfacePaletted->w;
        }
    } else {
        // Set the row buffer top-down
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            Mutbuffer[y] = mut;
            mut += SDLSurfacePaletted->w;
        }
    }

    return Mutbuffer;
}

void unlockbackbuffer(void) {
    if (!Bufflocked) {
        hiba("unlockbackbuffer-ben !Bufflocked!");
    }
    Bufflocked = 0;

#ifdef __APPLE__
    if (use_metal) {
        // Upload 8-bit indices directly to GPU - Metal shader does palette lookup!
        metal_upload_frame((unsigned char*)SDLSurfacePaletted->pixels, SDLSurfacePaletted->pitch);
        metal_present();
        
        // Update FPS counter
        fps_frame_count++;
        Uint32 current_time = SDL_GetTicks();
        Uint32 elapsed = current_time - fps_last_time;
        
        if (elapsed >= 500) {
            fps_current = (fps_frame_count * 1000.0) / elapsed;
            fps_frame_count = 0;
            fps_last_time = current_time;
            
            char title[128];
            snprintf(title, sizeof(title), "Elasto Mania - FPS: %.1f (Metal)", fps_current);
            SDL_SetWindowTitle(SDLWindow, title);
        }
        return;
    }
#endif

    SDL_BlitSurface(SDLSurfacePaletted, NULL, SDLSurfaceMain, NULL);
    SDL_UpdateWindowSurface(SDLWindow);
}

unsigned char** lockfrontbuffer(void) {
    if (Bufflocked) {
        hiba("lockfrontbuffer-ben Bufflocked!");
    }

    return lockbackbuffer();
}

void unlockfrontbuffer(void) {
    if (!Bufflocked) {
        hiba("unlockbackbuffer-ben !Bufflocked!");
    }

    unlockbackbuffer();
}

ddpal::ddpal(unsigned char* tomb) {
    for (int i = 0; i < 256; i++) {
        pal[i].r = tomb[3 * i] << 2;
        pal[i].g = tomb[3 * i + 1] << 2;
        pal[i].b = tomb[3 * i + 2] << 2;
        pal[i].a = 0xFF;
    }
}

void ddpal::set() {
#ifdef __APPLE__
    if (use_metal) {
        // Update Metal GPU palette texture
        for (int i = 0; i < 256; i++) {
            CurrentPalette[i] = (pal[i].a << 24) | (pal[i].r << 16) | (pal[i].g << 8) | pal[i].b;
        }
        metal_update_palette(CurrentPalette);
    } else
#endif
    {
        SDL_SetPaletteColors(SDLSurfacePaletted->format->palette, pal, 0, 256);
    }
}

#if defined(_WIN64) || !defined(_WIN32)
void mv_check(int skip_key_update) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            exit(0);
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                LeftMouseDown = true;
            }
            if (event.button.button == SDL_BUTTON_RIGHT) {
                RightMouseDown = true;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                LeftMouseDown = false;
            }
            if (event.button.button == SDL_BUTTON_RIGHT) {
                RightMouseDown = false;
            }
            break;
        }
    }

    void mkw_getDIstate();
    mkw_getDIstate();
    if (!skip_key_update) {
        void mkw_setkeydown();
        mkw_setkeydown();
    }
}
#endif

void fill_kb_state(char* buffer) {
    const unsigned char* state = SDL_GetKeyboardState(NULL);
    for (int i = 0; i < MaxKeycode; i++) {
        buffer[i] = state[windows_scancode_table[i]];
    }
}

void hide_cursor() { SDL_ShowCursor(SDL_DISABLE); }
void show_cursor() { SDL_ShowCursor(SDL_ENABLE); }

void getmou(int* x, int* y) { SDL_GetMouseState(x, y); }
void setmou(int x, int y) { SDL_WarpMouseInWindow(NULL, x, y); }

int getbutbmou(void) {
    mv_check();
    return LeftMouseDown;
}

int getbutjmou(void) {
    mv_check();
    return RightMouseDown;
}
