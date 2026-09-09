// SPDX-License-Identifier: GPL-3.0-only
//
// This program links WFB-NG's GPL-3.0 receiver implementation. It is kept as
// a separate process from the MIT-licensed fpv4mac capture/orchestration code.

#include "fpv4mac/devourer_json.hpp"
#include "protocol.hpp"

#include "rx.hpp"

#include <sodium.h>

#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

constexpr std::size_t maximum_discovery_candidates = 8;
volatile sig_atomic_t stop_requested = 0;

void handle_signal(int) {
    stop_requested = 1;
}

void install_signal_handlers() {
    struct sigaction action {};
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGINT, &action, nullptr) != 0 ||
        sigaction(SIGTERM, &action, nullptr) != 0) {
        throw std::runtime_error("unable to install signal handlers");
    }
}

struct Options {
    std::filesystem::path key;
    std::string host{"127.0.0.1"};
    std::uint16_t port{5600};
    std::optional<std::uint32_t> link_id;
    std::uint8_t radio_port{};
    std::uint8_t channel{161};
    std::uint64_t epoch{};
    std::filesystem::path sdp;
    std::uint32_t health_interval_ms{1000};
    bool accept_any_channel_id{};
};

struct RuntimeState {
    std::atomic<std::uint64_t> wifi_frames{};
    std::atomic<std::uint64_t> filtered{};
    std::atomic<std::uint64_t> malformed{};
    std::atomic<std::uint64_t> candidate_limit_drops{};
    std::atomic<std::uint64_t> authenticated_sessions{};
    std::atomic<std::uint64_t> decrypt_errors{};
    std::atomic<std::uint64_t> fec_recovered{};
    std::atomic<std::uint64_t> packet_loss{};
    std::atomic<std::uint64_t> bad_packets{};
    std::atomic<std::uint64_t> udp_packets{};
    std::atomic<std::uint64_t> udp_bytes{};
    std::atomic<std::uint64_t> udp_send_errors{};
    std::atomic<std::uint64_t> rtp_packets{};
    std::atomic<std::uint64_t> rtp_bytes{};
    std::atomic<std::uint64_t> rtp_sequence_gaps{};
    std::atomic<std::uint64_t> last_wifi_ms{};
    std::atomic<std::uint64_t> last_udp_ms{};
    std::atomic<std::uint32_t> channel_id{};
    std::atomic<std::uint32_t> link_id{};
    std::atomic<std::uint16_t> payload_type{256};
    std::atomic<std::uint8_t> radio_port{};
    std::atomic<fpv4mac::wfb::VideoCodec> codec{fpv4mac::wfb::VideoCodec::unknown};
    std::atomic<bool> link_selected{};
};

std::mutex output_mutex;

std::string json_escape(const std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}

std::string_view require_value(int argc, char* argv[], int& index) {
    if (++index >= argc) {
        throw std::runtime_error(std::string("missing value for ") + argv[index - 1]);
    }
    return argv[index];
}

template <typename T>
T parse_unsigned(std::string_view text, const char* name, const T maximum) {
    unsigned long long value{};
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2);
    }
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value > maximum) {
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
            const auto value = require_value(argc, argv, index);
            if (value == "auto") {
                options.link_id.reset();
            } else {
                options.link_id = parse_unsigned<std::uint32_t>(value, "link-id", 0x00ffffff);
            }
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
        } else if (argument == "--sdp") {
            options.sdp = require_value(argc, argv, index);
        } else if (argument == "--health-interval-ms") {
            options.health_interval_ms = parse_unsigned<std::uint32_t>(
                require_value(argc, argv, index), "health-interval-ms", 60000);
        } else if (argument == "--accept-any-channel-id") {
            options.accept_any_channel_id = true;
        } else if (argument == "--help" || argument == "-h") {
            std::cout
                << "Usage: fpv4mac-wfb --key gs.key [--host 127.0.0.1] [--port 5600]\n"
                   "                   [--channel 161] [--link-id auto|ID] [--radio-port 0]\n"
                   "                   [--sdp video.sdp] [--health-interval-ms 1000]\n"
                   "Reads devourer rx.frame JSONL on stdin and emits recovered RTP/UDP.\n"
                   "Auto mode authenticates a discovered WFB link with gs.key before selecting it.\n";
            std::exit(EXIT_SUCCESS);
        } else {
            throw std::runtime_error("unknown argument: " + std::string(argument));
        }
    }
    if (options.key.empty()) {
        throw std::runtime_error("--key gs.key is required");
    }
    if (options.port == 0) {
        throw std::runtime_error("port must be greater than zero");
    }
    return options;
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

