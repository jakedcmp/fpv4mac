#include "fpv4mac/capture.hpp"

#include <array>
#include <stdexcept>

namespace fpv4mac {
namespace {

constexpr std::array<char, 8> magic{'F', 'P', 'V', '4', 'C', 'A', 'P', '\0'};

template <typename T>
void write_unsigned(std::ostream& stream, T value) {
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        stream.put(static_cast<char>((value >> (index * 8)) & 0xff));
    }
}

template <typename T>
T read_unsigned(std::istream& stream) {
    T value{};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        const int byte = stream.get();
        if (byte == std::char_traits<char>::eof()) {
            throw std::runtime_error("truncated fpv4cap record");
        }
        value |= static_cast<T>(static_cast<unsigned char>(byte)) << (index * 8);
    }
    return value;
}

void require_stream(const std::ios& stream, const char* operation) {
    if (!stream) {
        throw std::runtime_error(std::string(operation) + " failed");
    }
}

} // namespace

CaptureWriter::CaptureWriter(const std::filesystem::path& path)
    : stream_(path, std::ios::binary | std::ios::trunc) {
    require_stream(stream_, "opening capture output");
    stream_.write(magic.data(), magic.size());
    write_unsigned<std::uint16_t>(stream_, capture_format_version);
    write_unsigned<std::uint16_t>(stream_, 16);
    write_unsigned<std::uint32_t>(stream_, 0);
    require_stream(stream_, "writing capture header");
    flush();
}

void CaptureWriter::write(const RadioFrame& frame) {
    if (frame.body.size() > maximum_capture_body_size) {
        throw std::runtime_error("frame body exceeds fpv4cap limit");
    }
    write_unsigned<std::uint64_t>(stream_, frame.capture_time_ns);
    write_unsigned<std::uint32_t>(stream_, frame.tsfl);
    write_unsigned<std::uint16_t>(stream_, frame.sequence);
    write_unsigned<std::uint16_t>(stream_, frame.rate);
    stream_.put(static_cast<char>(frame.bandwidth));
    std::uint8_t flags{};
    flags |= frame.crc_error ? 0x01 : 0;
    flags |= frame.icv_error ? 0x02 : 0;
    flags |= frame.ldpc ? 0x04 : 0;
    flags |= frame.short_guard_interval ? 0x08 : 0;
    stream_.put(static_cast<char>(flags));
    for (const auto value : frame.rssi) {
        stream_.put(static_cast<char>(value));
    }
    for (const auto value : frame.snr) {
        stream_.put(static_cast<char>(value));
    }
    for (const auto value : frame.evm) {
        stream_.put(static_cast<char>(value));
    }
    stream_.write(reinterpret_cast<const char*>(frame.source_address.data()),
                  frame.source_address.size());
    write_unsigned<std::uint32_t>(stream_, static_cast<std::uint32_t>(frame.body.size()));
    stream_.write(reinterpret_cast<const char*>(frame.body.data()),
                  static_cast<std::streamsize>(frame.body.size()));
    require_stream(stream_, "writing capture frame");
    ++frame_count_;
}

void CaptureWriter::flush() {
    stream_.flush();
    require_stream(stream_, "flushing capture output");
}

CaptureReader::CaptureReader(const std::filesystem::path& path) : stream_(path, std::ios::binary) {
    require_stream(stream_, "opening capture input");
    std::array<char, 8> actual_magic{};
    stream_.read(actual_magic.data(), actual_magic.size());
    if (actual_magic != magic) {
        throw std::runtime_error("not an fpv4cap capture");
    }
    const auto version = read_unsigned<std::uint16_t>(stream_);
    const auto header_size = read_unsigned<std::uint16_t>(stream_);
    (void)read_unsigned<std::uint32_t>(stream_);
    if (version != capture_format_version || header_size != 16) {
        throw std::runtime_error("unsupported fpv4cap version");
    }
}

std::optional<RadioFrame> CaptureReader::next() {
    if (stream_.peek() == std::char_traits<char>::eof()) {
        return std::nullopt;
    }
    RadioFrame frame;
    frame.capture_time_ns = read_unsigned<std::uint64_t>(stream_);
    frame.tsfl = read_unsigned<std::uint32_t>(stream_);
    frame.sequence = read_unsigned<std::uint16_t>(stream_);
    frame.rate = read_unsigned<std::uint16_t>(stream_);
    frame.bandwidth = read_unsigned<std::uint8_t>(stream_);
    const auto flags = read_unsigned<std::uint8_t>(stream_);
    frame.crc_error = (flags & 0x01) != 0;
    frame.icv_error = (flags & 0x02) != 0;
    frame.ldpc = (flags & 0x04) != 0;
    frame.short_guard_interval = (flags & 0x08) != 0;
    for (auto& value : frame.rssi) {
        value = static_cast<std::int8_t>(read_unsigned<std::uint8_t>(stream_));
    }
    for (auto& value : frame.snr) {
        value = static_cast<std::int8_t>(read_unsigned<std::uint8_t>(stream_));
    }
    for (auto& value : frame.evm) {
        value = static_cast<std::int8_t>(read_unsigned<std::uint8_t>(stream_));
    }
    stream_.read(reinterpret_cast<char*>(frame.source_address.data()), frame.source_address.size());
    const auto body_size = read_unsigned<std::uint32_t>(stream_);
    if (body_size > maximum_capture_body_size) {
        throw std::runtime_error("capture frame body exceeds safety limit");
    }
    frame.body.resize(body_size);
    stream_.read(reinterpret_cast<char*>(frame.body.data()), body_size);
    require_stream(stream_, "reading capture frame");
    return frame;
}

} // namespace fpv4mac
