cmake_minimum_required(VERSION 3.20)

project(fpv4mac VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

add_library(fpv4mac_core
    src/capture.cpp
    src/capture_commands.cpp
    src/devourer_json.cpp
    src/device_catalog.cpp
    src/radio_frame.cpp
    src/usb_probe.cpp
)
target_include_directories(fpv4mac_core PUBLIC include)
target_link_libraries(fpv4mac_core PUBLIC PkgConfig::LIBUSB)
target_compile_options(fpv4mac_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(fpv4mac src/main.cpp)
target_link_libraries(fpv4mac PRIVATE fpv4mac_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(device_catalog_test tests/device_catalog_test.cpp)
    target_link_libraries(device_catalog_test PRIVATE fpv4mac_core)
    add_test(NAME device_catalog_test COMMAND device_catalog_test)

    add_executable(capture_test tests/capture_test.cpp)
    target_link_libraries(capture_test PRIVATE fpv4mac_core)
    add_test(NAME capture_test COMMAND capture_test)

    add_executable(devourer_json_test tests/devourer_json_test.cpp)
    target_link_libraries(devourer_json_test PRIVATE fpv4mac_core)
    add_test(NAME devourer_json_test COMMAND devourer_json_test)
endif()

install(TARGETS fpv4mac RUNTIME DESTINATION bin)
