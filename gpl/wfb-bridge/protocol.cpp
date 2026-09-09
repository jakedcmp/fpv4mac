// SPDX-License-Identifier: GPL-3.0-only

#include "protocol.hpp"

#include <sstream>

namespace fpv4mac::wfb {
namespace {

VideoCodec codec_hint(std::span<const std::uint8_t> payload) {
    if (payload.empty()) {
        return VideoCodec::unknown;
    }

    if (payload.size() >= 2) {
        const auto h265_type = static_cast<std::uint8_t>((payload[0] >> 1) & 0x3f);
        const auto h265_layer_id = static_cast<std::uint8_t>(
            ((payload[0] & 0x01) << 5) | (payload[1] >> 3));
        const auto temporal_id_plus_one = static_cast<std::uint8_t>(payload[1] & 0x07);
        if (h265_layer_id == 0 && temporal_id_plus_one != 0 &&
            (h265_type == 32 || h265_type == 33 || h265_type == 34 || h265_type == 48 ||
             h265_type == 49)) {
            return VideoCodec::h265;
        }
    }

    const auto h264_type = static_cast<std::uint8_t>(payload[0] & 0x1f);
    if (h264_type == 5 || h264_type == 7 || h264_type == 8 || h264_type == 24 ||
        h264_type == 28) {
        return VideoCodec::h264;
    }
    return VideoCodec::unknown;
}

} // namespace

std::optional<ChannelIdentity> channel_identity_from_source(
    const std::array<std::uint8_t, 6>& source_address) {
    if (source_address[0] != 0x57 || source_address[1] != 0x42) {
        return std::nullopt;
    }

    const auto channel_id = (static_cast<std::uint32_t>(source_address[2]) << 24) |
                            (static_cast<std::uint32_t>(source_address[3]) << 16) |
                            (static_cast<std::uint32_t>(source_address[4]) << 8) |
                            static_cast<std::uint32_t>(source_address[5]);
    return ChannelIdentity{
        .channel_id = channel_id,
        .link_id = channel_id >> 8,
        .radio_port = static_cast<std::uint8_t>(channel_id & 0xff),
    };
}

std::string_view video_codec_name(const VideoCodec codec) {
    switch (codec) {
    case VideoCodec::h264:
        return "H264";
    case VideoCodec::h265:
        return "H265";
    case VideoCodec::unknown:
        return "unknown";
    }
    return "unknown";
}

std::optional<RtpPacketInfo> inspect_rtp_packet(const std::span<const std::uint8_t> packet) {
    if (packet.size() < 12 || (packet[0] >> 6) != 2) {
        return std::nullopt;
    }

    const auto csrc_count = static_cast<std::size_t>(packet[0] & 0x0f);
    std::size_t payload_offset = 12 + csrc_count * 4;
    if (payload_offset > packet.size()) {
        return std::nullopt;
    }

    if ((packet[0] & 0x10) != 0) {
        if (payload_offset + 4 > packet.size()) {
            return std::nullopt;
        }
        const auto extension_words =
            (static_cast<std::size_t>(packet[payload_offset + 2]) << 8) |
            static_cast<std::size_t>(packet[payload_offset + 3]);
        payload_offset += 4 + extension_words * 4;
        if (payload_offset > packet.size()) {
            return std::nullopt;
        }
    }

    std::size_t payload_end = packet.size();
    if ((packet[0] & 0x20) != 0) {
        const auto padding = static_cast<std::size_t>(packet.back());
        if (padding == 0 || padding > payload_end - payload_offset) {
            return std::nullopt;
        }
        payload_end -= padding;
    }
    const auto payload = packet.subspan(payload_offset, payload_end - payload_offset);

    return RtpPacketInfo{
        .payload_type = static_cast<std::uint8_t>(packet[1] & 0x7f),
        .sequence = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(packet[2]) << 8) | packet[3]),
        .ssrc = (static_cast<std::uint32_t>(packet[8]) << 24) |
                (static_cast<std::uint32_t>(packet[9]) << 16) |
                (static_cast<std::uint32_t>(packet[10]) << 8) |
                static_cast<std::uint32_t>(packet[11]),
        .codec_hint = codec_hint(payload),
    };
}

bool RtpObserver::observe(const std::span<const std::uint8_t> packet) {
    const auto info = inspect_rtp_packet(packet);
    if (!info) {
        return false;
    }

    ++packet_count_;
    byte_count_ += packet.size();
    payload_type_ = info->payload_type;

    const auto previous = last_sequence_by_ssrc_.find(info->ssrc);
    if (previous != last_sequence_by_ssrc_.end()) {
        const auto delta = static_cast<std::uint16_t>(info->sequence - previous->second);
        if (delta > 1 && delta < 0x8000) {
            sequence_gaps_ += delta - 1;
        }
        previous->second = info->sequence;
    } else {
        last_sequence_by_ssrc_.emplace(info->ssrc, info->sequence);
    }

    if (codec_ == VideoCodec::unknown && info->codec_hint != VideoCodec::unknown) {
        codec_ = info->codec_hint;
        return true;
    }
    return false;
}

std::string make_video_sdp(const std::string_view host, const std::uint16_t port,
                           const std::uint8_t payload_type, const VideoCodec codec) {
    if (codec == VideoCodec::unknown) {
        return {};
    }

    std::ostringstream output;
    output << "v=0\n"
           << "o=- 0 0 IN IP4 " << host << "\n"
           << "s=fpv4mac WFB-NG video\n"
           << "c=IN IP4 " << host << "\n"
           << "t=0 0\n"
           << "m=video " << port << " RTP/AVP " << static_cast<unsigned>(payload_type) << "\n"
           << "a=rtpmap:" << static_cast<unsigned>(payload_type) << ' '
           << video_codec_name(codec) << "/90000\n"
           << "a=recvonly\n";
    return output.str();
}

} // namespace fpv4mac::wfb
