#include "ffmpeg_encoder.h"
#include "pic/pic8.h"
#include "sound/engine.h"
#include <algorithm>
#include <format>

ffmpeg_encoder::ffmpeg_encoder(int width, int height, int fps, const std::string& video_path,
                               const std::vector<std::string>& extra_args,
                               const std::string& audio_path, int total_frames)
    : width_(width),
      height_(height),
      total_frames_(total_frames) {
    std::vector<std::string> argv = {
        "ffmpeg",
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-f",
        "rawvideo",
        "-pixel_format",
        "rgb24",
        "-video_size",
        std::format("{}x{}", width, height),
        "-framerate",
        std::to_string(fps),
        "-i",
        "-",
        // Defaults first: ffmpeg lets a later occurrence of an output option win,
        // so anything the user passed after -- overrides these.
        "-c:v",
        "libx264",
        "-pix_fmt",
        "yuv420p",
        "-crf",
        "18",
    };
    argv.insert(argv.end(), extra_args.begin(), extra_args.end());
    argv.push_back(video_path);

    ffmpeg_ = subprocess::spawn_with_stdin(argv);
    if (!ffmpeg_) {
        fail("Failed to start ffmpeg. Is it installed and on your PATH?");
        return;
    }

    if (!audio_path.empty()) {
        audio_ = fopen(audio_path.c_str(), "wb");
        if (!audio_) {
            fail("Failed to open temporary audio file: " + audio_path);
            return;
        }
    }

    rgb_.resize((size_t)width * height * 3);
}

ffmpeg_encoder::~ffmpeg_encoder() {
    if (audio_) {
        fclose(audio_);
    }
}

void ffmpeg_encoder::fail(const std::string& message) {
    if (error_.empty()) {
        error_ = message;
    }
}

void ffmpeg_encoder::write_frame(pic8& frame, const unsigned char* palette) {
    if (!error_.empty()) {
        return;
    }

    if (frame.get_width() != width_ || frame.get_height() != height_) {
        fail(std::format("Frame size changed from {}x{} to {}x{} mid-render", width_, height_,
                         frame.get_width(), frame.get_height()));
        return;
    }

    // The backbuffer is handed to us upside-down.
    frame.vertical_flip();
    unsigned char* out = rgb_.data();
    for (int y = 0; y < height_; y++) {
        const unsigned char* row = frame.get_row(y);
        for (int x = 0; x < width_; x++) {
            const unsigned char* color = &palette[3 * (size_t)row[x]];
            *out++ = color[0];
            *out++ = color[1];
            *out++ = color[2];
        }
    }
    frame.vertical_flip();

    if (!ffmpeg_->write(rgb_.data(), rgb_.size())) {
        fail("ffmpeg closed the pipe before the render finished");
        return;
    }

    frames_written_++;
    if (total_frames_ > 0) {
        // The total is estimated from the replay length, so keep it honest if
        // the render turns out to be a frame or two longer.
        total_frames_ = std::max(frames_written_, total_frames_);
        int percent = (int)((long long)frames_written_ * 100 / total_frames_);
        printf("\rframe %d/%d (%d%%)", frames_written_, total_frames_, percent);
    } else {
        printf("\rframe %d", frames_written_);
    }
    fflush(stdout);
}

void ffmpeg_encoder::write_audio(int sample_count) {
    if (!audio_ || !error_.empty() || sample_count <= 0) {
        return;
    }

    samples_.resize((size_t)sample_count);
    sound_mixer(samples_.data(), sample_count);

    if (fwrite(samples_.data(), sizeof(short), (size_t)sample_count, audio_) !=
        (size_t)sample_count) {
        fail("Failed to write to the temporary audio file");
    }
}

int ffmpeg_encoder::finish() {
    if (audio_) {
        fclose(audio_);
        audio_ = nullptr;
    }

    if (frames_written_ > 0) {
        printf("\n");
    }

    if (!ffmpeg_) {
        return -1;
    }
    return ffmpeg_->close_and_wait();
}
