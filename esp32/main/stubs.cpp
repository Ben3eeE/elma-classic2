#include "sound_engine.h"
#include "recorder.h"
#include "state.h"
#include "eol_settings.h"
#include "EDITUJ.H"
#include "KIRAJZOL.H"
#include "M_PIC.H"
#include "best_times.h"
#include "editor_dialog.h"
#include "flagtag.h"
#include "fs_utils.h"
#include "JATEKOS.H"
#include "keys.h"
#include "level.h"
#include "level_load.h"
#include "lgr.h"
#include "main.h"
#include "menu_intro.h"
#include "menu_pic.h"
#include "physics_init.h"
#include "pic8.h"
#include "platform_impl.h"
#include "platform_utils.h"
#include "skip.h"

#include <cstdio>
#include <cstring>
#include <directinput/scancodes.h>

// ---- Sound ----

bool Mute = true;

void sound_engine_init() {}
void start_motor_sound(bool) {}
void stop_motor_sound(bool) {}
void set_motor_frequency(bool, double, int) {}
void set_friction_volume(double) {}
void start_wav(WavEvent, double) {}
void sound_mixer(short*, int) {}

// ---- Recorder ----

recorder* Rec1 = nullptr;
recorder* Rec2 = nullptr;
int MultiplayerRec = 0;

recorder::recorder() : frame_count_(0), event_count(0), finished(false), flagtag_(0) {
    previous_frame_time = 0;
    next_frame_time = 0;
    next_frame_index = 0;
    current_event_index = 0;
}

recorder::~recorder() {}

void recorder::erase(const char*) {
    frame_count_ = 0;
    event_count = 0;
    frames.clear();
    events.clear();
}

void recorder::rewind() {
    next_frame_index = 0;
    current_event_index = 0;
    next_frame_time = 0;
    finished = false;
}

bool recorder::recall_frame(motorst*, double, bike_sound*) { return false; }
void recorder::store_frames(motorst*, double, bike_sound*) {}
void recorder::store_event(double, WavEvent, double, int) {}
bool recorder::recall_event(double, WavEvent*, double*, int*) { return false; }
bool recorder::recall_event_reverse(double, WavEvent*, double*, int*) { return false; }
double recorder::find_last_turn_frame_time(double) const { return -1000.0; }
double recorder::find_last_volt_time(double, bool*) const { return -1000.0; }
void recorder::encode_frame_count() {}
bool recorder::frame_count_integrity() { return true; }
int recorder::load_rec_file(const char*, bool) { return 0; }
void recorder::save_rec_file(const char*, int) {}
recorder::merge_result recorder::load_merge(const std::string&, const std::string&) {
    return {0, false, false, false};
}

// ---- Event buffer ----

struct event_entry {
    WavEvent event;
    double volume;
    int object_index;
};

static constexpr int MAX_EVENTS = 64;
static event_entry EventBuffer[MAX_EVENTS];
static int EventBufferCount = 0;
static int EventBufferReadIndex = 0;

void add_event_buffer(WavEvent event, double volume, int object_index) {
    if (EventBufferCount < MAX_EVENTS) {
        EventBuffer[EventBufferCount].event = event;
        EventBuffer[EventBufferCount].volume = volume;
        EventBuffer[EventBufferCount].object_index = object_index;
        EventBufferCount++;
    }
}

void reset_event_buffer() {
    EventBufferCount = 0;
    EventBufferReadIndex = 0;
}

bool get_event_buffer(WavEvent* event, double* volume, int* object_index) {
    if (EventBufferReadIndex >= EventBufferCount) {
        EventBufferCount = 0;
        EventBufferReadIndex = 0;
        return false;
    }
    *event = EventBuffer[EventBufferReadIndex].event;
    *volume = EventBuffer[EventBufferReadIndex].volume;
    *object_index = EventBuffer[EventBufferReadIndex].object_index;
    EventBufferReadIndex++;
    return true;
}

// ---- State ----

state* State = nullptr;

