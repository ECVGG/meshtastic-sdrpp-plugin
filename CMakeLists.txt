cmake_minimum_required(VERSION 3.13)
project(meshtastic_decoder LANGUAGES CXX)

# Use static protobuf libraries
set(Protobuf_USE_STATIC_LIBS ON)

# Find required packages via vcpkg (use CONFIG mode for packages that provide config files)
find_package(Protobuf CONFIG REQUIRED)
find_package(Cryptopp REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(absl REQUIRED)

# Include SDR++ config directly (avoids vcpkg's find_package wrapper)
# SDRPP_CMAKE_DIR is passed via -DSDRPP_CMAKE_DIR=...
include("${SDRPP_CMAKE_DIR}/SDRPlusPlusConfig.cmake")
include("${SDRPP_CMAKE_DIR}/sdrpp_module.cmake")

# Proto files from Meshtastic firmware submodule (proto/meshtastic/protobufs/meshtastic/)
set(MESHTASTIC_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/protobufs/meshtastic")
set(MESHTASTIC_PROTO_IMPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/protobufs")
# Don't set OUTPUT_DIR - let protobuf_generate_cpp use default behavior
# which creates package subdirectories (build/meshtastic/) matching .proto imports

set(MESHTASTIC_PROTO_FILES
    "${MESHTASTIC_PROTO_DIR}/mesh.proto"
    "${MESHTASTIC_PROTO_DIR}/telemetry.proto"
    "${MESHTASTIC_PROTO_DIR}/channel.proto"
    "${MESHTASTIC_PROTO_DIR}/portnums.proto"
    "${MESHTASTIC_PROTO_DIR}/admin.proto"
    "${MESHTASTIC_PROTO_DIR}/config.proto"
    "${MESHTASTIC_PROTO_DIR}/deviceonly.proto"
    "${MESHTASTIC_PROTO_DIR}/interdevice.proto"
    "${MESHTASTIC_PROTO_DIR}/localonly.proto"
    "${MESHTASTIC_PROTO_DIR}/mqtt.proto"
    "${MESHTASTIC_PROTO_DIR}/remote_hardware.proto"
    "${MESHTASTIC_PROTO_DIR}/serial_hal.proto"
    "${MESHTASTIC_PROTO_DIR}/storeforward.proto"
)

# Add import directory for protoc to find meshtastic/xxx.proto imports
# Default output goes to CMAKE_CURRENT_BINARY_DIR/meshtastic/ (package subdirectory)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${MESHTASTIC_PROTO_FILES}
    IMPORT_DIRS ${MESHTASTIC_PROTO_IMPORT_DIR}
)

# Sources - must be set before including sdrpp_module.cmake
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.h" "include/*.h")
list(APPEND SRC ${PROTO_SRCS} ${PROTO_HDRS})

# Create the module target using SDR++'s function (sdrpp_add_module from find_package)
sdrpp_add_module(meshtastic_decoder ${SRC})

# Add /utf-8 for fmt Unicode support
if (MSVC)
    target_compile_options(meshtastic_decoder PRIVATE /utf-8)
endif()

# Additional include directories
# Include build/ for generated proto headers (they use #include "meshtastic/xxx.pb.h")
# Generated files are in build/meshtastic/ due to package name in .proto files
target_include_directories(meshtastic_decoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    # SDR++ includes (already contains core/src, src, gui, dsp, third-party/imgui from config)
    ${SDRPlusPlus_INCLUDE_DIRS}
    # Explicit imgui path - frequency_select.h includes "imgui.h" directly
    ${SDRPlusPlus_INCLUDE_DIRS}/third-party/imgui
    ${SDRPlusPlus_INCLUDE_DIRS}/core/third-party/imgui
)

# Link libraries using imported targets (vcpkg provides these)
target_link_libraries(meshtastic_decoder PRIVATE
    protobuf::libprotobuf
    cryptopp::cryptopp
    fmt::fmt
    spdlog::spdlog
    absl::strings
    absl::memory
    absl::synchronization
    absl::time
)