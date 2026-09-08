#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fpv4mac {

enum class WfbPacketType : std::uint8_t {
    unknown = 0,
    data = 1,
    session = 2,
};

struct RadioFrame {
    std::uint64_t capture_time_ns{};
    std::uint32_t tsfl{};
    std::uint16_t sequence{};
    std::uint16_t rate{};
    std::uint8_t bandwidth{};
    bool crc_error{};
    bool icv_error{};
    bool ldpc{};
    bool short_guard_interval{};
    std::array<std::int8_t, 2> rssi{};
    std::array<std::int8_t, 2> snr{};
    std::array<std::int8_t, 2> evm{};
    std::array<std::uint8_t, 6> source_address{};
    std::vector<std::uint8_t> body;
};

WfbPacketType classify_wfb_packet(const RadioFrame& frame);
std::string_view wfb_packet_type_name(WfbPacketType type);

} // namespace fpv4mac
