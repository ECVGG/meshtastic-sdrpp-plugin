# Meshtastic SDR++ Plugin - Out-of-tree CMake Build System
# Copyright (c) 2024 Victor

cmake_minimum_required(VERSION 3.16)
project(MeshtasticPlugin
    VERSION 1.0.0
    LANGUAGES CXX
    DESCRIPTION "Meshtastic protocol decoder/decryptor plugin for SDR++"
    HOMEPAGE_URL "https://github.com/yourusername/meshtastic-sdrpp-plugin"
)

# =============================================================================
# Version Management
# =============================================================================
set(MESHTASTIC_PLUGIN_VERSION_MAJOR 1)
set(MESHTASTIC_PLUGIN_VERSION_MINOR 0)
set(MESHTASTIC_PLUGIN_VERSION_PATCH 0)
set(MESHTASTIC_PLUGIN_VERSION "${MESHTASTIC_PLUGIN_VERSION_MAJOR}.${MESHTASTIC_PLUGIN_VERSION_MINOR}.${MESHTASTIC_PLUGIN_VERSION_PATCH}")

# Version header generation
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/Version.h"
    @ONLY
)

# =============================================================================
# CMake Policies and Settings
# =============================================================================
# Modern CMake practices
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
endif()
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)  # FindPython3 prefers newest
endif()

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Build date for version header
string(TIMESTAMP BUILD_DATE "%Y-%m-%d %H:%M:%S" UTC)

# =============================================================================
# Find SDR++ Core
# =============================================================================
# Allow user to specify SDR++ install prefix or build directory
set(SDRPP_CORE_ROOT "" CACHE PATH "Path to SDR++ installation or build directory")
set(SDRPP_CMAKE_DIR "${SDRPP_CORE_ROOT}/lib/cmake/SDRPlusPlus" CACHE PATH "Path to SDR++ CMake config directory")

find_package(SDRPlusPlus REQUIRED
    PATHS "${SDRPP_CMAKE_DIR}"
    NO_DEFAULT_PATH
)

message(STATUS "Found SDR++: ${SDRPlusPlus_VERSION}")
message(STATUS "SDR++ include dirs: ${SDRPlusPlus_INCLUDE_DIRS}")
message(STATUS "SDR++ library dirs: ${SDRPlusPlus_LIBRARY_DIRS}")

# =============================================================================
# Find Dependencies - FORCE STATIC LINKING for single-file plugin
# =============================================================================

# Force static libraries for all dependencies
set(Protobuf_USE_STATIC_LIBS ON)
set(CRYPTOPP_STATIC ON)
set(BUILD_SHARED_LIBS OFF)

# Protobuf - Required for Meshtastic protocol definitions
# Add hints for Homebrew on macOS (both Intel /usr/local and Apple Silicon /opt/homebrew)
set(Protobuf_ROOT_HINTS
    /usr/local
    /opt/homebrew
    ${CMAKE_PREFIX_PATH}
)
find_package(Protobuf REQUIRED)
if(NOT Protobuf_FOUND)
    # Try manual find as fallback
    find_path(PROTOBUF_INCLUDE_DIR google/protobuf/message.h
        PATHS /usr/include /usr/local/include /opt/homebrew/include ${CMAKE_PREFIX_PATH}/include
    )
    find_library(PROTOBUF_LIBRARY protobuf
        PATHS /usr/lib /usr/local/lib /opt/homebrew/lib ${CMAKE_PREFIX_PATH}/lib
    )
    find_program(PROTOBUF_PROTOC protoc
        PATHS /usr/bin /usr/local/bin /opt/homebrew/bin ${CMAKE_PREFIX_PATH}/bin
    )
    if(PROTOBUF_INCLUDE_DIR AND PROTOBUF_LIBRARY AND PROTOBUF_PROTOC)
        set(Protobuf_FOUND TRUE)
        set(Protobuf_INCLUDE_DIRS ${PROTOBUF_INCLUDE_DIR})
        set(Protobuf_LIBRARIES ${PROTOBUF_LIBRARY})
        set(Protobuf_PROTOC_EXECUTABLE ${PROTOBUF_PROTOC})
        message(STATUS "Found Protobuf manually: ${PROTOBUF_INCLUDE_DIR}, ${PROTOBUF_LIBRARY}")
    else()
        message(FATAL_ERROR "Could not find Protobuf library, headers, and protoc")
    endif()
