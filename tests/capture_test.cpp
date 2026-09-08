#include "fpv4mac/capture.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace {
void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      ("fpv4mac-capture-test-" + std::to_string(suffix) + ".fpv4cap");

    fpv4mac::RadioFrame expected;
    expected.capture_time_ns = 1'250'000;
    expected.tsfl = 42;
    expected.sequence = 7;
    expected.rate = 260;
    expected.bandwidth = 20;
    expected.ldpc = true;
    expected.rssi = {-50, -51};
    expected.snr = {25, 24};
    expected.evm = {-18, -19};
    expected.source_address = {0x57, 0x42, 0, 0, 0, 1};
    expected.body = {0x01, 0x02, 0x03, 0xaa, 0xbb, 0xcc, 0xdd};
    require(fpv4mac::classify_wfb_packet(expected) == fpv4mac::WfbPacketType::data,
            "WFB data classification mismatch");
    auto ambient = expected;
    ambient.source_address[0] = 0xaa;
    require(fpv4mac::classify_wfb_packet(ambient) == fpv4mac::WfbPacketType::unknown,
            "ambient source should not be classified as WFB");

    {
        fpv4mac::CaptureWriter writer(path);
        writer.write(expected);
        require(writer.frame_count() == 1, "capture writer count mismatch");
    }
    {
        fpv4mac::CaptureReader reader(path);
        const auto actual = reader.next();
        require(actual.has_value(), "capture should contain one frame");
        require(actual->capture_time_ns == expected.capture_time_ns, "capture time mismatch");
        require(actual->source_address == expected.source_address, "source address mismatch");
        require(actual->body == expected.body, "capture body mismatch");
        require(actual->ldpc, "LDPC flag mismatch");
        require(!reader.next(), "capture should contain exactly one frame");
    }
    std::filesystem::remove(path);
    return 0;
}
