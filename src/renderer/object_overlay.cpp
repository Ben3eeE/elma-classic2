#include "renderer/object_overlay.h"
#include "anim.h"
#include "eol_settings.h"
#include "main.h"
#include "pic8.h"

// Gravity arrow sprites indexed by object::Property (1=Up, 2=Down, 3=Left, 4=Right)
constexpr int GRAVITY_ARROW_COUNT = 4;
static pic8* GravityArrows[GRAVITY_ARROW_COUNT] = {};

// Gravity multiplier overlay sprites (0=Double, 1=Half, 2=None, 3=Normal)
constexpr int GRAVITY_OVERLAY_COUNT = 4;
static pic8* GravityOverlays[GRAVITY_OVERLAY_COUNT] = {};

static_assert((int)object::Property::GravityUp == 1);
static_assert((int)object::Property::GravityDown == 2);
static_assert((int)object::Property::GravityLeft == 3);
static_assert((int)object::Property::GravityRight == 4);
static_assert((int)object::Property::GravityDouble == 5);
static_assert((int)object::Property::GravityHalf == 6);
static_assert((int)object::Property::GravityNone == 7);
static_assert((int)object::Property::GravityNormal == 8);

static pic8* load_arrow_bmp() { return pic8::from_bmp("resources/gravarrow.bmp"); }

static void free_gravity_arrows() {
    for (int i = 0; i < GRAVITY_ARROW_COUNT; i++) {
        delete GravityArrows[i];
        GravityArrows[i] = nullptr;
    }
}

void init_gravity_arrows() {
    free_gravity_arrows();

    pic8* arrow_down = load_arrow_bmp();
    if (!arrow_down) {
        return;
    }

    unsigned char transparency_color = arrow_down->gpixel(0, 0);

    int target_height = (int)(ANIM_WIDTH * EolSettings->zoom());
    arrow_down = pic8::resize(arrow_down, target_height);
    pic8* arrow_up = arrow_down->clone();
    arrow_up->vertical_flip();

    // Screen is rendered upside-down, so up/down arrows are swapped.
    // Left/right are derived by transposing the vertical arrows.
    pic8* arrows[] = {arrow_down, arrow_up, pic8::transpose(arrow_up), pic8::transpose(arrow_down)};
    for (int i = 0; i < GRAVITY_ARROW_COUNT; i++) {
        arrows[i]->add_transparency(transparency_color);
        GravityArrows[i] = arrows[i];
    }
}

void draw_gravity_arrow(pic8* pic, int obj_i, int obj_j, object::Property property) {
    if (property < object::Property::GravityUp || property > object::Property::GravityRight) {
        internal_error("unknown property in draw_gravity_arrow()!");
    }
    pic8* arrow = GravityArrows[(int)property - 1];
    if (!arrow) {
        return;
    }

    int obj_half_size = (int)(ANIM_WIDTH * EolSettings->zoom()) / 2;
    int arrow_i = obj_i + obj_half_size - arrow->get_width() / 2;
    int arrow_j = obj_j + obj_half_size - arrow->get_height() / 2;

    constexpr unsigned char FONT_PALETTE_INDEX = 0x19;
    blit8_recolor(pic, arrow, arrow_i, arrow_j, FONT_PALETTE_INDEX);
}

static const char* GRAVITY_OVERLAY_BMPS[GRAVITY_OVERLAY_COUNT] = {
    "resources/grav2x.bmp",
    "resources/gravhalf.bmp",
    "resources/grav0.bmp",
    "resources/gravnormal.bmp",
};

static void free_gravity_overlays() {
    for (int i = 0; i < GRAVITY_OVERLAY_COUNT; i++) {
        delete GravityOverlays[i];
        GravityOverlays[i] = nullptr;
    }
}

void init_gravity_overlays() {
    free_gravity_overlays();

    int target_height = (int)(ANIM_WIDTH * EolSettings->zoom());
    for (int i = 0; i < GRAVITY_OVERLAY_COUNT; i++) {
        pic8* overlay = pic8::from_bmp(GRAVITY_OVERLAY_BMPS[i]);
        if (!overlay) {
            continue;
        }
        unsigned char transparency_color = overlay->gpixel(0, 0);
        overlay = pic8::resize(overlay, target_height);
        overlay->add_transparency(transparency_color);
        GravityOverlays[i] = overlay;
    }
}

void draw_gravity_overlay(pic8* pic, int obj_i, int obj_j, object::Property property) {
    if (property < object::Property::GravityDouble || property > object::Property::GravityNormal) {
        internal_error("unknown property in draw_gravity_overlay()!");
    }
    int index = (int)property - (int)object::Property::GravityDouble;
    pic8* overlay = GravityOverlays[index];
    if (!overlay) {
        return;
    }

    int obj_half_size = (int)(ANIM_WIDTH * EolSettings->zoom()) / 2;
    int overlay_i = obj_i + obj_half_size - overlay->get_width() / 2;
    int overlay_j = obj_j + obj_half_size - overlay->get_height() / 2;

    constexpr unsigned char FONT_PALETTE_INDEX = 0x19;
    blit8_recolor(pic, overlay, overlay_i, overlay_j, FONT_PALETTE_INDEX);
}
