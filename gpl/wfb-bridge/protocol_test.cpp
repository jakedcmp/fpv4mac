// SPDX-License-Identifier: GPL-3.0-only

#include "protocol.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<std::uint8_t> rtp_packet(const std::uint8_t payload_type,
                                     const std::uint16_t sequence,
                                     const std::array<std::uint8_t, 2> payload) {
    return {
        0x80,
        payload_type,
        static_cast<std::uint8_t>(sequence >> 8),
        static_cast<std::uint8_t>(sequence),
        0,
        0,
        0,
        1,
        0x12,
        0x34,
        0x56,
        0x78,
        payload[0],
        payload[1],
    };
}

} // namespace

int main() {
    const auto identity = fpv4mac::wfb::channel_identity_from_source(
        std::array<std::uint8_t, 6>{0x57, 0x42, 0x75, 0x05, 0xd6, 0x00});
    require(identity.has_value(), "WFB source address was not recognized");
    require(identity->channel_id == 0x7505d600, "channel ID was decoded incorrectly");
    require(identity->link_id == 0x7505d6, "link ID was decoded incorrectly");
    require(identity->radio_port == 0, "radio port was decoded incorrectly");
    require(!fpv4mac::wfb::channel_identity_from_source(
                 std::array<std::uint8_t, 6>{0, 1, 2, 3, 4, 5}),
            "non-WFB source address was accepted");

    fpv4mac::wfb::RtpObserver h265;
    const auto h265_first = rtp_packet(97, 100, {0x40, 0x01}); // VPS, NAL type 32
    const auto h265_gap = rtp_packet(97, 103, {0x42, 0x01});   // SPS, NAL type 33
    require(h265.observe(h265_first), "H.265 was not detected");
    require(!h265.observe(h265_gap), "codec detection was reported twice");
    require(h265.codec() == fpv4mac::wfb::VideoCodec::h265, "wrong H.265 codec result");
    require(h265.payload_type() == 97, "wrong RTP payload type");
    require(h265.packet_count() == 2, "wrong RTP packet count");
    require(h265.sequence_gaps() == 2, "wrong RTP sequence gap count");

    fpv4mac::wfb::RtpObserver h264;
    require(h264.observe(rtp_packet(96, 1, {0x67, 0x01})), "H.264 was not detected");
    require(h264.codec() == fpv4mac::wfb::VideoCodec::h264, "wrong H.264 codec result");
    fpv4mac::wfb::RtpObserver ambiguous_h264_slice;
    require(!ambiguous_h264_slice.observe(rtp_packet(96, 1, {0x41, 0x9a})),
            "H.264 slice was misclassified as H.265");

    const std::array<std::uint8_t, 4> short_packet{0x80, 0x61, 0, 0};
    require(!fpv4mac::wfb::inspect_rtp_packet(short_packet), "short RTP packet was accepted");
    const std::array<std::uint8_t, 16> bad_extension{
        0x90, 0x61, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2};
    require(!fpv4mac::wfb::inspect_rtp_packet(bad_extension),
            "truncated RTP extension was accepted");

    const auto sdp = fpv4mac::wfb::make_video_sdp("127.0.0.1", 5600, 97,
                                                  fpv4mac::wfb::VideoCodec::h265);
    require(sdp.find("m=video 5600 RTP/AVP 97") != std::string::npos,
            "SDP media line is missing");
    require(sdp.find("a=rtpmap:97 H265/90000") != std::string::npos,
            "SDP codec line is missing");
    return 0;
}
