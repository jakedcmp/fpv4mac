#include "fpv4mac/capture_commands.hpp"
#include "fpv4mac/device_catalog.hpp"
#include "fpv4mac/usb_probe.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view version = "0.2.0";

std::string json_escape(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (const char character : input) {
        switch (character) {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output += character;
        }
    }
    return output;
}
void print_help() {
    std::cout << "fpv4mac " << version << "\n"
              << "Native macOS ground receiver tooling for OpenIPC/WFB-NG links.\n\n"
              << "Usage:\n"
              << "  fpv4mac doctor [--json] [--no-claim]\n"
              << "  fpv4mac capture --output FILE [--input FILE|-] [--passthrough]\n"
              << "  fpv4mac inspect --input FILE\n"
              << "  fpv4mac replay --input FILE [--speed max|realtime]\n"
              << "  fpv4mac --version\n\n"
              << "doctor safely enumerates compatible USB receivers and, by default,\n"
              << "claims and immediately releases interface 0 to prove readiness.\n";
}

void print_human(const fpv4mac::UsbProbeReport& report, const bool claim_interface) {
    std::cout << "fpv4mac doctor\n\n";
    if (!report.compatible_device_found) {
        std::cout << "Receiver:             not found\n"
                  << "Expected USB ID:       0bda:8812 (RTL8812AU)\n";
        return;
    }

    for (const auto& device : report.devices) {
        if (!device.supported) {
            continue;
        }
        std::cout << "Receiver:             " << device.chipset << '\n'
                  << "USB ID:               "
                  << fpv4mac::format_usb_id(device.vendor_id, device.product_id) << '\n'
                  << "USB location:         bus " << static_cast<int>(device.bus) << ", address "
                  << static_cast<int>(device.address) << '\n'
                  << "USB transport:        " << device.speed << '\n'
                  << "Reported product:     "
                  << (device.product.empty() ? "unavailable" : device.product) << '\n'
                  << "Interface:            " << device.interface_number << " ("
                  << device.endpoint_count << " endpoints)\n"
                  << "Device open:          " << (device.opened ? "yes" : "no") << '\n';
        if (claim_interface) {
            std::cout << "Interface claim:      "
                      << (device.interface_claimed ? "passed (released cleanly)" : "failed")
                      << '\n';
        } else {
            std::cout << "Interface claim:      skipped\n";
        }
        std::cout << "Radio initialization: available through scripts/radio-smoke-test.sh\n"
                  << "Live receive:         available through scripts/receive.sh\n";
        if (!device.error.empty()) {
            std::cout << "Error:                " << device.error << '\n';
        }
    }
}

void print_json(const fpv4mac::UsbProbeReport& report) {
    std::cout << "{\"version\":\"" << version << "\",\"compatible_device_found\":"
              << (report.compatible_device_found ? "true" : "false")
              << ",\"compatible_device_ready\":"
              << (report.compatible_device_ready ? "true" : "false") << ",\"devices\":[";

    bool first = true;
    for (const auto& device : report.devices) {
        if (!device.supported) {
            continue;
        }
        if (!first) {
            std::cout << ',';
        }
        first = false;
        std::cout << "{\"usb_id\":\""
                  << fpv4mac::format_usb_id(device.vendor_id, device.product_id)
                  << "\",\"chipset\":\"" << json_escape(device.chipset)
                  << "\",\"product\":\"" << json_escape(device.product)
                  << "\",\"bus\":" << static_cast<int>(device.bus)
                  << ",\"address\":" << static_cast<int>(device.address)
                  << ",\"speed\":\"" << json_escape(device.speed)
                  << "\",\"opened\":" << (device.opened ? "true" : "false")
                  << ",\"interface\":" << device.interface_number
                  << ",\"endpoint_count\":" << device.endpoint_count
                  << ",\"interface_claimed\":"
                  << (device.interface_claimed ? "true" : "false")
                  << ",\"error\":\"" << json_escape(device.error) << "\"}";
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc > 1) {
        try {
            const std::string_view requested(argv[1]);
            if (requested == "capture") {
                return fpv4mac::run_capture_command(argc, argv);
            }
            if (requested == "inspect") {
                return fpv4mac::run_inspect_command(argc, argv);
            }
            if (requested == "replay") {
                return fpv4mac::run_replay_command(argc, argv);
            }
        } catch (const std::exception& error) {
            std::cerr << "fpv4mac: " << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }

    bool json = false;
    bool claim_interface = true;
    std::string command = "doctor";

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "doctor") {
            command = "doctor";
        } else if (argument == "--json") {
            json = true;
        } else if (argument == "--no-claim") {
            claim_interface = false;
        } else if (argument == "--version") {
            std::cout << "fpv4mac " << version << '\n';
            return EXIT_SUCCESS;
        } else if (argument == "--help" || argument == "-h") {
            print_help();
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Unknown argument: " << argument << "\n\n";
            print_help();
            return EXIT_FAILURE;
        }
    }

    if (command != "doctor") {
        print_help();
        return EXIT_FAILURE;
    }

    const auto report = fpv4mac::probe_usb_devices(claim_interface);
    if (json) {
        print_json(report);
    } else {
        print_human(report, claim_interface);
    }

    if (!report.compatible_device_found) {
        return 2;
    }
    return report.compatible_device_ready ? EXIT_SUCCESS : 3;
}