std::uint64_t elapsed_ms(const std::chrono::steady_clock::time_point started) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now() - started)
                                          .count());
}

std::uint64_t age_ms(const std::uint64_t now, const std::uint64_t last) {
    return now >= last ? now - last : 0;
}

void write_sdp(const std::filesystem::path& destination, const std::string& contents) {
    if (destination.empty()) {
        return;
    }
    if (!destination.parent_path().empty()) {
        std::filesystem::create_directories(destination.parent_path());
    }
    auto temporary = destination;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << contents;
        if (!output.good()) {
            throw std::runtime_error("unable to write SDP file: " + temporary.string());
        }
    }
    std::filesystem::rename(temporary, destination);
}

void emit_json(std::string_view json) {
    const std::lock_guard lock(output_mutex);
    std::cerr << json << '\n';
}

class ObservedAggregator final : public Aggregator {
public:
    ObservedAggregator(const Options& options, RuntimeState& state, const std::uint32_t channel_id,
                       const std::chrono::steady_clock::time_point started)
        : Aggregator(options.key.string(), options.epoch, channel_id), options_(options),
          state_(state), channel_id_(channel_id), started_(started) {
        socket_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_ < 0) {
            throw std::runtime_error("unable to open UDP socket");
        }
        address_.sin_family = AF_INET;
        address_.sin_port = htons(options_.port);
        if (inet_pton(AF_INET, options_.host.c_str(), &address_.sin_addr) != 1) {
            close(socket_);
            socket_ = -1;
            throw std::runtime_error("--host must be an IPv4 address");
        }
    }

    ~ObservedAggregator() override {
        if (socket_ >= 0) {
            close(socket_);
        }
    }

    ObservedAggregator(const ObservedAggregator&) = delete;
    ObservedAggregator& operator=(const ObservedAggregator&) = delete;

    [[nodiscard]] std::uint32_t channel_id() const { return channel_id_; }

    void sync_metrics() const {
        state_.authenticated_sessions.store(count_p_session);
        state_.decrypt_errors.store(count_p_dec_err);
        state_.fec_recovered.store(count_p_fec_recovered);
        state_.packet_loss.store(count_p_lost);
        state_.bad_packets.store(count_p_bad);
        state_.udp_packets.store(count_p_outgoing);
        state_.udp_bytes.store(count_b_outgoing);
        state_.rtp_packets.store(rtp_.packet_count());
        state_.rtp_bytes.store(rtp_.byte_count());
        state_.rtp_sequence_gaps.store(rtp_.sequence_gaps());
    }

protected:
    void send_to_socket(const std::uint8_t* payload, const std::uint16_t packet_size) override {
        const auto detected = rtp_.observe({payload, packet_size});
        if (rtp_.payload_type()) {
            state_.payload_type.store(*rtp_.payload_type());
        }
        state_.codec.store(rtp_.codec());
        if (detected) {
            const auto payload_type = rtp_.payload_type().value();
            const auto sdp = fpv4mac::wfb::make_video_sdp(
                options_.host, options_.port, payload_type, rtp_.codec());
            write_sdp(options_.sdp, sdp);

            const auto sdp_path = json_escape(options_.sdp.string());

            char event[512];
            std::snprintf(event, sizeof(event),
                          "{\"event\":\"media.detected\",\"codec\":\"%.*s\","
                          "\"payload_type\":%u,\"udp_url\":\"udp://%s:%u\","
                          "\"sdp\":\"%s\"}",
                          static_cast<int>(fpv4mac::wfb::video_codec_name(rtp_.codec()).size()),
                          fpv4mac::wfb::video_codec_name(rtp_.codec()).data(), payload_type,
                          options_.host.c_str(), options_.port, sdp_path.c_str());
            emit_json(event);
        }

        const auto sent = sendto(socket_, payload, packet_size, MSG_DONTWAIT,
                                 reinterpret_cast<const sockaddr*>(&address_), sizeof(address_));
        if (sent != packet_size) {
            ++state_.udp_send_errors;
        }
        state_.last_udp_ms.store(elapsed_ms(started_));
        state_.rtp_packets.store(rtp_.packet_count());
        state_.rtp_bytes.store(rtp_.byte_count());
        state_.rtp_sequence_gaps.store(rtp_.sequence_gaps());
    }

