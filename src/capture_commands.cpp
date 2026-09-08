#include "fpv4mac/capture_commands.hpp"

#include "fpv4mac/bounded_queue.hpp"
#include "fpv4mac/capture.hpp"
#include "fpv4mac/devourer_json.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace fpv4mac {
namespace {

volatile std::sig_atomic_t capture_stop_requested = 0;

void request_capture_stop(int) {
    capture_stop_requested = 1;
}

struct CaptureOptions {
    std::filesystem::path output;
    std::filesystem::path input;
    std::size_t queue_capacity{1024};
    bool passthrough{};
};

std::string_view require_value(int argc, char* argv[], int& index) {
    if (++index >= argc) {
        throw std::runtime_error(std::string("missing value for ") + argv[index - 1]);
    }
    return argv[index];
}

std::size_t parse_size(const std::string_view text, const char* name) {
    std::size_t consumed{};
    const auto value = std::stoull(std::string(text), &consumed);
    if (consumed != text.size() || value == 0) {
        throw std::runtime_error(std::string(name) + " must be a positive integer");
    }
    return static_cast<std::size_t>(value);
}

std::unique_ptr<std::istream> open_input(const std::filesystem::path& path) {
    if (path.empty() || path == "-") {
        return nullptr;
    }
    auto stream = std::make_unique<std::ifstream>(path);
    if (!*stream) {
        throw std::runtime_error("unable to open input: " + path.string());
    }
    return stream;
}

CaptureOptions parse_capture_options(int argc, char* argv[]) {
    CaptureOptions options;
    for (int index = 2; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--output") {
            options.output = require_value(argc, argv, index);
        } else if (argument == "--input") {
            options.input = require_value(argc, argv, index);
        } else if (argument == "--queue") {
            options.queue_capacity = parse_size(require_value(argc, argv, index), "queue");
        } else if (argument == "--passthrough") {
            options.passthrough = true;
        } else {
            throw std::runtime_error("unknown capture argument: " + std::string(argument));
        }
    }
    if (options.output.empty()) {
        throw std::runtime_error("capture requires --output FILE");
    }
    return options;
}

std::filesystem::path required_input_path(int argc, char* argv[]) {
    std::filesystem::path input;
    for (int index = 2; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--input") {
            input = require_value(argc, argv, index);
        } else if (argument == "--speed") {
            (void)require_value(argc, argv, index);
        } else if (argument != "--json") {
            throw std::runtime_error("unknown argument: " + std::string(argument));
        }
    }
    if (input.empty()) {
        throw std::runtime_error("command requires --input FILE");
    }
    return input;
}

} // namespace

int run_capture_command(int argc, char* argv[]) {
    const auto options = parse_capture_options(argc, argv);
    auto owned_input = open_input(options.input);
    std::istream& input = owned_input ? *owned_input : std::cin;
    CaptureWriter writer(options.output);
    BoundedQueue<RadioFrame> queue(options.queue_capacity);
    std::atomic<std::uint64_t> seen{};
    std::atomic<std::uint64_t> malformed{};
    std::atomic<std::uint64_t> dropped{};
    const auto start = std::chrono::steady_clock::now();
    capture_stop_requested = 0;
    std::signal(SIGINT, request_capture_stop);
    std::signal(SIGTERM, request_capture_stop);

    std::thread producer([&] {
        std::string line;
        while (std::getline(input, line)) {
            if (!is_devourer_frame_event(line)) {
                continue;
            }
            ++seen;
            std::string error;
            auto frame = parse_devourer_frame(line, &error);
            if (!frame) {
                ++malformed;
                continue;
            }
            frame->capture_time_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count());
            if (!queue.try_push(std::move(*frame))) {
                ++dropped;
            }
            if (capture_stop_requested != 0) {
                break;
            }
        }
        queue.close();
    });

    while (auto frame = queue.pop()) {
        writer.write(*frame);
        if (options.passthrough) {
            std::cout << serialize_devourer_frame(*frame) << '\n' << std::flush;
        }
    }
    producer.join();
    writer.flush();
    std::cerr << "{\"event\":\"capture.complete\",\"frames\":" << writer.frame_count()
              << ",\"seen\":" << seen << ",\"malformed\":" << malformed
              << ",\"queue_dropped\":" << dropped << "}\n";
    return dropped == 0 && malformed == 0 ? EXIT_SUCCESS : 4;
}

int run_inspect_command(int argc, char* argv[]) {
    const auto input = required_input_path(argc, argv);
    CaptureReader reader(input);
    std::uint64_t frames{};
    std::uint64_t data{};
    std::uint64_t sessions{};
    std::uint64_t unknown{};
    std::uint64_t bytes{};
    std::uint64_t crc_errors{};
    std::uint64_t first_time{};
    std::uint64_t last_time{};
    while (auto frame = reader.next()) {
        if (frames == 0) {
            first_time = frame->capture_time_ns;
        }
        last_time = frame->capture_time_ns;
        ++frames;
        bytes += frame->body.size();
        crc_errors += frame->crc_error ? 1 : 0;
        switch (classify_wfb_packet(*frame)) {
        case WfbPacketType::data:
            ++data;
            break;
        case WfbPacketType::session:
            ++sessions;
            break;
        case WfbPacketType::unknown:
            ++unknown;
            break;
        }
    }
    const auto duration_ms = frames > 1 ? (last_time - first_time) / 1'000'000 : 0;
    std::cout << "{\"format\":\"fpv4cap\",\"version\":" << capture_format_version
              << ",\"frames\":" << frames << ",\"bytes\":" << bytes
              << ",\"duration_ms\":" << duration_ms << ",\"wfb_data\":" << data
              << ",\"wfb_session\":" << sessions << ",\"unknown\":" << unknown
              << ",\"crc_errors\":" << crc_errors << "}\n";
    return EXIT_SUCCESS;
}

int run_replay_command(int argc, char* argv[]) {
    const auto input = required_input_path(argc, argv);
    bool realtime{};
    for (int index = 2; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--speed" && index + 1 < argc) {
            const std::string_view speed(argv[++index]);
            if (speed != "max" && speed != "realtime") {
                throw std::runtime_error("speed must be max or realtime");
            }
            realtime = speed == "realtime";
        } else if (std::string_view(argv[index]) == "--input") {
            ++index;
        }
    }
    CaptureReader reader(input);
    std::uint64_t previous_time{};
    while (auto frame = reader.next()) {
        if (realtime && previous_time != 0 && frame->capture_time_ns > previous_time) {
            std::this_thread::sleep_for(
                std::chrono::nanoseconds(frame->capture_time_ns - previous_time));
        }
        previous_time = frame->capture_time_ns;
        std::cout << serialize_devourer_frame(*frame) << '\n' << std::flush;
    }
    return EXIT_SUCCESS;
}

} // namespace fpv4mac