state::state(const char*) {
    memset(this, 0, sizeof(*this));
    player_count = 1;
    strcpy(players[0].name, "Badge");
    players[0].levels_completed = 54;
    players[0].selected_level = 0;
    strcpy(player1, "Badge");
    strcpy(player2, "Badge2");
    sound_on = 0;
    compatibility_mode = 0;
    single = 1;
    flag_tag = 0;
    player1_bike1 = 1;
    high_quality = 1;
    animated_objects = 1;
    animated_menus = 0;

    keys1.gas = DIK_SPACE;
    keys1.brake = DIK_DOWN;
    keys1.right_volt = DIK_RIGHT;
    keys1.left_volt = DIK_LEFT;
    keys1.turn = DIK_RETURN;
    keys1.toggle_minimap = DIK_UNKNOWN;
    keys1.toggle_timer = DIK_UNKNOWN;
    keys1.toggle_visibility = DIK_UNKNOWN;
    keys1.alovolt = DIK_LSHIFT;
    keys1.brake_alias = DIK_UNKNOWN;

    keys2 = keys1;

    key_increase_screen_size = DIK_UNKNOWN;
    key_decrease_screen_size = DIK_UNKNOWN;
    key_screenshot = DIK_UNKNOWN;
    key_escape_alias = DIK_UNKNOWN;
    key_replay_fast_2x = DIK_UNKNOWN;
    key_replay_fast_4x = DIK_UNKNOWN;
    key_replay_fast_8x = DIK_UNKNOWN;
    key_replay_slow_2x = DIK_UNKNOWN;
    key_replay_slow_4x = DIK_UNKNOWN;
    key_replay_pause = DIK_UNKNOWN;
    key_replay_rewind = DIK_UNKNOWN;

    strcpy(editor_filename, "");
    strcpy(external_filename, "");
}

void state::reload_toptens() {}
void state::save() {}
void state::write_stats() {}
void state::reset_keys() {}
player* state::get_player(const char*) { return &players[0]; }

void state::write_stats_player_total_time(FILE*, const char*, bool) {}
void state::write_stats_anonymous_total_time(FILE*, bool, const char*, const char*, const char*) {}

void test_player() {}
void merge_states() {}

// ---- eol_settings ----

eol_settings* EolSettings = nullptr;

template <typename T> Default<T>::operator T() const { return value; }
template <typename T> Default<T>& Default<T>::operator=(T v) {
    value = v;
    return *this;
}
template <typename T> void Default<T>::reset() { value = def; }

template <typename T> Clamp<T>::operator T() const { return value; }
template <typename T> Clamp<T>& Clamp<T>::operator=(T v) {
    value = (v < min) ? min : (v > max ? max : v);
    return *this;
}
template <typename T> void Clamp<T>::reset() { value = def; }

template struct Default<bool>;
template struct Default<MapAlignment>;
template struct Default<RendererType>;
template struct Default<DikScancode>;
template struct Default<std::string>;
template struct Clamp<int>;
template struct Clamp<double>;

void eol_settings::set_screen_width(int) {}
void eol_settings::set_screen_height(int) {}
void eol_settings::set_pictures_in_background(bool b) { pictures_in_background_ = b; }
void eol_settings::set_center_camera(bool) {}
void eol_settings::set_center_map(bool) {}
void eol_settings::set_map_alignment(MapAlignment) {}
void eol_settings::set_renderer(RendererType) {}
void eol_settings::set_zoom(double z) {
    if (z != zoom_) {
        zoom_ = z;
        set_zoom_factor();
    }
}
void eol_settings::set_zoom_textures(bool b) { zoom_textures_ = b; }
void eol_settings::set_turn_time(double) {}
void eol_settings::set_lctrl_search(bool) {}
void eol_settings::set_alovolt_key_player_a(DikScancode) {}
void eol_settings::set_alovolt_key_player_b(DikScancode) {}
void eol_settings::set_brake_alias_key_player_a(DikScancode) {}
void eol_settings::set_brake_alias_key_player_b(DikScancode) {}
void eol_settings::set_escape_alias_key(DikScancode) {}
void eol_settings::set_replay_fast_2x_key(DikScancode) {}
void eol_settings::set_replay_fast_4x_key(DikScancode) {}
void eol_settings::set_replay_fast_8x_key(DikScancode) {}
void eol_settings::set_replay_slow_2x_key(DikScancode) {}
void eol_settings::set_replay_slow_4x_key(DikScancode) {}
void eol_settings::set_replay_pause_key(DikScancode) {}
void eol_settings::set_replay_rewind_key(DikScancode) {}
void eol_settings::set_default_lgr_name(std::string) {}
void eol_settings::set_show_last_apple_time(bool) {}
void eol_settings::set_recording_fps(int) {}
void eol_settings::set_show_demo_menu(bool) {}
void eol_settings::set_show_help_menu(bool) {}
void eol_settings::set_show_best_times_menu(bool) {}
void eol_settings::set_still_objects(bool) {}
void eol_settings::set_all_internals_accessible(bool) {}

