#ifndef TOOLS_FFMPEG_ENCODER_H
#define TOOLS_FFMPEG_ENCODER_H

#include "game/video_export.h"
#include "platform/process.h"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// Streams rendered frames to an ffmpeg process as raw rgb24 video, and, when an
// audio path is given, writes the matching mono s16le samples to a file. The
// two are combined by rec2vid in a second ffmpeg pass.
class ffmpeg_encoder : public video_encoder {
  public:
    ffmpeg_encoder(int width, int height, int fps, const std::string& video_path,
                   const std::vector<std::string>& extra_args, const std::string& audio_path,
                   int total_frames);
    ~ffmpeg_encoder() override;

    // Non-empty if the encoder could not be started or a write failed.
    const std::string& error() const { return error_; }

    void write_frame(pic8& frame, const unsigned char* palette) override;
    void write_audio(int sample_count) override;
    bool failed() const override { return !error_.empty(); }

    // Close the pipe and wait for ffmpeg. Returns its exit code, or -1.
    int finish();

  private:
    void fail(const std::string& message);

    int width_;
    int height_;
    int total_frames_;
    int frames_written_ = 0;
    std::string error_;

    std::unique_ptr<subprocess> ffmpeg_;
    FILE* audio_ = nullptr;

    std::vector<unsigned char> rgb_;
    std::vector<short> samples_;
};

#endif
