cmake_minimum_required(VERSION 3.20)

project(fpv4mac_wfb_bridge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if(NOT WFB_NG_SOURCE_DIR)
    message(FATAL_ERROR "Set WFB_NG_SOURCE_DIR to a pinned wfb-ng checkout")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
find_library(PCAP_LIBRARY pcap REQUIRED)

add_library(wfb_ng_aggregator STATIC
    ${WFB_NG_SOURCE_DIR}/src/rx.cpp
    ${WFB_NG_SOURCE_DIR}/src/radiotap.c
    ${WFB_NG_SOURCE_DIR}/src/wifibroadcast.cpp
    ${WFB_NG_SOURCE_DIR}/src/zfex.c
)
target_include_directories(wfb_ng_aggregator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${WFB_NG_SOURCE_DIR}/src
)
target_compile_definitions(wfb_ng_aggregator PRIVATE
    __WFB_RX_SHARED_LIBRARY__=1
    WFB_VERSION="fpv4mac"
)
target_compile_options(wfb_ng_aggregator PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/compat/macos.hpp
    -fno-strict-aliasing
)
target_link_libraries(wfb_ng_aggregator PUBLIC PkgConfig::SODIUM ${PCAP_LIBRARY})

add_executable(fpv4mac-wfb
    main.cpp
    protocol.cpp
    ../../src/devourer_json.cpp
    ../../src/radio_frame.cpp
)
target_include_directories(fpv4mac-wfb PRIVATE ../../include)
target_link_libraries(fpv4mac-wfb PRIVATE wfb_ng_aggregator)
target_compile_options(fpv4mac-wfb PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(wfb_bridge_protocol_test
        protocol_test.cpp
        protocol.cpp
    )
    target_include_directories(wfb_bridge_protocol_test PRIVATE ../../include)
    target_compile_options(wfb_bridge_protocol_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME wfb_bridge_protocol_test COMMAND wfb_bridge_protocol_test)

    add_executable(wfb_bridge_integration_test
        integration_test.cpp
        ../../src/devourer_json.cpp
        ../../src/radio_frame.cpp
    )
    target_include_directories(wfb_bridge_integration_test PRIVATE ../../include)
    target_link_libraries(wfb_bridge_integration_test PRIVATE wfb_ng_aggregator)
    add_test(NAME wfb_bridge_integration_test COMMAND wfb_bridge_integration_test)
endif()