endif()
message(STATUS "Found Protobuf: ${Protobuf_VERSION}")
message(STATUS "Protobuf compiler: ${Protobuf_PROTOC_EXECUTABLE}")

# Crypto++ - Required for AES-CTR decryption
# Try find_package first, fall back to manual find for systems without CMake config (e.g., Ubuntu)
find_package(Cryptopp QUIET)
if(NOT Cryptopp_FOUND)
    message(STATUS "Cryptopp config not found, trying manual find...")
    find_path(CRYPTOPP_INCLUDE_DIR cryptopp/cryptlib.h
        PATHS /usr/include /usr/local/include /opt/homebrew/include ${CMAKE_PREFIX_PATH}/include
    )
    find_library(CRYPTOPP_LIBRARY cryptopp
        PATHS /usr/lib /usr/local/lib /opt/homebrew/lib ${CMAKE_PREFIX_PATH}/lib
    )
    if(CRYPTOPP_INCLUDE_DIR AND CRYPTOPP_LIBRARY)
        set(Cryptopp_FOUND TRUE)
        set(Cryptopp_INCLUDE_DIRS ${CRYPTOPP_INCLUDE_DIR})
        set(Cryptopp_LIBRARIES ${CRYPTOPP_LIBRARY})
        message(STATUS "Found Crypto++ manually: ${CRYPTOPP_INCLUDE_DIR}, ${CRYPTOPP_LIBRARY}")
    else()
        message(FATAL_ERROR "Could not find Crypto++ library and headers")
    endif()
else()
    message(STATUS "Found Crypto++: ${CRYPTOPP_VERSION}")
endif()

# Try to find other dependencies for static linking
find_package(absl QUIET)
if(absl_FOUND)
    message(STATUS "Found absl")
    set(HAVE_ABSL 1)
else()
    set(HAVE_ABSL 0)
endif()

find_package(fmt QUIET)
if(fmt_FOUND)
    message(STATUS "Found fmt: ${fmt_VERSION}")
    set(HAVE_FMT 1)
else()
    set(HAVE_FMT 0)
endif()

find_package(spdlog QUIET)
if(spdlog_FOUND)
    message(STATUS "Found spdlog")
    set(HAVE_SPDLOG 1)
else()
    set(HAVE_SPDLOG 0)
endif()

find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "Found nlohmann_json")
    set(HAVE_JSON 1)
else()
    # Try to find manually
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
        PATHS ${CMAKE_INSTALL_PREFIX}/include ${CMAKE_PREFIX_PATH}/include
        PATH_SUFFIXES nlohmann
    )
    if(NLOHMANN_JSON_INCLUDE_DIR)
        message(STATUS "Found nlohmann_json at: ${NLOHMANN_JSON_INCLUDE_DIR}")
        set(HAVE_JSON 1)
    else()
        set(HAVE_JSON 0)
    endif()
endif()

# liquid-dsp for LoRa demodulation
find_package(liquid QUIET)
if(liquid_FOUND)
    message(STATUS "Found liquid-dsp: ${liquid_VERSION}")
    set(HAVE_LIQUID 1)
else()
    message(STATUS "liquid-dsp not found - using built-in LoRa demodulator")
    set(HAVE_LIQUID 0)
endif()

# Check for ImGui (bundled with SDR++ but headers may not be in portable release)
find_path(IMGUI_INCLUDE_DIR imgui.h
    PATHS ${SDRPlusPlus_INCLUDE_DIRS} ${CMAKE_INSTALL_PREFIX}/include ${CMAKE_PREFIX_PATH}/include
)
if(IMGUI_INCLUDE_DIR)
    message(STATUS "Found ImGui at: ${IMGUI_INCLUDE_DIR}")
    set(HAVE_IMGUI 1)
else()
    message(STATUS "ImGui headers not found - GUI will be disabled")
    set(HAVE_IMGUI 0)
endif()

# Threads
find_package(Threads REQUIRED)

# =============================================================================
# Protobuf Code Generation
# =============================================================================
# Meshtastic proto files
set(MESHTASTIC_PROTO_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/mesh.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/telemetry.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/remote_hardware.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/admin.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/detection_sensor.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/serialization.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/neighbor_info.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/remote_logging.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/store_forward.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/local_only.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/device_ui.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/channel.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/radio.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/serial_interface.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/pax.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/routing.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/mqtt.proto"
)

