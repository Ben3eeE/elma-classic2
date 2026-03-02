#include "best_times.h"
#include "editor_canvas.h"
#include "editor_dialog.h"
#include "editor_topology.h"
#include "EDITUJ.H"
#include "EDITTOLT.H"
#include "eol_settings.h"
#include "KIRAJZOL.H"
#include "LEJATSZO.H"
#include "level_load.h"
#include "lgr.h"
#include "M_PIC.H"
#include "main.h"
#include "menu_pic.h"
#include "state.h"
#include <cstdio>
#include <cstdlib>

// main.cpp
eol_settings* EolSettings = new eol_settings();
bool ErrorGraphicsLoaded = false;

int random_range(int maximum) {
    if (maximum <= 0) {
        return 0;
    }
    return rand() % maximum;
}

void internal_error(const char* text1, const char* text2, const char* text3) {
    fprintf(stderr, "Internal error: %s", text1);
    if (text2) {
        fprintf(stderr, " %s", text2);
    }
    if (text3) {
        fprintf(stderr, " %s", text3);
    }
    fprintf(stderr, "\n");
    abort();
}

void external_error(const char* text1, const char* text2, const char* text3) {
    fprintf(stderr, "External error: %s", text1);
    if (text2) {
        fprintf(stderr, " %s", text2);
    }
    if (text3) {
        fprintf(stderr, " %s", text3);
    }
    fprintf(stderr, "\n");
    abort();
}

double stopwatch() { return 0.0; }
void stopwatch_reset() {}
void delay(int) {}

// D_PIC.CPP
int SCREEN_WIDTH = 640;
int SCREEN_HEIGHT = 480;

// EDITUJ.CPP
level* Ptop = nullptr;

// state.cpp
state* State = nullptr;

// lgr.cpp
lgrfile* Lgr = nullptr;
void invalidate_lgr_cache() {}
int lgrfile::get_picture_index(const char*) { return -1; }
int lgrfile::get_mask_index(const char*) { return -1; }
int lgrfile::get_texture_index(const char*) { return -1; }

// menu_pic.cpp
palette* MenuPalette = nullptr;

// KIRAJ320.CPP
int Kitoltestmegrak = 0;
int Kitoltestmegrakkezd = 0;
int Voltkilogas = 0;
double Hatarx1 = 0, Hatarx2 = 0, Hatary1 = 0, Hatary2 = 0;
int Cxsize = 0, Cysize = 0;

void init_renderer() {}
void novelkepmeret() {}
void csokkentkepmeret() {}
void kirajzol320(double, valtozok*, valtozok*, int, int, int, int, camera&) {}

// editor_dialog.cpp
int dialog(const char*, const char*, const char*, const char*, const char*, const char*,
           const char*, const char*, const char*, const char*, const char*, const char*,
           const char*, const char*, const char*, const char*, const char*, const char*,
           const char*, const char*, const char*, const char*, const char*, const char*) {
    return 0;
}

// editor_canvas.cpp
void render_line(vect2, vect2, bool) {}
double max_grab_distance() { return 1000000.0; }

// EDITTOLT.CPP
int Rajzolpoligon = 1;
int Rajzolkoveto = 1;
int Rajzolkepek = 1;

// editor_topology.cpp
bool check_topology(bool) { return false; }

// best_times.cpp
void centiseconds_to_string(int, char* text, bool, bool) { text[0] = 0; }

// level_load.cpp
void invalidate_level() {}
