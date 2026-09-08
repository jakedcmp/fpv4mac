#pragma once

#include "fpv4mac/radio_frame.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace fpv4mac {

inline constexpr std::uint16_t capture_format_version = 1;
inline constexpr std::uint32_t maximum_capture_body_size = 16 * 1024;

class CaptureWriter {
public:
    explicit CaptureWriter(const std::filesystem::path& path);
    void write(const RadioFrame& frame);
    void flush();
    std::uint64_t frame_count() const { return frame_count_; }

private:
    std::ofstream stream_;
    std::uint64_t frame_count_{};
};

class CaptureReader {
public:
    explicit CaptureReader(const std::filesystem::path& path);
    std::optional<RadioFrame> next();

private:
    std::ifstream stream_;
};

} // namespace fpv4mac
