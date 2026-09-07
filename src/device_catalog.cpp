#include "fpv4mac/device_catalog.hpp"

#include <iomanip>
#include <sstream>

namespace fpv4mac {

DeviceSupport classify_device(const std::uint16_t vendor_id, const std::uint16_t product_id) {
    if (vendor_id == 0x0bda && product_id == 0x8812) {
        return {
            .supported = true,
            .chipset = "RTL8812AU",
            .note = "Primary fpv4mac development target; supported by OpenIPC devourer",
        };
    }

    return {
        .supported = false,
        .chipset = "unknown",
        .note = "Not yet listed in the fpv4mac compatibility catalog",
    };
}

std::string format_usb_id(const std::uint16_t vendor_id, const std::uint16_t product_id) {
    std::ostringstream value;
    value << std::hex << std::setfill('0') << std::setw(4) << vendor_id << ':' << std::setw(4)
          << product_id;
    return value.str();
}

} // namespace fpv4mac
