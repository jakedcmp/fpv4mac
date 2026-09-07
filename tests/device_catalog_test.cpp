#include "fpv4mac/device_catalog.hpp"

#include <cassert>

int main() {
    const auto rtl8812au = fpv4mac::classify_device(0x0bda, 0x8812);
    assert(rtl8812au.supported);
    assert(rtl8812au.chipset == "RTL8812AU");
    assert(fpv4mac::format_usb_id(0x0bda, 0x8812) == "0bda:8812");

    const auto unknown = fpv4mac::classify_device(0x1234, 0xabcd);
    assert(!unknown.supported);
    assert(unknown.chipset == "unknown");
    return 0;
}
