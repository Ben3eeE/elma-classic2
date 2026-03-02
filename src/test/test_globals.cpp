#include "menu/best_times.h"
#include "canvas.h"
#include "editor_canvas.h"
#include "editor_dialog.h"
#include "editor_topology.h"
#include "editor/window.h"
#include "EDITUJ.H"
#include "eol/console.h"
#include "eol/eol.h"
#include "eol_settings.h"
#include "KIRAJZOL.H"
#include "LEJATSZO.H"
#include "level_load.h"
#include "lgr.h"
#include "M_PIC.H"
#include "main.h"
#include "menu/pic.h"
#include "menu/rec_list.h"
#include "renderer/object_overlay.h"
#include "state.h"
#include <cstdio>
#include <cstdlib>
#include <string>

// main.cpp
eol_settings* EolSettings = new eol_settings();
bool ErrorGraphicsLoaded = false;

void internal_error(const std::string& message) {
    fprintf(stderr, "Internal error: %s\n", message.c_str());
    abort();
}

void external_error(const std::string& message) {
    fprintf(stderr, "External error: %s\n", message.c_str());
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

// editor/window.cpp
bool ShowPolygons = true;
bool ShowGrassPolygons = true;
bool ShowObjects = true;

// KIRAJ320.CPP
int GameBackgroundRender = 0;

void init_renderer() {}
void increase_view_size() {}
void decrease_view_size() {}
void render_game(double, valtozok*, valtozok*, bool, bool, bool, bool, camera&) {}

// renderer/object_overlay.cpp
void init_gravity_arrows() {}

// canvas.cpp
canvas* CanvasBack = nullptr;
bool canvas::bike_out_of_bounds(vect2) { return false; }

// eol/console.cpp — Console must be a valid pointer because belsoresz/kulsoresz
// route input checks through is_game_key_down(), which dereferences Console.
console* Console = new console();
bool console::is_input_active() const { return false; }
void console::toggle_active() {}
void console::deactivate_input() {}
void console::handle_input() {}

// eol/eol.cpp — EolClient is only dereferenced from lejatszo()/lejatszo_r(),
// which the test harness does not call, so a null pointer is fine.
eol* EolClient = nullptr;
void eol::set_table(TableType) {}
void eol::enter_level(const char*, const level*) {}
void eol::exit_level(const char*, double, int, int, bool) {}

// menu/rec_list.cpp
void rec_list::add_new_replay(const std::string&, int) {}

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

// level_load.cpp
void invalidate_level() {}
