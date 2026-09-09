// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace fpv4mac::wfb {

struct ChannelIdentity {
    std::uint32_t channel_id{};
    std::uint32_t link_id{};
    std::uint8_t radio_port{};
};

std::optional<ChannelIdentity> channel_identity_from_source(
    const std::array<std::uint8_t, 6>& source_address);

enum class VideoCodec : std::uint8_t {
    unknown = 0,
    h264,
    h265,
};

std::string_view video_codec_name(VideoCodec codec);

struct RtpPacketInfo {
    std::uint8_t payload_type{};
    std::uint16_t sequence{};
    std::uint32_t ssrc{};
    VideoCodec codec_hint{VideoCodec::unknown};
};

std::optional<RtpPacketInfo> inspect_rtp_packet(std::span<const std::uint8_t> packet);

class RtpObserver {
public:
    bool observe(std::span<const std::uint8_t> packet);

    [[nodiscard]] VideoCodec codec() const { return codec_; }
    [[nodiscard]] std::optional<std::uint8_t> payload_type() const { return payload_type_; }
    [[nodiscard]] std::uint64_t packet_count() const { return packet_count_; }
    [[nodiscard]] std::uint64_t byte_count() const { return byte_count_; }
    [[nodiscard]] std::uint64_t sequence_gaps() const { return sequence_gaps_; }

private:
    VideoCodec codec_{VideoCodec::unknown};
    std::optional<std::uint8_t> payload_type_;
    std::uint64_t packet_count_{};
    std::uint64_t byte_count_{};
    std::uint64_t sequence_gaps_{};
    std::unordered_map<std::uint32_t, std::uint16_t> last_sequence_by_ssrc_;
};

std::string make_video_sdp(std::string_view host, std::uint16_t port,
                           std::uint8_t payload_type, VideoCodec codec);

} // namespace fpv4mac::wfb