private:
    const Options& options_;
    RuntimeState& state_;
    std::uint32_t channel_id_{};
    std::chrono::steady_clock::time_point started_;
    int socket_{-1};
    sockaddr_in address_{};
    fpv4mac::wfb::RtpObserver rtp_;
};

void process_frame(ObservedAggregator& aggregator, const fpv4mac::RadioFrame& frame,
                   const Options& options) {
    const std::array<std::uint8_t, RX_ANT_MAX> antennas{0, 1, 0xff, 0xff};
    const std::array<std::int8_t, RX_ANT_MAX> rssi{
        frame.rssi[0], frame.rssi[1], std::numeric_limits<std::int8_t>::min(),
        std::numeric_limits<std::int8_t>::min()};
    const std::array<std::int8_t, RX_ANT_MAX> noise{
        static_cast<std::int8_t>(frame.rssi[0] - frame.snr[0]),
        static_cast<std::int8_t>(frame.rssi[1] - frame.snr[1]),
        std::numeric_limits<std::int8_t>::max(), std::numeric_limits<std::int8_t>::max()};
    aggregator.process_packet(frame.body.data(), frame.body.size() - 4, 0, antennas.data(),
                              rssi.data(), noise.data(), frequency_for_channel(options.channel),
                              static_cast<std::uint8_t>(frame.rate), frame.bandwidth, nullptr);
}

void select_link(RuntimeState& state, const fpv4mac::wfb::ChannelIdentity identity) {
    state.channel_id.store(identity.channel_id);
    state.link_id.store(identity.link_id);
    state.radio_port.store(identity.radio_port);
    state.link_selected.store(true);
    char event[256];
    std::snprintf(event, sizeof(event),
                  "{\"event\":\"wfb.link_selected\",\"link_id\":%u,"
                  "\"link_id_hex\":\"0x%06x\",\"radio_port\":%u,"
                  "\"channel_id\":%u}",
                  identity.link_id, identity.link_id, identity.radio_port, identity.channel_id);
    emit_json(event);
}

