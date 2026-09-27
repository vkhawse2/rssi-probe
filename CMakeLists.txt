cmake_minimum_required(VERSION 3.16)
project(rssi-probe LANGUAGES CXX)

# Standalone RSSI probe tool: a portable Windows executable that shows live
# Bluetooth signal (RSSI) values and the movement-detector verdict, so
# Adaptive Volume thresholds can be tuned from real data.
#
# Extracted from vkhawse2/Sound-connect (tools/rssi-probe) on 2026-09-27.
# MovementDetector / RssiWatcher are vendored snapshots of the app sources
# at extraction time — this repo builds on its own, with no dependency on
# the Sound-connect tree.

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)

if(NOT Qt6_FOUND)
    message(WARNING "Qt 6 (Core, Gui, Widgets) not found — skipping rssi-probe.")
    return()
endif()

add_executable(rssi-probe
    main.cpp
    ProbeWindow.cpp
    ProbeWindow.h
    BleDiscovery.cpp
    BleDiscovery.h
    # Vendored from the Sound-connect app so the values on screen are the
    # values the app would act on.
    MovementDetector.cpp
    MovementDetector.h
    RssiWatcher.cpp
    RssiWatcher.h
)

target_include_directories(rssi-probe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rssi-probe PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

if(WIN32)
    # C++/WinRT BLE APIs, same as the main app.
    target_link_libraries(rssi-probe PRIVATE WindowsApp)
endif()
