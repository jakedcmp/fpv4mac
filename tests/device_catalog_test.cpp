#include "fpv4mac/device_catalog.hpp"

#include <stdexcept>

namespace {
void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main() {
    const auto rtl8812au = fpv4mac::classify_device(0x0bda, 0x8812);
    require(rtl8812au.supported, "RTL8812AU should be supported");
    require(rtl8812au.chipset == "RTL8812AU", "chipset name mismatch");
    require(fpv4mac::format_usb_id(0x0bda, 0x8812) == "0bda:8812", "USB ID mismatch");

    const auto unknown = fpv4mac::classify_device(0x1234, 0xabcd);
    require(!unknown.supported, "unknown adapter should not be supported");
    require(unknown.chipset == "unknown", "unknown chipset name mismatch");
    return 0;
}