void emit_health(const RuntimeState& state,
                 const std::chrono::steady_clock::time_point started) {
    const auto now = elapsed_ms(started);
    const auto wifi_frames = state.wifi_frames.load();
    const auto sessions = state.authenticated_sessions.load();
    const auto udp_packets = state.udp_packets.load();
    const auto last_wifi = state.last_wifi_ms.load();
    const auto last_udp = state.last_udp_ms.load();
    std::string_view phase = "waiting_radio";
    if (wifi_frames > 0) {
        phase = "waiting_session";
    }
    if (sessions > 0) {
        phase = "authenticated";
    }
    if (udp_packets > 0) {
        phase = age_ms(now, last_udp) <= 3000 ? "receiving" : "stalled";
    }

    const auto codec = state.codec.load();
    const auto payload_type = state.payload_type.load();
    const auto payload_type_text = payload_type <= 127 ? std::to_string(payload_type) : "null";
    const auto wifi_age_text = wifi_frames > 0 ? std::to_string(age_ms(now, last_wifi)) : "null";
    const auto udp_age_text = udp_packets > 0 ? std::to_string(age_ms(now, last_udp)) : "null";
    char event[1024];
    std::snprintf(
        event, sizeof(event),
        "{\"event\":\"wfb.health\",\"state\":\"%.*s\",\"uptime_ms\":%llu,"
        "\"link_selected\":%s,\"channel_id\":%u,\"link_id\":%u,\"radio_port\":%u,"
        "\"wifi_frames\":%llu,\"authenticated_sessions\":%llu,"
        "\"decrypt_errors\":%llu,\"fec_recovered\":%llu,\"packet_loss\":%llu,"
        "\"bad_packets\":%llu,\"udp_packets\":%llu,\"udp_bytes\":%llu,"
        "\"udp_send_errors\":%llu,"
        "\"rtp_packets\":%llu,\"rtp_bytes\":%llu,\"rtp_sequence_gaps\":%llu,"
        "\"codec\":\"%.*s\",\"payload_type\":%s,"
        "\"last_wifi_age_ms\":%s,\"last_udp_age_ms\":%s}",
        static_cast<int>(phase.size()), phase.data(), static_cast<unsigned long long>(now),
        state.link_selected.load() ? "true" : "false", state.channel_id.load(),
        state.link_id.load(), state.radio_port.load(), static_cast<unsigned long long>(wifi_frames),
        static_cast<unsigned long long>(sessions),
        static_cast<unsigned long long>(state.decrypt_errors.load()),
        static_cast<unsigned long long>(state.fec_recovered.load()),
        static_cast<unsigned long long>(state.packet_loss.load()),
        static_cast<unsigned long long>(state.bad_packets.load()),
        static_cast<unsigned long long>(udp_packets),
        static_cast<unsigned long long>(state.udp_bytes.load()),
        static_cast<unsigned long long>(state.udp_send_errors.load()),
        static_cast<unsigned long long>(state.rtp_packets.load()),
        static_cast<unsigned long long>(state.rtp_bytes.load()),
        static_cast<unsigned long long>(state.rtp_sequence_gaps.load()),
        static_cast<int>(fpv4mac::wfb::video_codec_name(codec).size()),
        fpv4mac::wfb::video_codec_name(codec).data(),
        payload_type_text.c_str(), wifi_age_text.c_str(), udp_age_text.c_str());
    emit_json(event);
}

class HealthReporter {
public:
    HealthReporter(const RuntimeState& state,
                   const std::chrono::steady_clock::time_point started,
                   const std::uint32_t interval_ms)
        : state_(state), started_(started), interval_ms_(interval_ms) {
        if (interval_ms_ > 0) {
            worker_ = std::thread([this] {
                std::unique_lock lock(mutex_);
                while (!done_) {
                    if (condition_.wait_for(lock, std::chrono::milliseconds(interval_ms_),
                                            [this] { return done_; })) {
                        break;
                    }
                    lock.unlock();
                    emit_health(state_, started_);
                    lock.lock();
                }
            });
        }
    }

