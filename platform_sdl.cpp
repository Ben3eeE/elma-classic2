#include "platform_impl.h"
#include "ALL.H"
#include "editor_dialog.h"
#include "EDITUJ.H"
#include "HANG.H"
#include "HANGHIGH.H"
#include "keys.h"
#include "gl_renderer.h"
#include "vk_renderer.h"
#ifdef __APPLE__
#include "metal_renderer.h"
#endif
#ifdef _WIN32
#include "dx_renderer.h"
#endif
#include "main.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include "scancodes_windows.h"

SDL_Window* SDLWindow;
SDL_Surface* SDLSurfaceMain;
SDL_Surface* SDLSurfacePaletted;

enum RendererType {
    RENDERER_SOFTWARE,
    RENDERER_OPENGL,
    RENDERER_VULKAN,
    RENDERER_METAL,
    RENDERER_DIRECTX
};

#ifdef _WIN32
static RendererType renderer_type = RENDERER_DIRECTX;
#elif __APPLE__
static RendererType renderer_type = RENDERER_METAL;
#else
static RendererType renderer_type = RENDERER_OPENGL;
#endif

static Uint32 fps_frame_count = 0;
static Uint32 fps_last_time = 0;
static double fps_current = 0.0;

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
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        internal_error(SDL_GetError());
        return;
    }

    int window_flags = 0;
    
    if (renderer_type == RENDERER_OPENGL) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        window_flags = SDL_WINDOW_OPENGL;
    } else if (renderer_type == RENDERER_VULKAN) {
        window_flags = SDL_WINDOW_VULKAN;
    }

    SDLWindow = SDL_CreateWindow("Elasto Mania", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 SCREEN_WIDTH, SCREEN_HEIGHT, window_flags);
    if (!SDLWindow) {
        internal_error(SDL_GetError());
        return;
    }

    SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
    SDL_EventState(SDL_DROPTEXT, SDL_DISABLE);

    if (renderer_type == RENDERER_OPENGL) {
        if (gl_init(SDLWindow, SCREEN_WIDTH, SCREEN_HEIGHT) != 0) {
            internal_error("Failed to initialize OpenGL renderer");
            return;
        }

        SDLSurfacePaletted = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0,
                                                            SDL_PIXELFORMAT_INDEX8);
        if (!SDLSurfacePaletted) {
            internal_error(SDL_GetError());
            return;
        }
        SDLSurfaceMain = nullptr; // Not used in GL mode
    } else if (renderer_type == RENDERER_VULKAN) {
        if (vk_init(SDLWindow, SCREEN_WIDTH, SCREEN_HEIGHT) != 0) {
            internal_error("Failed to initialize Vulkan renderer");
            return;
        }

        SDLSurfacePaletted = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0,
                                                            SDL_PIXELFORMAT_INDEX8);
        if (!SDLSurfacePaletted) {
            internal_error(SDL_GetError());
            return;
        }
        SDLSurfaceMain = nullptr; // Not used in Vulkan mode
    } else if (renderer_type == RENDERER_METAL) {
#ifdef __APPLE__
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (!SDL_GetWindowWMInfo(SDLWindow, &wmInfo)) {
            internal_error("Failed to get window info");
            return;
        }
        
        void* nsWindow = (void*)wmInfo.info.cocoa.window;
        if (metal_init(nsWindow, SCREEN_WIDTH, SCREEN_HEIGHT) != 0) {
            internal_error("Failed to initialize Metal renderer");
            return;
        }

        SDLSurfacePaletted = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0,
                                                            SDL_PIXELFORMAT_INDEX8);
        if (!SDLSurfacePaletted) {
            internal_error(SDL_GetError());
            return;
        }
        SDLSurfaceMain = nullptr; // Not used in Metal mode
#else
        internal_error("Metal renderer is only available on macOS");
        return;
#endif
    } else if (renderer_type == RENDERER_DIRECTX) {
#ifdef _WIN32
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (!SDL_GetWindowWMInfo(SDLWindow, &wmInfo)) {
            internal_error("Failed to get window info");
            return;
        }
        
        HWND hwnd = wmInfo.info.win.window;
        if (dx_init(hwnd, SCREEN_WIDTH, SCREEN_HEIGHT) != 0) {
            internal_error("Failed to initialize DirectX renderer");
            return;
        }

        SDLSurfacePaletted = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0,
                                                            SDL_PIXELFORMAT_INDEX8);
        if (!SDLSurfacePaletted) {
            internal_error(SDL_GetError());
            return;
        }
        SDLSurfaceMain = nullptr; // Not used in DirectX mode
#else
        internal_error("DirectX renderer is only available on Windows");
        return;
