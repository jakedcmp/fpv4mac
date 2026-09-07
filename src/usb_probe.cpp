#include "fpv4mac/usb_probe.hpp"

#include "fpv4mac/device_catalog.hpp"

#include <libusb.h>

#include <memory>
#include <utility>

namespace fpv4mac {
namespace {

struct ContextDeleter {
    void operator()(libusb_context* context) const {
        if (context != nullptr) {
            libusb_exit(context);
        }
    }
};

struct DeviceListDeleter {
    void operator()(libusb_device** devices) const {
        if (devices != nullptr) {
            libusb_free_device_list(devices, 1);
        }
    }
};

struct HandleDeleter {
    void operator()(libusb_device_handle* handle) const {
        if (handle != nullptr) {
            libusb_close(handle);
        }
    }
};

std::string read_string(libusb_device_handle* handle, const std::uint8_t index) {
    if (handle == nullptr || index == 0) {
        return {};
    }

    unsigned char buffer[256]{};
    const int length =
        libusb_get_string_descriptor_ascii(handle, index, buffer, sizeof(buffer) - 1);
    if (length <= 0) {
        return {};
    }
    return {reinterpret_cast<char*>(buffer), static_cast<std::size_t>(length)};
}

std::string speed_name(const int speed) {
    switch (speed) {
    case LIBUSB_SPEED_LOW:
        return "1.5 Mbps (low speed)";
    case LIBUSB_SPEED_FULL:
        return "12 Mbps (full speed)";
    case LIBUSB_SPEED_HIGH:
        return "480 Mbps (high speed)";
    case LIBUSB_SPEED_SUPER:
        return "5 Gbps (SuperSpeed)";
    case LIBUSB_SPEED_SUPER_PLUS:
        return "10+ Gbps (SuperSpeedPlus)";
    default:
        return "unknown";
    }
}

void inspect_active_interface(libusb_device* device, UsbDeviceProbe& probe) {
    libusb_config_descriptor* raw_config = nullptr;
    if (libusb_get_active_config_descriptor(device, &raw_config) != LIBUSB_SUCCESS ||
        raw_config == nullptr) {
        return;
    }

    std::unique_ptr<libusb_config_descriptor, decltype(&libusb_free_config_descriptor)> config(
        raw_config, libusb_free_config_descriptor);
    if (config->bNumInterfaces == 0 || config->interface[0].num_altsetting == 0) {
        return;
    }

    const auto& descriptor = config->interface[0].altsetting[0];
    probe.interface_number = descriptor.bInterfaceNumber;
    probe.endpoint_count = descriptor.bNumEndpoints;
}

} // namespace

UsbProbeReport probe_usb_devices(const bool claim_interface) {
    UsbProbeReport report;

    libusb_context* raw_context = nullptr;
    const int init_result = libusb_init(&raw_context);
    if (init_result != LIBUSB_SUCCESS) {
        UsbDeviceProbe failure;
        failure.error = std::string("libusb initialization failed: ") +
                        libusb_error_name(init_result);
        report.devices.push_back(std::move(failure));
        return report;
    }
    std::unique_ptr<libusb_context, ContextDeleter> context(raw_context);

    libusb_device** raw_devices = nullptr;
    const ssize_t device_count = libusb_get_device_list(context.get(), &raw_devices);
    if (device_count < 0) {
        UsbDeviceProbe failure;
        failure.error = std::string("USB enumeration failed: ") +
                        libusb_error_name(static_cast<int>(device_count));
        report.devices.push_back(std::move(failure));
        return report;
    }
    std::unique_ptr<libusb_device*, DeviceListDeleter> devices(raw_devices);

    for (ssize_t index = 0; index < device_count; ++index) {
        libusb_device* device = raw_devices[index];
        libusb_device_descriptor descriptor{};
        if (libusb_get_device_descriptor(device, &descriptor) != LIBUSB_SUCCESS) {
            continue;
        }

        const auto support = classify_device(descriptor.idVendor, descriptor.idProduct);
        if (!support.supported) {
            continue;
        }

        UsbDeviceProbe probe{
            .vendor_id = descriptor.idVendor,
            .product_id = descriptor.idProduct,
            .bus = libusb_get_bus_number(device),
            .address = libusb_get_device_address(device),
            .chipset = support.chipset,
            .support_note = support.note,
            .speed = speed_name(libusb_get_device_speed(device)),
            .supported = true,
        };
        report.compatible_device_found = true;
        inspect_active_interface(device, probe);

        libusb_device_handle* raw_handle = nullptr;
        const int open_result = libusb_open(device, &raw_handle);
        if (open_result != LIBUSB_SUCCESS) {
            probe.error = std::string("open failed: ") + libusb_error_name(open_result);
            report.devices.push_back(std::move(probe));
            continue;
        }

        std::unique_ptr<libusb_device_handle, HandleDeleter> handle(raw_handle);
        probe.opened = true;
        probe.manufacturer = read_string(handle.get(), descriptor.iManufacturer);
        probe.product = read_string(handle.get(), descriptor.iProduct);
        probe.kernel_driver_active =
            libusb_kernel_driver_active(handle.get(), probe.interface_number);

        if (!claim_interface) {
            report.compatible_device_ready = true;
            report.devices.push_back(std::move(probe));
            continue;
        }

        const int claim_result = libusb_claim_interface(handle.get(), probe.interface_number);
        if (claim_result != LIBUSB_SUCCESS) {
            probe.error = std::string("interface claim failed: ") +
                          libusb_error_name(claim_result);
            report.devices.push_back(std::move(probe));
            continue;
        }

        probe.interface_claimed = true;
        report.compatible_device_ready = true;
        libusb_release_interface(handle.get(), probe.interface_number);
        report.devices.push_back(std::move(probe));
    }

    return report;
}

} // namespace fpv4mac
