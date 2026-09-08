#include "fpv4mac/devourer_json.hpp"
#include "fpv4mac/capture.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <sstream>

namespace fpv4mac {
namespace {

std::optional<std::string_view> find_value(std::string_view line, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\":";
    const auto position = line.find(needle);
    if (position == std::string_view::npos) {
        return std::nullopt;
    }
    return line.substr(position + needle.size());
}

std::optional<std::string> string_value(std::string_view line, std::string_view key) {
    auto value = find_value(line, key);
    if (!value || value->empty() || value->front() != '"') {
        return std::nullopt;
    }
    const auto end = value->find('"', 1);
    if (end == std::string_view::npos) {
        return std::nullopt;
    }
    return std::string(value->substr(1, end - 1));
}

template <typename T>
std::optional<T> integer_value(std::string_view line, std::string_view key) {
    const auto value = find_value(line, key);
    if (!value) {
        return std::nullopt;
    }
    long long parsed{};
    const char* begin = value->data();
    const char* end = begin + value->size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || parsed < std::numeric_limits<T>::min() ||
        parsed > std::numeric_limits<T>::max()) {
        return std::nullopt;
    }
    return static_cast<T>(parsed);
}

template <typename T>
std::optional<std::array<T, 2>> integer_pair(std::string_view line, std::string_view key) {
    const auto value = find_value(line, key);
    if (!value || value->empty() || value->front() != '[') {
        return std::nullopt;
    }
    const auto comma = value->find(',');
    const auto close = value->find(']');
    if (comma == std::string_view::npos || close == std::string_view::npos || comma > close) {
        return std::nullopt;
    }
    const auto parse = [](std::string_view text) -> std::optional<T> {
        while (!text.empty() &&
               (text.front() == '[' ||
                std::isspace(static_cast<unsigned char>(text.front())) != 0)) {
            text.remove_prefix(1);
        }
        long long parsed{};
        const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
        if (result.ec != std::errc{} || parsed < std::numeric_limits<T>::min() ||
            parsed > std::numeric_limits<T>::max()) {
            return std::nullopt;
        }
        return static_cast<T>(parsed);
    };
    const auto first = parse(value->substr(0, comma));
    const auto second = parse(value->substr(comma + 1, close - comma - 1));
    if (!first || !second) {
        return std::nullopt;
    }
    return std::array<T, 2>{*first, *second};
}

int hex_nibble(const char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

std::optional<std::vector<std::uint8_t>> decode_hex(std::string_view input) {
    if (input.size() % 2 != 0) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> output;
    output.reserve(input.size() / 2);
    for (std::size_t index = 0; index < input.size(); index += 2) {
        const int high = hex_nibble(input[index]);
        const int low = hex_nibble(input[index + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        output.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return output;
}

std::string encode_hex(const std::uint8_t* data, const std::size_t size) {
    constexpr char alphabet[] = "0123456789abcdef";
    std::string output(size * 2, '0');
    for (std::size_t index = 0; index < size; ++index) {
        output[index * 2] = alphabet[data[index] >> 4];
        output[index * 2 + 1] = alphabet[data[index] & 0x0f];
    }
    return output;
}

} // namespace

bool is_devourer_frame_event(const std::string_view line) {
    return line.starts_with("{\"ev\":\"rx.frame\"");
}

std::optional<RadioFrame> parse_devourer_frame(const std::string_view line, std::string* error) {
    const auto fail = [error](const std::string& message) -> std::optional<RadioFrame> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };
    if (!is_devourer_frame_event(line)) {
        return fail("not an rx.frame event");
    }

    const auto body_text = string_value(line, "body");
    const auto source_text = string_value(line, "sa");
    const auto body = body_text ? decode_hex(*body_text) : std::nullopt;
    const auto source = source_text ? decode_hex(*source_text) : std::nullopt;
    const auto rssi = integer_pair<std::int8_t>(line, "rssi");
    const auto snr = integer_pair<std::int8_t>(line, "snr");
    const auto evm = integer_pair<std::int8_t>(line, "evm");
    if (!body || !source || source->size() != 6 || !rssi || !snr || !evm) {
        return fail("missing or malformed body, source address, or radio metrics");
    }
    if (body->size() > maximum_capture_body_size) {
        return fail("frame body exceeds capture limit");
    }

    RadioFrame frame;
    frame.body = *body;
    std::copy(source->begin(), source->end(), frame.source_address.begin());
    frame.rssi = *rssi;
    frame.snr = *snr;
    frame.evm = *evm;
    frame.tsfl = integer_value<std::uint32_t>(line, "tsfl").value_or(0);
    frame.sequence = integer_value<std::uint16_t>(line, "seq").value_or(0);
    frame.rate = integer_value<std::uint16_t>(line, "rate").value_or(0);
    frame.bandwidth = integer_value<std::uint8_t>(line, "bw").value_or(0);
    frame.crc_error = integer_value<std::uint8_t>(line, "crc").value_or(0) != 0;
    frame.icv_error = integer_value<std::uint8_t>(line, "icv").value_or(0) != 0;
    frame.ldpc = integer_value<std::uint8_t>(line, "ldpc").value_or(0) != 0;
    frame.short_guard_interval = integer_value<std::uint8_t>(line, "sgi").value_or(0) != 0;
    return frame;
}

std::string serialize_devourer_frame(const RadioFrame& frame) {
    std::ostringstream output;
    output << "{\"ev\":\"rx.frame\",\"rate\":" << frame.rate
           << ",\"len\":" << frame.body.size() + 24
           << ",\"crc\":" << frame.crc_error << ",\"icv\":" << frame.icv_error
           << ",\"rssi\":[" << static_cast<int>(frame.rssi[0]) << ','
           << static_cast<int>(frame.rssi[1]) << "],\"evm\":["
           << static_cast<int>(frame.evm[0]) << ',' << static_cast<int>(frame.evm[1])
           << "],\"snr\":[" << static_cast<int>(frame.snr[0]) << ','
           << static_cast<int>(frame.snr[1]) << "],\"seq\":" << frame.sequence
           << ",\"tsfl\":" << frame.tsfl << ",\"bw\":"
           << static_cast<int>(frame.bandwidth) << ",\"ldpc\":" << frame.ldpc
           << ",\"sgi\":" << frame.short_guard_interval << ",\"sa\":\""
           << encode_hex(frame.source_address.data(), frame.source_address.size())
           << "\",\"body\":\"" << encode_hex(frame.body.data(), frame.body.size()) << "\"}";
    return output.str();
}

} // namespace fpv4mac
