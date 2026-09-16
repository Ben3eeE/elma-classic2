#include "editor/editor.h"
#include "eol/settings.h"
#include "ffmpeg_encoder.h"
#include "game/game.h"
#include "game/level_load.h"
#include "game/recorder.h"
#include "game/state.h"
#include "game/video_export.h"
#include "level/level.h"
#include "pic/surface.h"
#include "platform/implementation.h"
#include "platform/process.h"
#include "runtime.h"
#include "sound/engine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr int EXIT_USAGE = 1;
constexpr int EXIT_FFMPEG = 2;

// Replays store one frame every 1/30th of a second
constexpr double REPLAY_FRAMES_PER_SECOND = 30.0;

struct options {
    std::string input;
    std::string output;
    bool audio = true;
    std::vector<std::string> ffmpeg_args;
};

void print_usage() {
    printf("Usage: rec2vid <input.rec> <output-file> [options] [-- <ffmpeg args>...]\n"
           "\n"
           "Renders a replay to a video file using ffmpeg, which must be on your PATH.\n"
           "\n"
           "Options:\n"
           "  --no-audio     render video only\n"
           "  -h, --help     show this help\n"
           "\n"
           "Resolution, zoom and frame rate come from settings.json. Point\n"
           "EOL_SETTINGS_OVERRIDES at a json file to override them for one render.\n"
           "Arguments after -- are passed to ffmpeg and override the built-in\n"
           "encoding defaults (-c:v libx264 -pix_fmt yuv420p -crf 18).\n"
           "\n"
           "Example:\n"
           "  rec2vid rec/blabla.rec out/video.avi -- -c:v libx264 -crf 22\n");
}

bool parse_arguments(int argc, char** argv, options& opts) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--") {
            for (int j = i + 1; j < argc; j++) {
                opts.ffmpeg_args.emplace_back(argv[j]);
            }
            return true;
        }

        if (arg == "-h" || arg == "--help") {
            print_usage();
            exit(0);
        } else if (arg == "--no-audio") {
            opts.audio = false;
        } else if (arg.starts_with("-")) {
            fprintf(stderr, "rec2vid: unknown option: %s\n", arg.c_str());
            return false;
        } else if (opts.input.empty()) {
            opts.input = arg;
        } else if (opts.output.empty()) {
            opts.output = arg;
        } else {
            fprintf(stderr, "rec2vid: unexpected argument: %s\n", arg.c_str());
            return false;
        }
    }

    return true;
}

// Accepts both a path as given and a name relative to the rec/ folder.
std::optional<std::string> resolve_replay(const std::string& input) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(input, ec)) {
        return input;
    }

    std::filesystem::path in_rec_folder = std::filesystem::path("rec") / input;
    if (std::filesystem::is_regular_file(in_rec_folder, ec)) {
        return in_rec_folder.string();
    }

    return std::nullopt;
}

int run_ffmpeg(const std::vector<std::string>& argv) {
    std::unique_ptr<subprocess> process = subprocess::spawn_with_stdin(argv);
    if (!process) {
        fprintf(stderr, "rec2vid: failed to start ffmpeg. Is it installed and on your PATH?\n");
        return -1;
    }
    return process->close_and_wait();
}

} // namespace