# Verify proto files exist
foreach(proto_file ${MESHTASTIC_PROTO_FILES})
    if(NOT EXISTS "${proto_file}")
        message(WARNING "Proto file not found: ${proto_file}")
    endif()
endforeach()

# Generate C++ code from protobuf
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${MESHTASTIC_PROTO_FILES})
message(STATUS "Generated protobuf sources: ${PROTO_SRCS}")
message(STATUS "Generated protobuf headers: ${PROTO_HDRS}")

# Include directories for generated protobuf code
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${Protobuf_INCLUDE_DIRS})

# =============================================================================
# Plugin Source Files
# =============================================================================
set(PLUGIN_SOURCES
    src/MeshtasticPlugin.cpp
    src/MeshtasticDecoder.cpp
    src/MeshtasticDecryptor.cpp
    src/MeshtasticProtocol.cpp
    src/LoRaDemodulator.cpp
    ${PROTO_SRCS}
)

set(PLUGIN_HEADERS
    include/MeshtasticPlugin.h
    include/MeshtasticDecoder.h
    include/MeshtasticDecryptor.h
    include/MeshtasticProtocol.h
    include/LoRaDemodulator.h
    ${PROTO_HDRS}
)

# =============================================================================
# Create Shared Library (MODULE for SDR++ plugin)
# =============================================================================
add_library(MeshtasticPlugin MODULE ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})

# Target properties
set_target_properties(MeshtasticPlugin PROPERTIES
    OUTPUT_NAME "meshtastic"
    PREFIX ""
    SUFFIX "${SDRPlusPlus_PLUGIN_SUFFIX}"
    VERSION ${MESHTASTIC_PLUGIN_VERSION}
    SOVERSION ${MESHTASTIC_PLUGIN_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(MeshtasticPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SDRPlusPlus_INCLUDE_DIRS}
    ${Protobuf_INCLUDE_DIRS}
    ${CRYPTOPP_INCLUDE_DIRS}
)

# Add nlohmann_json include if found manually
if(HAVE_JSON AND NLOHMANN_JSON_INCLUDE_DIR)
    target_include_directories(MeshtasticPlugin PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})
endif()

# Add ImGui include if found
if(HAVE_IMGUI)
    target_include_directories(MeshtasticPlugin PRIVATE ${IMGUI_INCLUDE_DIR})
    target_compile_definitions(MeshtasticPlugin PRIVATE HAS_IMGUI=1)
else()
    target_compile_definitions(MeshtasticPlugin PRIVATE HAS_IMGUI=0)
endif()

# Add liquid-dsp include if found
if(HAVE_LIQUID)
    target_include_directories(MeshtasticPlugin PRIVATE ${liquid_INCLUDE_DIRS})
    target_compile_definitions(MeshtasticPlugin PRIVATE HAVE_LIQUID=1)
else()
    target_compile_definitions(MeshtasticPlugin PRIVATE HAVE_LIQUID=0)
endif()

# Compile definitions
target_compile_definitions(MeshtasticPlugin PRIVATE
    MESHTASTIC_PLUGIN_VERSION="${MESHTASTIC_PLUGIN_VERSION}"
    MESHTASTIC_PLUGIN_BUILD_DATE="\"${BUILD_DATE}\""
    PROTOBUF_USE_DLLS=OFF
)

# Link libraries - STATIC linking for single-file plugin
target_link_libraries(MeshtasticPlugin PRIVATE
    SDRPlusPlus::SDRPlusPlus
    ${Protobuf_LIBRARIES}
    ${CRYPTOPP_LIBRARIES}
    Threads::Threads
)

if(HAVE_ABSL)
    target_link_libraries(MeshtasticPlugin PRIVATE absl::base absl::strings absl::flags absl::flags_parse)
endif()

if(HAVE_FMT)
    target_link_libraries(MeshtasticPlugin PRIVATE fmt::fmt)
endif()

if(HAVE_SPDLOG)
    target_link_libraries(MeshtasticPlugin PRIVATE spdlog::spdlog)
endif()

if(nlohmann_json_FOUND)
    target_link_libraries(MeshtasticPlugin PRIVATE nlohmann_json::nlohmann_json)
endif()

if(HAVE_LIQUID)
    target_link_libraries(MeshtasticPlugin PRIVATE liquid::liquid)
endif()

