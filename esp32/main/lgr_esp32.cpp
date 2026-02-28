#include "lgr.h"
#include "main.h"
#include "platform_utils.h"

#include <cstdio>
#include <cstring>

extern const uint8_t default_lgr_start[] asm("_binary_Default_lgr_start");
extern const uint8_t default_lgr_end[] asm("_binary_Default_lgr_end");

extern "C" FILE* __real_fopen(const char* path, const char* mode);

// Intercept fopen for LGR file access, redirect to flash-embedded data
extern "C" FILE* __wrap_fopen(const char* path, const char* mode) {
    if (path && strstr(path, "lgr/") == path && strstr(path, ".lgr")) {
        size_t size = default_lgr_end - default_lgr_start;
        return fmemopen((void*)default_lgr_start, size, "rb");
    }
    return __real_fopen(path, mode);
}

lgrfile* Lgr = nullptr;
static char CurrentLgrName[30] = "";

bike_box BikeBox1 = {3, 36, 147, 184};
bike_box BikeBox2 = {32, 183, 147, 297};
bike_box BikeBox3 = {146, 141, 273, 264};
bike_box BikeBox4 = {272, 181, 353, 244};

void invalidate_lgr_cache() { CurrentLgrName[0] = '\0'; }

bool lgrfile::try_load_lgr(const char* name, const char* desc) {
    (void)desc;

    if (strcmpi(name, CurrentLgrName) == 0 && Lgr != nullptr) {
        return true;
    }

    strcpy(CurrentLgrName, name);
    delete Lgr;
    Lgr = new lgrfile("default");
    return true;
}

void lgrfile::load_lgr_file(const char* lgr_name) {
    (void)lgr_name;
    if (!try_load_lgr("default", nullptr)) {
        internal_error("Failed to load embedded LGR");
    }
}
