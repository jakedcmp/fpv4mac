// SPDX-License-Identifier: GPL-3.0-only
//
// This program links WFB-NG's GPL-3.0 receiver implementation. It is kept as
// a separate process from the MIT-licensed fpv4mac capture/orchestration code.

#include "fpv4mac/devourer_json.hpp"

#include "rx.hpp"

#include <sodium.h>

#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct Options {
    std::filesystem::path key;
    std::string host{"127.0.0.1"};
    std::uint16_t port{5600};
    std::uint32_t link_id{};
    std::uint8_t radio_port{};
    std::uint8_t channel{161};
    std::uint64_t epoch{};
    bool accept_any_channel_id{};
};

std::string_view require_value(int argc, char* argv[], int& index) {
    if (++index >= argc) {
        throw std::runtime_error(std::string("missing value for ") + argv[index - 1]);
    }
    return argv[index];
}

template <typename T>
T parse_unsigned(std::string_view text, const char* name, const T maximum) {
    unsigned long long value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value > maximum) {
        throw std::runtime_error(std::string(name) + " is out of range");
    }
    return static_cast<T>(value);
}

Options parse_options(int argc, char* argv[]) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--key") {
            options.key = require_value(argc, argv, index);
        } else if (argument == "--host") {
            options.host = require_value(argc, argv, index);
        } else if (argument == "--port") {
            options.port = parse_unsigned<std::uint16_t>(
                require_value(argc, argv, index), "port", 65535);
        } else if (argument == "--link-id") {
            options.link_id = parse_unsigned<std::uint32_t>(
                require_value(argc, argv, index), "link-id", 0x00ffffff);
        } else if (argument == "--radio-port") {
            options.radio_port = parse_unsigned<std::uint8_t>(
                require_value(argc, argv, index), "radio-port", 255);
        } else if (argument == "--channel") {
            options.channel = parse_unsigned<std::uint8_t>(
                require_value(argc, argv, index), "channel", 255);
        } else if (argument == "--epoch") {
            options.epoch = parse_unsigned<std::uint64_t>(
                require_value(argc, argv, index), "epoch",
                std::numeric_limits<std::uint64_t>::max());
        } else if (argument == "--accept-any-channel-id") {
            options.accept_any_channel_id = true;
        } else if (argument == "--help" || argument == "-h") {
            std::cout << "Usage: fpv4mac-wfb --key gs.key [--host 127.0.0.1] [--port 5600]\n"
                         "                   [--channel 161] [--link-id 0] [--radio-port 0]\n"
                         "Reads devourer rx.frame JSONL on stdin and emits recovered UDP.\n";
            std::exit(EXIT_SUCCESS);
        } else {
            throw std::runtime_error("unknown argument: " + std::string(argument));
        }
    }
    if (options.key.empty()) {
        throw std::runtime_error("--key gs.key is required");
    }
    return options;
}

bool matches_channel(const fpv4mac::RadioFrame& frame, const std::uint32_t channel_id) {
    return frame.source_address[0] == 0x57 && frame.source_address[1] == 0x42 &&
           frame.source_address[2] == static_cast<std::uint8_t>(channel_id >> 24) &&
           frame.source_address[3] == static_cast<std::uint8_t>(channel_id >> 16) &&
           frame.source_address[4] == static_cast<std::uint8_t>(channel_id >> 8) &&
           frame.source_address[5] == static_cast<std::uint8_t>(channel_id);
}

std::uint16_t frequency_for_channel(const std::uint8_t channel) {
    if (channel >= 1 && channel <= 13) {
        return static_cast<std::uint16_t>(2407 + channel * 5);
    }
    if (channel == 14) {
        return 2484;
    }
    return static_cast<std::uint16_t>(5000 + channel * 5);
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parse_options(argc, argv);
        if (sodium_init() < 0) {
            throw std::runtime_error("libsodium initialization failed");
        }
        const std::uint32_t channel_id = (options.link_id << 8) + options.radio_port;
        AggregatorUDPv4 aggregator(options.host, options.port, options.key.string(), options.epoch,
                                   channel_id, 0);
        std::uint64_t accepted{};
        std::uint64_t filtered{};
        std::uint64_t malformed{};
        const auto started = std::chrono::steady_clock::now();
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!fpv4mac::is_devourer_frame_event(line)) {
                continue;
            }
            std::string error;
            auto frame = fpv4mac::parse_devourer_frame(line, &error);
            if (!frame || frame->body.size() <= 4) {
                ++malformed;
                continue;
            }
            if (!options.accept_any_channel_id && !matches_channel(*frame, channel_id)) {
                ++filtered;
                continue;
            }

            const std::array<std::uint8_t, RX_ANT_MAX> antennas{0, 1, 0xff, 0xff};
            const std::array<std::int8_t, RX_ANT_MAX> rssi{
                frame->rssi[0], frame->rssi[1], std::numeric_limits<std::int8_t>::min(),
                std::numeric_limits<std::int8_t>::min()};
            const std::array<std::int8_t, RX_ANT_MAX> noise{
                static_cast<std::int8_t>(frame->rssi[0] - frame->snr[0]),
                static_cast<std::int8_t>(frame->rssi[1] - frame->snr[1]),
                std::numeric_limits<std::int8_t>::max(),
                std::numeric_limits<std::int8_t>::max()};
            aggregator.process_packet(
                frame->body.data(), frame->body.size() - 4, 0, antennas.data(), rssi.data(),
                noise.data(), frequency_for_channel(options.channel),
                static_cast<std::uint8_t>(frame->rate), frame->bandwidth, nullptr);
            ++accepted;
        }
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - started)
                                    .count();
        std::cerr << "{\"event\":\"wfb.complete\",\"accepted\":" << accepted
                  << ",\"filtered\":" << filtered << ",\"malformed\":" << malformed
                  << ",\"decrypt_errors\":" << aggregator.count_p_dec_err
                  << ",\"fec_recovered\":" << aggregator.count_p_fec_recovered
                  << ",\"packet_loss\":" << aggregator.count_p_lost
                  << ",\"bad_packets\":" << aggregator.count_p_bad
                  << ",\"udp_packets\":" << aggregator.count_p_outgoing
                  << ",\"udp_bytes\":" << aggregator.count_b_outgoing
                  << ",\"elapsed_ms\":" << elapsed_ms << "}\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "fpv4mac-wfb: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
