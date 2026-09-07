#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fpv4mac {

struct UsbDeviceProbe {
    std::uint16_t vendor_id{};
    std::uint16_t product_id{};
    std::uint8_t bus{};
    std::uint8_t address{};
    std::string manufacturer;
    std::string product;
    std::string chipset;
    std::string support_note;
    std::string speed;
    bool supported{};
    bool opened{};
    bool interface_claimed{};
    int interface_number{};
    int endpoint_count{};
    int kernel_driver_active{-1};
    std::string error;
};

struct UsbProbeReport {
    std::vector<UsbDeviceProbe> devices;
    bool compatible_device_found{};
    bool compatible_device_ready{};
};

UsbProbeReport probe_usb_devices(bool claim_interface);

} // namespace fpv4mac
