cmake_minimum_required(VERSION 3.13)
project(meshtastic_decoder LANGUAGES CXX)

# Use static protobuf libraries
set(Protobuf_USE_STATIC_LIBS ON)

# Include SDR++ config directly (avoids vcpkg's find_package wrapper)
# SDRPP_CMAKE_DIR is passed via -DSDRPP_CMAKE_DIR=...
include("${SDRPP_CMAKE_DIR}/SDRPlusPlusConfig.cmake")
include("${SDRPP_CMAKE_DIR}/sdrpp_module.cmake")

# Find all required packages (vcpkg toolchain provides these via find_package)
# Explicit find_package calls needed even with vcpkg toolchain
find_package(Protobuf CONFIG REQUIRED)
find_package(Cryptopp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)

# Proto files from Meshtastic firmware submodule (proto/meshtastic/protobufs/meshtastic/)
set(MESHTASTIC_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/protobufs/meshtastic")
set(MESHTASTIC_PROTO_IMPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/protobufs")

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

# Generate protobuf files (flat in build dir)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${MESHTASTIC_PROTO_FILES}
    IMPORT_DIRS ${MESHTASTIC_PROTO_IMPORT_DIR}
)

# Create meshtastic subdirectory and copy generated files there
# Generated headers include each other as #include "meshtastic/xxx.pb.h"
# so they need to be in build/meshtastic/
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/mesh.pb.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/meshtastic
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/*.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/*.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/
    DEPENDS ${PROTO_HDRS} ${PROTO_SRCS}
    COMMENT "Copy generated protobuf files to meshtastic subdirectory"
    VERBATIM
)

# Custom target to ensure proto files are copied before building
add_custom_target(copy_proto_files DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/mesh.pb.h)

# Sources - must be set before including sdrpp_module.cmake
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.h" "include/*.h")
list(APPEND SRC ${PROTO_SRCS} ${PROTO_HDRS})

# Create the module target using SDR++'s function
sdrpp_add_module(meshtastic_decoder ${SRC})

# Ensure proto files are copied before building
add_dependencies(meshtastic_decoder copy_proto_files)

# Add /utf-8 for fmt Unicode support
if (MSVC)
    target_compile_options(meshtastic_decoder PRIVATE /utf-8)
endif()

# Additional include directories
# Generated proto headers are in build/meshtastic/ - they include each other as #include "meshtastic/xxx.pb.h"
# Source files include them as #include <meshtastic/mesh.pb.h> so we need build/ in include path
target_include_directories(meshtastic_decoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    # SDR++ includes (already contains core/src, src, gui, dsp, third-party/imgui from config)
    ${SDRPlusPlus_INCLUDE_DIRS}
)

# Link libraries using imported targets (vcpkg provides these via toolchain)
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