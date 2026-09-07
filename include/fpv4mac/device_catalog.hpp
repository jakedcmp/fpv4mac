#pragma once

#include <cstdint>
#include <string>

namespace fpv4mac {

struct DeviceSupport {
    bool supported;
    std::string chipset;
    std::string note;
};

DeviceSupport classify_device(std::uint16_t vendor_id, std::uint16_t product_id);
std::string format_usb_id(std::uint16_t vendor_id, std::uint16_t product_id);

} // namespace fpv4mac