    ~HealthReporter() {
        {
            const std::lock_guard lock(mutex_);
            done_ = true;
        }
        condition_.notify_one();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    const RuntimeState& state_;
    std::chrono::steady_clock::time_point started_;
    std::uint32_t interval_ms_{};
    bool done_{};
    std::mutex mutex_;
    std::condition_variable condition_;
    std::thread worker_;
};

} // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parse_options(argc, argv);
        if (sodium_init() < 0) {
            throw std::runtime_error("libsodium initialization failed");
        }
        install_signal_handlers();

        const auto started = std::chrono::steady_clock::now();
        RuntimeState state;
        HealthReporter health(state, started, options.health_interval_ms);
        std::unique_ptr<ObservedAggregator> selected;
        std::map<std::uint32_t, std::unique_ptr<ObservedAggregator>> candidates;

        if (options.link_id) {
            const auto channel_id = (*options.link_id << 8) | options.radio_port;
            selected = std::make_unique<ObservedAggregator>(options, state, channel_id, started);
            select_link(state, fpv4mac::wfb::ChannelIdentity{
                                   .channel_id = channel_id,
                                   .link_id = *options.link_id,
                                   .radio_port = options.radio_port,
                               });
        }

        std::string line;
        while (!stop_requested) {
            pollfd input{.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
            const auto poll_result = poll(&input, 1, 250);
            if (poll_result < 0) {
                if (stop_requested) {
                    break;
                }
                throw std::runtime_error("polling receiver input failed");
            }
            if (poll_result == 0) {
                continue;
            }
            if ((input.revents & (POLLERR | POLLNVAL)) != 0) {
                throw std::runtime_error("receiver input became unavailable");
            }
            if (!std::getline(std::cin, line)) {
                break;
            }
            if (!fpv4mac::is_devourer_frame_event(line)) {
                continue;
            }
            std::string error;
            auto frame = fpv4mac::parse_devourer_frame(line, &error);
            if (!frame || frame->body.size() <= 4) {
                ++state.malformed;
                continue;
            }
            const auto identity = fpv4mac::wfb::channel_identity_from_source(frame->source_address);
            if (!identity) {
                ++state.filtered;
                continue;
            }

            ++state.wifi_frames;
            state.last_wifi_ms.store(elapsed_ms(started));
            if (selected) {
                if (!options.accept_any_channel_id &&
                    identity->channel_id != selected->channel_id()) {
                    ++state.filtered;
                    continue;
                }
                process_frame(*selected, *frame, options);
                selected->sync_metrics();
                continue;
            }

            auto candidate = candidates.find(identity->channel_id);
            if (candidate == candidates.end()) {
                if (frame->body.front() != WFB_PACKET_SESSION) {
                    ++state.filtered;
                    continue;
                }
                if (candidates.size() >= maximum_discovery_candidates) {
                    ++state.candidate_limit_drops;
                    continue;
                }
                candidate = candidates
                                .emplace(identity->channel_id,
                                         std::make_unique<ObservedAggregator>(
                                             options, state, identity->channel_id, started))
                                .first;
            }
            process_frame(*candidate->second, *frame, options);
            if (candidate->second->count_p_session > 0) {
                select_link(state, *identity);
                selected = std::move(candidate->second);
                candidates.clear();
                selected->sync_metrics();
            }
        }

        if (selected) {
            selected->sync_metrics();
        }
        emit_health(state, started);
        const auto total_ms = elapsed_ms(started);
        char event[1024];
        std::snprintf(
            event, sizeof(event),
            "{\"event\":\"wfb.complete\",\"link_selected\":%s,\"channel_id\":%u,\"link_id\":%u,"
            "\"radio_port\":%u,\"wifi_frames\":%llu,\"filtered\":%llu,"
            "\"malformed\":%llu,\"candidate_limit_drops\":%llu,"
            "\"authenticated_sessions\":%llu,\"decrypt_errors\":%llu,"
            "\"fec_recovered\":%llu,\"packet_loss\":%llu,\"bad_packets\":%llu,"
            "\"udp_packets\":%llu,\"udp_bytes\":%llu,\"udp_send_errors\":%llu,"
            "\"rtp_packets\":%llu,"
            "\"rtp_sequence_gaps\":%llu,\"elapsed_ms\":%llu}",
            state.link_selected.load() ? "true" : "false", state.channel_id.load(),
            state.link_id.load(), state.radio_port.load(),
            static_cast<unsigned long long>(state.wifi_frames.load()),
            static_cast<unsigned long long>(state.filtered.load()),
            static_cast<unsigned long long>(state.malformed.load()),
            static_cast<unsigned long long>(state.candidate_limit_drops.load()),
            static_cast<unsigned long long>(state.authenticated_sessions.load()),
            static_cast<unsigned long long>(state.decrypt_errors.load()),
            static_cast<unsigned long long>(state.fec_recovered.load()),
            static_cast<unsigned long long>(state.packet_loss.load()),
            static_cast<unsigned long long>(state.bad_packets.load()),
            static_cast<unsigned long long>(state.udp_packets.load()),
            static_cast<unsigned long long>(state.udp_bytes.load()),
            static_cast<unsigned long long>(state.udp_send_errors.load()),
            static_cast<unsigned long long>(state.rtp_packets.load()),
            static_cast<unsigned long long>(state.rtp_sequence_gaps.load()),
            static_cast<unsigned long long>(total_ms));
        emit_json(event);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "fpv4mac-wfb: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