int main(int argc, char** argv) {
    options opts;
    if (!parse_arguments(argc, argv, opts)) {
        return EXIT_USAGE;
    }

    if (opts.input.empty() || opts.output.empty()) {
        print_usage();
        return EXIT_USAGE;
    }

    std::filesystem::path output(opts.output);
    if (output.extension().empty()) {
        fprintf(stderr, "rec2vid: the output file needs an extension, e.g. .avi or .mp4\n");
        return EXIT_USAGE;
    }

    runtime::init_settings();
    platform_init();
    runtime::init_data();

    // The sound mixer is pulled directly instead of through an audio device
    State->sound_on = 1;

    std::optional<std::string> replay_path = resolve_replay(opts.input);
    if (!replay_path) {
        fprintf(stderr, "rec2vid: replay not found: %s\n", opts.input.c_str());
        return EXIT_USAGE;
    }

    int level_id = recorder::load_rec_path(*replay_path);

    if (!level_file_exists(Rec1->level_filename)) {
        fprintf(stderr, "rec2vid: cannot find the level for this replay: %s\n",
                Rec1->level_filename);
        return EXIT_USAGE;
    }
    load_level_play(Rec1->level_filename);
    if (Level->level_id != level_id) {
        fprintf(stderr, "rec2vid: %s has changed since %s was recorded\n", Rec1->level_filename,
                replay_path->c_str());
        return EXIT_USAGE;
    }

    Rec1->rewind();
    Rec2->rewind();

    std::error_code ec;
    if (output.has_parent_path()) {
        std::filesystem::create_directories(output.parent_path(), ec);
        if (ec) {
            fprintf(stderr, "rec2vid: failed to create %s: %s\n",
                    output.parent_path().string().c_str(), ec.message().c_str());
            return EXIT_USAGE;
        }
    }

    // With audio the video is encoded to a temporary file first and muxed with
    // the samples afterwards, because ffmpeg can only read one pipe portably.
    std::filesystem::path temp_base =
        output.parent_path() / (output.stem().string() + ".rec2vid-tmp");
    std::filesystem::path temp_video = temp_base;
    temp_video += output.extension();
    std::filesystem::path temp_audio = temp_base;
    temp_audio += ".pcm";

    const int fps = EolSettings->recording_fps();
    const std::string video_target = opts.audio ? temp_video.string() : output.string();

    int replay_frames = Rec1->frame_count();
    if (MultiplayerRec && Rec2->frame_count() > replay_frames) {
        replay_frames = Rec2->frame_count();
    }
    // Frames are rendered at times 0, 1/fps, 2/fps ... up to the end of the
    // replay. Only an estimate, used for the progress display.
    int total_frames = (int)std::floor(replay_frames / REPLAY_FRAMES_PER_SECOND * fps) + 1;
    if (EolSettings->pause_replay_for_1s()) {
        total_frames += fps;
    }

    printf("Rendering %s to %s at %dx%d, %d fps%s\n", replay_path->c_str(), output.string().c_str(),
           SCREEN_WIDTH, SCREEN_HEIGHT, fps, opts.audio ? "" : ", no audio");
    fflush(stdout);

    ffmpeg_encoder encoder(SCREEN_WIDTH, SCREEN_HEIGHT, fps, video_target, opts.ffmpeg_args,
                           opts.audio ? temp_audio.string() : std::string(), total_frames);
    if (!encoder.error().empty()) {
        fprintf(stderr, "rec2vid: %s\n", encoder.error().c_str());
        return EXIT_FFMPEG;
    }

    VideoEncoder = &encoder;
    bool completed = render_replay(Rec1->level_filename);
    VideoEncoder = nullptr;

    int ffmpeg_status = encoder.finish();

    if (!completed && encoder.error().empty()) {
        printf("Aborted - writing what was rendered so far.\n");
    }

    auto remove_temporaries = [&]() {
        std::error_code remove_ec;
        std::filesystem::remove(temp_video, remove_ec);
        std::filesystem::remove(temp_audio, remove_ec);
    };

    if (!encoder.error().empty()) {
        fprintf(stderr, "rec2vid: %s\n", encoder.error().c_str());
        remove_temporaries();
        return EXIT_FFMPEG;
    }
    if (ffmpeg_status != 0) {
        fprintf(stderr, "rec2vid: ffmpeg exited with status %d\n", ffmpeg_status);
        remove_temporaries();
        return EXIT_FFMPEG;
    }

    if (opts.audio) {
        printf("Muxing audio...\n");
        int mux_status = run_ffmpeg({
            "ffmpeg",
            "-y",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            temp_video.string(),
            "-f",
            "s16le",
            "-ar",
            std::to_string(SOUND_SAMPLE_RATE),
            "-ac",
            "1",
            "-i",
            temp_audio.string(),
            "-c:v",
            "copy",
            // No -shortest: the two streams are generated to the same length,
            // and it truncates the audio well before the video ends.
            output.string(),
        });

        remove_temporaries();

        if (mux_status != 0) {
            fprintf(stderr, "rec2vid: ffmpeg exited with status %d while muxing audio\n",
                    mux_status);
            return EXIT_FFMPEG;
        }
    }

    printf("Wrote %s\n", output.string().c_str());
    return 0;
}
