cmake_minimum_required(VERSION 3.16)
project(MiluDpsMeter LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR "CMAKE_BUILD_TYPE not specified")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
)

set(SOURCE_FILES
    "DpsLogic.cpp"
    "SWPacketCapture.cpp"
    "MainWindow.cpp"
    "PacketCapture.cpp"
    "TitleBar.cpp"
    "main.cpp"
)
set(HEADER_FILES
    "DpsLogic.hpp"
    "SWPacketCapture.hpp"
    "SWPacketStructs.hpp"
    "PacketCapture.hpp"
    "MainWindow.hpp"
    "TitleBar.hpp"
)

if(NOT WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PCAP REQUIRED libpcap)

    list(APPEND SOURCE_FILES
        "PCap.cpp"
    )
    list(APPEND HEADER_FILES
        "PCap.hpp"
    )
else()
    set(WINDIVERT_INCLUDE_DIRS "" CACHE STRING "Path to WinDivert header directory")
    set(WINDIVERT_LINK_LIBRARIES "" CACHE STRING "Path to WinDivert link library file")

    list(APPEND SOURCE_FILES
        "WinDivert.cpp"
    )
    list(APPEND HEADER_FILES
        "WinDivert.hpp"
    )
endif()

set(OTHER_FILES "Font.qrc")

if(WIN32 AND NOT CMAKE_BUILD_TYPE MATCHES "Deb")
    set(SET_MANIFESTUAC ON)
    set(WIN32_EXEC WIN32)
else()
    unset(WIN32_EXEC)
endif()

if(SET_MANIFESTUAC AND CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    list(APPEND OTHER_FILES "MinGW.rc")
endif()

add_executable(${PROJECT_NAME} ${WIN32_EXEC}
    ${SOURCE_FILES}
    ${HEADER_FILES}
    ${OTHER_FILES}
)

if(SET_MANIFESTUAC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
    )
endif()

if(WIN32)
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/qt.conf" CONTENT
        "[Platforms]\nWindowsArguments = fontengine=freetype\n"
    )
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    -DMILU_DPS_METER_VERSION="0.1.1"
)

target_precompile_headers(${PROJECT_NAME} PRIVATE
    ${HEADER_FILES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${WINDIVERT_INCLUDE_DIRS}
    ${PCAP_INCLUDE_DIRS}
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt::Core
    Qt::Widgets
    ${WINDIVERT_LINK_LIBRARIES}
    ${PCAP_LINK_LIBRARIES}
)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ws2_32
    )
endif()
