#include "main.h"
#include "platform_utils.h"
#include "qopen.h"

#include <cstdio>
#include <cstring>

#include "esp_log.h"
static const char* TAG_Q = "qopen";

struct res_file {
    char filename[16];
    int length;
    int offset;
};

constexpr int RES_MAGIC_NUMBER = 1347839;
constexpr int RES_MAX_FILES_LEGACY = 150;
constexpr int RES_MAX_FILES = 3000;

static res_file* ResFiles = nullptr;
static int ResMaxFiles;
static int FileCount = 0;

extern const uint8_t elma_res_start[] asm("_binary_Elma_res_start");
extern const uint8_t elma_res_end[] asm("_binary_Elma_res_end");

static void decrypt() {
    short a = 23;
    short b = 9982;
    short c = 3391;

    if (!ResFiles) {
        internal_error("ResFiles is NULL!");
    }
    for (int i = 0; i < ResMaxFiles; i++) {
        unsigned char* pc = (unsigned char*)(&ResFiles[i]);
        for (size_t j = 0; j < sizeof(res_file); j++) {
            pc[j] ^= a;
            a %= c;
            b += a * c;
            a = 31 * b + c;
        }
    }
}

static bool QOpenInitialized = false;

static bool magic_number_at(const uint8_t* data, int offset) {
    int pos = sizeof(FileCount) + sizeof(res_file) * offset;
    int magic_number = 0;
    memcpy(&magic_number, data + pos, sizeof(magic_number));
    return magic_number == RES_MAGIC_NUMBER;
}

void init_qopen() {
    if (QOpenInitialized) {
        internal_error("init_qopen() already called!");
    }
    QOpenInitialized = true;

    const uint8_t* res_data = elma_res_start;

    if (magic_number_at(res_data, RES_MAX_FILES_LEGACY)) {
        ResMaxFiles = RES_MAX_FILES_LEGACY;
    } else if (magic_number_at(res_data, RES_MAX_FILES)) {
        ResMaxFiles = RES_MAX_FILES;
    } else {
        internal_error(".res file is corrupt!");
    }

    memcpy(&FileCount, res_data, sizeof(FileCount));
    if (FileCount <= 0 || FileCount > ResMaxFiles) {
        internal_error("FileCount <= 0 || FileCount > ResMaxFiles!");
    }

    ResFiles = new res_file[ResMaxFiles];
    if (!ResFiles) {
        external_error("init_qopen() out of memory!");
    }
    int size = sizeof(res_file) * ResMaxFiles;
    memcpy(ResFiles, res_data + sizeof(FileCount), size);
    decrypt();

    ESP_LOGI(TAG_Q, "Loaded %d files (max %d)", FileCount, ResMaxFiles);
}

constexpr int MAX_HANDLES = 3;
static FILE* Handles[MAX_HANDLES];
static int HandleOffset[MAX_HANDLES];
static int NumHandles = 0;

FILE* qopen(const char* filename, const char* mode) {
    if (!QOpenInitialized) {
        internal_error("qopen() called before init_qopen()!");
    }
    if (NumHandles == MAX_HANDLES) {
        internal_error("NumHandles == MAX_HANDLES!");
    }
    if (strcmp(mode, "rb") != 0 && strcmp(mode, "r") != 0) {
        internal_error("qopen() mode is not \"rb\" or \"r\"!: ", filename, mode);
    }

    for (int i = 0; i < FileCount; i++) {
        if (strcmpi(filename, ResFiles[i].filename) == 0) {
            const uint8_t* file_data = elma_res_start + ResFiles[i].offset;
            FILE* h = fmemopen((void*)file_data, ResFiles[i].length, "rb");
            if (!h) {
                internal_error("qopen() fmemopen failed for: ", filename);
            }
            Handles[NumHandles] = h;
            HandleOffset[NumHandles] = i;
            NumHandles++;
            return h;
        }
    }
    internal_error("qopen() failed to find file: ", filename);
    return nullptr;
}

void qclose(FILE* h) {
    if (!QOpenInitialized) {
        internal_error("init_qopen() not yet called!");
    }
    if (!NumHandles) {
        internal_error("qclose() no handles open!");
    }
    for (int i = 0; i < NumHandles; i++) {
        if (Handles[i] == h) {
            fclose(h);
            for (int j = i; j < NumHandles - 1; j++) {
                Handles[j] = Handles[j + 1];
                HandleOffset[j] = HandleOffset[j + 1];
            }
            NumHandles--;
            return;
        }
    }
    internal_error("qclose() cannot find handle!");
}

int qseek(FILE* h, int offset, int whence) {
    if (whence != SEEK_SET && whence != SEEK_END && whence != SEEK_CUR) {
        internal_error("whence != SEEK_SET && whence != SEEK_END && whence != SEEK_CUR!");
    }
    return fseek(h, offset, whence);
}