# Windows: Force static runtime to avoid MSVCP dependency
if(WIN32 AND MSVC)
    target_compile_options(MeshtasticPlugin PRIVATE /MT)
    # Also set for all dependencies via CMake
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Platform-specific settings
if(WIN32)
    target_compile_definitions(MeshtasticPlugin PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(MeshtasticPlugin PRIVATE ws2_32)
elseif(APPLE)
    target_link_libraries(MeshtasticPlugin PRIVATE "-framework Foundation" "-framework Security")
endif()

# =============================================================================
# Installation
# =============================================================================
# Install plugin to SDR++ plugin directory
install(TARGETS MeshtasticPlugin
    LIBRARY DESTINATION "${SDRPlusPlus_PLUGIN_INSTALL_DIR}"
    RUNTIME DESTINATION "${SDRPlusPlus_PLUGIN_INSTALL_DIR}"
    ARCHIVE DESTINATION "${SDRPlusPlus_PLUGIN_INSTALL_DIR}"
)

# Install headers (for other plugins that might depend on this)
install(FILES
    include/MeshtasticPlugin.h
    include/MeshtasticDecoder.h
    include/MeshtasticDecryptor.h
    include/MeshtasticProtocol.h
    include/LoRaDemodulator.h
    DESTINATION include/sdrpp/plugins/meshtastic
)

# =============================================================================
# CPack Configuration
# =============================================================================
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "Meshtastic-SDRPP-Plugin")
set(CPACK_PACKAGE_VERSION ${MESHTASTIC_PLUGIN_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${MESHTASTIC_PLUGIN_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${MESHTASTIC_PLUGIN_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${MESHTASTIC_PLUGIN_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Meshtastic protocol decoder/decryptor plugin for SDR++")
set(CPACK_PACKAGE_VENDOR "Victor")
set(CPACK_PACKAGE_CONTACT "victor@example.com")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Platform-specific packaging
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_INSTALL_ROOT "${SDRPlusPlus_PLUGIN_INSTALL_DIR}")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_DISPLAY_NAME "Meshtastic SDR++ Plugin ${MESHTASTIC_PLUGIN_VERSION}")
    set(CPACK_NSIS_HELP_LINK "https://github.com/yourusername/meshtastic-sdrpp-plugin")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/yourusername/meshtastic-sdrpp-plugin")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Meshtastic SDR++ Plugin")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Meshtastic SDR++ Plugin ${MESHTASTIC_PLUGIN_VERSION}")
    set(CPACK_DMG_FORMAT "UDZO")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Victor <victor@example.com>")
    set(CPACK_RPM_PACKAGE_VENDOR "Victor")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
endif()

# Package file name
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${MESHTASTIC_PLUGIN_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

include(CPack)

# =============================================================================
# Testing
# =============================================================================
option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    enable_testing()

    # Add unit tests if test sources exist
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
else()
    message(STATUS "Tests disabled (set BUILD_TESTS=ON to enable)")
endif()

# =============================================================================
# Developer Targets
# =============================================================================
# Format target
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${PLUGIN_SOURCES} ${PLUGIN_HEADERS}
        COMMENT "Formatting source files with clang-format"
    )
endif()

# Static analysis
find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    add_custom_target(static-analysis
        COMMAND ${CPPCHECK} --enable=all --std=c++17 --suppress=missingIncludeSystem
                -I ${CMAKE_CURRENT_SOURCE_DIR}/include
                -I ${CMAKE_CURRENT_BINARY_DIR}/include
                -I ${SDRPlusPlus_INCLUDE_DIRS}
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        COMMENT "Running static analysis with cppcheck"
    )
endif()

# Documentation
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    set(DOXYGEN_INPUT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src")
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
    doxygen_add_docs(docs ${DOXYGEN_INPUT_DIRS} ALL)
endif()

# =============================================================================
# Summary
# =============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Meshtastic SDR++ Plugin Build Summary")
message(STATUS "========================================")
message(STATUS "Version: ${MESHTASTIC_PLUGIN_VERSION}")
message(STATUS "SDR++ Version: ${SDRPlusPlus_VERSION}")
message(STATUS "Protobuf Version: ${Protobuf_VERSION}")
message(STATUS "Crypto++ Version: ${CRYPTOPP_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Plugin Install Dir: ${SDRPlusPlus_PLUGIN_INSTALL_DIR}")
message(STATUS "========================================")