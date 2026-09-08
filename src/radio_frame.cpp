#include "fpv4mac/radio_frame.hpp"

#include <string_view>

namespace fpv4mac {

WfbPacketType classify_wfb_packet(const RadioFrame& frame) {
    if (frame.body.empty() || frame.source_address[0] != 0x57 ||
        frame.source_address[1] != 0x42) {
        return WfbPacketType::unknown;
    }
    if (frame.body.front() == static_cast<std::uint8_t>(WfbPacketType::data)) {
        return WfbPacketType::data;
    }
    if (frame.body.front() == static_cast<std::uint8_t>(WfbPacketType::session)) {
        return WfbPacketType::session;
    }
    return WfbPacketType::unknown;
}

std::string_view wfb_packet_type_name(const WfbPacketType type) {
    switch (type) {
    case WfbPacketType::data:
        return "data";
    case WfbPacketType::session:
        return "session";
    case WfbPacketType::unknown:
        return "unknown";
    }
    return "unknown";
}

} // namespace fpv4mac
