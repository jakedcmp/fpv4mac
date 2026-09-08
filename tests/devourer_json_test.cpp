#include "fpv4mac/devourer_json.hpp"

#include <stdexcept>

namespace {
void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main() {
    constexpr auto line =
        R"({"ev":"rx.frame","rate":260,"len":32,"crc":0,"icv":0,"rssi":[-42,-45],"evm":[-20,-21],"snr":[30,28],"seq":123,"tsfl":456789,"bw":20,"ldpc":1,"sgi":0,"sa":"574200000001","body":"02010203aabbccdd"})";
    std::string error;
    const auto parsed = fpv4mac::parse_devourer_frame(line, &error);
    require(parsed.has_value(), "valid frame should parse");
    require(error.empty(), "valid frame should not set an error");
    require(parsed->rate == 260, "rate mismatch");
    require(parsed->rssi[0] == -42, "RSSI mismatch");
    require(parsed->source_address[0] == 0x57, "source address mismatch");
    require(parsed->body.size() == 8, "body length mismatch");
    require(fpv4mac::classify_wfb_packet(*parsed) == fpv4mac::WfbPacketType::session,
            "WFB session classification mismatch");

    const auto encoded = fpv4mac::serialize_devourer_frame(*parsed);
    const auto reparsed = fpv4mac::parse_devourer_frame(encoded, &error);
    require(reparsed.has_value(), "serialized frame should reparse");
    require(reparsed->body == parsed->body, "round-trip body mismatch");
    require(reparsed->source_address == parsed->source_address,
            "round-trip source address mismatch");
    require(!fpv4mac::parse_devourer_frame("{}", &error), "non-frame JSON should be rejected");
    return 0;
}