void eol_settings::read_settings() {}
void eol_settings::write_settings() {}
void eol_settings::sync_controls_to_state(state*) {}
void eol_settings::sync_controls_from_state(state*) {}

// ---- Level ----

char BestTime[30] = "";

void load_best_time(const char*, int) { BestTime[0] = '\0'; }

// ---- Editor ----

level* Ptop = nullptr;

palette* Pal_editor = nullptr;
abc8 *Pabc1 = nullptr, *Pabc2 = nullptr;
polygon* Pgy = nullptr;
int K = 0;
bool Fel = false;
object* Pker = nullptr;
sprite* Psp = nullptr;
int Valtozott = 0;
int Tool = 0;
unsigned char Feherszin = 255;
unsigned char Hatterindex = 0;
int Moux = 0, Mouy = 0;

void seteditorpal(void) {}
void push(void) {}
void pop(void) {}
void update_and_draw_cursor() {}
void invalidate(void) {}
void invalidateegesz(void) {}
void editor(void) {}
void toolhelp(const char*) {}
void kispritenev(const char*, const char*, const char*, int, Clipping) {}

// ---- Menu ----

pic8* BufferMain = nullptr;
pic8* BufferBall = nullptr;
abc8* MenuFont = nullptr;
palette* MenuPalette = nullptr;
pic8* Intro = nullptr;

void init_menu_pictures() {
    if (!BufferMain) {
        BufferMain = new pic8(SCREEN_WIDTH, SCREEN_HEIGHT);
    }
}
void render_error(const char*, const char*, const char*, const char*) {}

// ---- File system ----

bool find_first(const char*, char*) { return false; }
bool find_next(char*) { return false; }
void find_close() {}

// ---- Flagtag ----

bool FlagTagAHasFlag = false;
bool FlagTagImmunity = false;
bool FlagTagAStarts = true;
double FlagTimeA = 0;
double FlagTimeB = 0;

void flagtag_reset() {}
void flagtag(double) {}
void flagtag_replay(double) {}

// ---- Skip ----

bool is_skippable(int) { return false; }

// ---- Best times ----

void centiseconds_to_string(int time, char* text, bool show_hours, bool compact) {
    if (time < 0) {
        time = 0;
    }
    int centiseconds = time % 100;
    int total_seconds = time / 100;
    int seconds = total_seconds % 60;
    int total_minutes = total_seconds / 60;
    int minutes = total_minutes % 60;
    int hours = total_minutes / 60;

    if (compact) {
        if (hours > 0) {
            sprintf(text, "%d:%02d:%02d:%02d", hours, minutes, seconds, centiseconds);
        } else if (minutes > 0) {
            sprintf(text, "%d:%02d:%02d", minutes, seconds, centiseconds);
        } else {
            sprintf(text, "%d:%02d", seconds, centiseconds);
        }
    } else {
        if (show_hours) {
            sprintf(text, "%02d:%02d:%02d:%02d", hours, minutes, seconds, centiseconds);
        } else {
            sprintf(text, "%02d:%02d:%02d", minutes, seconds, centiseconds);
        }
    }
}

void menu_internal_topten(int, bool) {}
void menu_external_topten(level*, bool) {}
void menu_best_times() {}

// ---- Keys ----

void add_char_to_buffer(char) {}
void add_text_to_buffer(const char*) {}
void empty_keypress_buffer() {}
bool has_text_input() { return false; }
char pop_text_input() { return 0; }

// ---- Misc ----

void newjatekos(int, int) {}
void jatekosvalasztas(int, int) {}
void menu_intro() {}
void invalidate_level() {}
void dialog_warn_lgr_assets_deleted() {}

bool InEditor = false;

int dialog(const char*, const char*, const char*, const char*, const char*, const char*,
           const char*, const char*, const char*, const char*, const char*, const char*,
           const char*, const char*, const char*, const char*, const char*, const char*,
           const char*, const char*, const char*, const char*, const char*, const char*) {
    return 0;
}

bool is_in_box(int, int, box) { return false; }

void render_box(pic8*, int, int, int, int, unsigned char, unsigned char) {}
void render_box(pic8*, box, unsigned char, unsigned char) {}
