#pragma once

#include "fpv4mac/radio_frame.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace fpv4mac {

bool is_devourer_frame_event(std::string_view line);
std::optional<RadioFrame> parse_devourer_frame(std::string_view line, std::string* error = nullptr);
std::string serialize_devourer_frame(const RadioFrame& frame);

} // namespace fpv4mac
