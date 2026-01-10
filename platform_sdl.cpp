#include "platform_impl.h"
#include "ALL.H"
#include "editor_dialog.h"
#include "EDITUJ.H"
#include "HANG.H"
#include "HANGHIGH.H"
#include "keys.h"
#include "main.h"
#include <cmath>
#include <SDL3/SDL.h>
#include "scancodes_windows.h"

SDL_Window* SDLWindow;
SDL_Surface* SDLSurfaceMain;
SDL_Surface* SDLSurfacePaletted;

static bool LeftMouseDownPrev = false;
static bool RightMouseDownPrev = false;
static bool LeftMouseDown = false;
static bool RightMouseDown = false;

void message_box(const char* text) {
    // As per docs, can be called even before SDL_Init
    // SDLWindow will either be a handle to the window, or nullptr if no parent
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Message", text, SDLWindow);
}

void platform_init() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        internal_error(SDL_GetError());
        return;
    }

    SDLWindow = SDL_CreateWindow("Elasto Mania", SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!SDLWindow) {
        internal_error(SDL_GetError());
        return;
    }

    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, false);
    SDL_SetEventEnabled(SDL_EVENT_DROP_TEXT, false);

    SDLSurfaceMain = SDL_GetWindowSurface(SDLWindow);
    if (!SDLSurfaceMain) {
        internal_error(SDL_GetError());
        return;
    }
    SDLSurfacePaletted =
        SDL_CreateSurface(SDLSurfaceMain->w, SDLSurfaceMain->h, SDL_PIXELFORMAT_INDEX8);

    SDL_Palette* palette = SDL_CreateSurfacePalette(SDLSurfacePaletted);
    if (!SDLSurfacePaletted || !palette) {
        internal_error(SDL_GetError());
        return;
    }
}

static unsigned char* SurfaceBuffer[SCREEN_HEIGHT];
static bool SurfaceLocked = false;

unsigned char** lock_backbuffer(bool flipped) {
    if (SurfaceLocked) {
        internal_error("lock_backbuffer SurfaceLocked!");
    }
    SurfaceLocked = true;

    unsigned char* row = (unsigned char*)SDLSurfacePaletted->pixels;
    if (flipped) {
        // Set the row buffer bottom-down
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            SurfaceBuffer[SCREEN_HEIGHT - 1 - y] = row;
            row += SDLSurfacePaletted->w;
        }
    } else {
        // Set the row buffer top-down
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            SurfaceBuffer[y] = row;
            row += SDLSurfacePaletted->w;
        }
    }

    return SurfaceBuffer;
}

void unlock_backbuffer() {
    if (!SurfaceLocked) {
        internal_error("unlock_backbuffer !SurfaceLocked!");
    }
    SurfaceLocked = false;

    SDL_BlitSurface(SDLSurfacePaletted, NULL, SDLSurfaceMain, NULL);
    SDL_UpdateWindowSurface(SDLWindow);
}

unsigned char** lock_frontbuffer(bool flipped) {
    if (SurfaceLocked) {
        internal_error("lock_frontbuffer SurfaceLocked!");
    }

    return lock_backbuffer(flipped);
}

void unlock_frontbuffer() {
    if (!SurfaceLocked) {
        internal_error("unlock_frontbuffer !SurfaceLocked!");
    }

    unlock_backbuffer();
}

palette::palette(unsigned char* palette_data) {
    SDL_Color* pal = new SDL_Color[256];
    for (int i = 0; i < 256; i++) {
        pal[i].r = palette_data[3 * i];
        pal[i].g = palette_data[3 * i + 1];
        pal[i].b = palette_data[3 * i + 2];
        pal[i].a = 0xFF;
    }
    data = (void*)pal;
}

palette::~palette() { delete[] (SDL_Color*)data; }

void palette::set() {
    SDL_SetPaletteColors(SDL_GetSurfacePalette(SDLSurfacePaletted), (const SDL_Color*)data, 0, 256);
}

void handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            // Exit request probably sent by user to terminate program
            if (InEditor && Valtozott) {
                // Disallow exiting if unsaved changes in editor
                break;
            }
            quit();
            break;
        // Force editor redraw if focus gained/lost to fix editor sometimes blanking
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            invalidateegesz();
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            invalidateegesz();
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                LeftMouseDown = true;
            }
            if (event.button.button == SDL_BUTTON_RIGHT) {
                RightMouseDown = true;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                LeftMouseDown = false;
            }
            if (event.button.button == SDL_BUTTON_RIGHT) {
                RightMouseDown = false;
            }
            break;
        }
    }

    update_key_state();
    update_keypress_buffer();
}

void fill_key_state(char* buffer) {
    const bool* state = SDL_GetKeyboardState(NULL);
    for (int i = 0; i < MaxKeycode; i++) {
        buffer[i] = state[windows_scancode_table[i]] ? 1 : 0;
    }
}

void hide_cursor() { SDL_HideCursor(); }
void show_cursor() { SDL_ShowCursor(); }

void get_mouse_position(int* x, int* y) {
    float fx, fy;
    SDL_GetMouseState(&fx, &fy);
    *x = std::round(fx);
    *y = std::round(fy);
}
void set_mouse_position(int x, int y) { SDL_WarpMouseInWindow(NULL, x, y); }

bool left_mouse_clicked() {
    handle_events();
    bool click = !LeftMouseDownPrev && LeftMouseDown;
    LeftMouseDownPrev = LeftMouseDown;
    return click;
}

bool right_mouse_clicked() {
    handle_events();
    bool click = !RightMouseDownPrev && RightMouseDown;
    RightMouseDownPrev = RightMouseDown;
    return click;
}

bool is_fullscreen() {
    SDL_WindowFlags flags = SDL_GetWindowFlags(SDLWindow);
    return flags & SDL_WINDOW_FULLSCREEN;
}

static SDL_AudioDeviceID SDLAudioDevice;
static SDL_AudioStream* SDLAudioStreamHandle;
static bool SoundInitialized = false;

static void audio_callback(void* udata, SDL_AudioStream* stream, int additional_amount,
                           int total_amount) {
    if (additional_amount > 0) {
        Uint8* data = (Uint8*)SDL_stack_alloc(Uint8, additional_amount);
        if (data) {
            callbackhang((short*)data, additional_amount / 2);
            SDL_PutAudioStreamData(stream, data, additional_amount);
            SDL_stack_free(data);
        }
    }
}

void init_sound() {
    if (SoundInitialized) {
        internal_error("Sound already initialized!");
    }
    SoundInitialized = true;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        internal_error("Failed to initialize audio subsystem", SDL_GetError());
    }

    const SDL_AudioSpec spec = {SDL_AUDIO_S16, 1, 11025};

    SDLAudioStreamHandle =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, NULL);
    if (!SDLAudioStreamHandle) {
        internal_error("Failed to open audio device stream", SDL_GetError());
    }

    SDLAudioDevice = SDL_GetAudioStreamDevice(SDLAudioStreamHandle);
    if (SDLAudioDevice == 0) {
        internal_error("Failed to get audio device from stream");
    }

    Hangenabled = 1;

    SDL_ResumeAudioDevice(SDLAudioDevice);
}