#endif
    } else {
        SDLSurfaceMain = SDL_GetWindowSurface(SDLWindow);
        if (!SDLSurfaceMain) {
            internal_error(SDL_GetError());
            return;
        }
        SDLSurfacePaletted = SDL_CreateRGBSurfaceWithFormat(0, SDLSurfaceMain->w, SDLSurfaceMain->h,
                                                            0, SDL_PIXELFORMAT_INDEX8);
        if (!SDLSurfacePaletted) {
            internal_error(SDL_GetError());
            return;
        }
    }

    fps_last_time = SDL_GetTicks();
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

    if (renderer_type == RENDERER_OPENGL) {
        gl_upload_frame((unsigned char*)SDLSurfacePaletted->pixels, SDLSurfacePaletted->pitch);
        gl_present();
        SDL_GL_SwapWindow(SDLWindow);
    } else if (renderer_type == RENDERER_VULKAN) {
        vk_upload_frame((unsigned char*)SDLSurfacePaletted->pixels, SDLSurfacePaletted->pitch);
        vk_present();
    } else if (renderer_type == RENDERER_METAL) {
#ifdef __APPLE__
        metal_upload_frame((unsigned char*)SDLSurfacePaletted->pixels, SDLSurfacePaletted->pitch);
        metal_present();
#endif
    } else if (renderer_type == RENDERER_DIRECTX) {
#ifdef _WIN32
        dx_upload_frame((unsigned char*)SDLSurfacePaletted->pixels, SDLSurfacePaletted->pitch);
        dx_present();
#endif
    } else {
        SDL_BlitSurface(SDLSurfacePaletted, NULL, SDLSurfaceMain, NULL);
        SDL_UpdateWindowSurface(SDLWindow);
    }

    fps_frame_count++;
    Uint32 current_time = SDL_GetTicks();
    Uint32 elapsed = current_time - fps_last_time;

    if (elapsed >= 500) {
        fps_current = (fps_frame_count * 1000.0) / elapsed;
        fps_frame_count = 0;
        fps_last_time = current_time;

        char title[128];
        snprintf(title, sizeof(title), "Elasto Mania - FPS: %.1f", fps_current);
        SDL_SetWindowTitle(SDLWindow, title);
    }
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
    if (renderer_type == RENDERER_OPENGL) {
        Uint32 CurrentPalette[256];
        // Update OpenGL GPU palette texture
        for (int i = 0; i < 256; i++) {
            CurrentPalette[i] = ((SDL_Color*)data)[i].a << 24 | ((SDL_Color*)data)[i].r << 16 |
                                ((SDL_Color*)data)[i].g << 8 | ((SDL_Color*)data)[i].b;
        }
        gl_update_palette(CurrentPalette);
    } else if (renderer_type == RENDERER_VULKAN) {
        Uint32 CurrentPalette[256];
        // Update Vulkan GPU palette texture
        for (int i = 0; i < 256; i++) {
            CurrentPalette[i] = ((SDL_Color*)data)[i].a << 24 | ((SDL_Color*)data)[i].r << 16 |
                                ((SDL_Color*)data)[i].g << 8 | ((SDL_Color*)data)[i].b;
        }
        vk_update_palette(CurrentPalette);
    } else if (renderer_type == RENDERER_METAL) {
#ifdef __APPLE__
        Uint32 CurrentPalette[256];
        // Update Metal GPU palette texture
        for (int i = 0; i < 256; i++) {
            CurrentPalette[i] = ((SDL_Color*)data)[i].a << 24 | ((SDL_Color*)data)[i].r << 16 |
                                ((SDL_Color*)data)[i].g << 8 | ((SDL_Color*)data)[i].b;
        }
        metal_update_palette(CurrentPalette);
#endif
    } else if (renderer_type == RENDERER_DIRECTX) {
#ifdef _WIN32
        Uint32 CurrentPalette[256];
        // Update DirectX GPU palette texture
        for (int i = 0; i < 256; i++) {
            CurrentPalette[i] = ((SDL_Color*)data)[i].a << 24 | ((SDL_Color*)data)[i].r << 16 |
                                ((SDL_Color*)data)[i].g << 8 | ((SDL_Color*)data)[i].b;
        }
        dx_update_palette(CurrentPalette);
#endif
    } else {
        SDL_SetPaletteColors(SDLSurfacePaletted->format->palette, (const SDL_Color*)data, 0, 256);
    }
}

void handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            // Exit request probably sent by user to terminate program
            if (InEditor && Valtozott) {
                // Disallow exiting if unsaved changes in editor
                break;
            }
            quit();
            break;
        case SDL_WINDOWEVENT:
            // Force editor redraw if focus gained/lost to fix editor sometimes blanking
            switch (event.window.event) {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                invalidateegesz();
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                invalidateegesz();
                break;
            }
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

    update_key_state();
    update_keypress_buffer();
}

void fill_key_state(char* buffer) {
    const unsigned char* state = SDL_GetKeyboardState(NULL);
    for (int i = 0; i < MaxKeycode; i++) {
        buffer[i] = state[windows_scancode_table[i]];
    }
}

void hide_cursor() { SDL_ShowCursor(SDL_DISABLE); }
void show_cursor() { SDL_ShowCursor(SDL_ENABLE); }

void get_mouse_position(int* x, int* y) { SDL_GetMouseState(x, y); }
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
    Uint32 flags = SDL_GetWindowFlags(SDLWindow);
    return flags & SDL_WINDOW_FULLSCREEN;
}

static SDL_AudioDeviceID SDLAudioDevice;
static bool SoundInitialized = false;

static void audio_callback(void* udata, Uint8* stream, int len) {
    callbackhang((short*)stream, len / 2);
}

void init_sound() {
    if (SoundInitialized) {
        internal_error("Sound already initialized!");
    }
    SoundInitialized = true;

    SDL_AudioSpec desired_spec;
    memset(&desired_spec, 0, sizeof(desired_spec));
    desired_spec.callback = audio_callback;
    desired_spec.freq = 11025;
    desired_spec.channels = 1;
    desired_spec.samples = 512;
    desired_spec.format = AUDIO_S16LSB;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        internal_error("Failed to initialize audio subsystem", SDL_GetError());
    }
    SDL_AudioSpec obtained_spec;
    SDLAudioDevice = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &obtained_spec, 0);
    if (SDLAudioDevice == 0) {
        internal_error("Failed to open audio device", SDL_GetError());
    }
    if (obtained_spec.format != desired_spec.format) {
        internal_error("Failed to get correct audio format");
    }
    Hangenabled = 1;
    SDL_PauseAudioDevice(SDLAudioDevice, 0);
}
