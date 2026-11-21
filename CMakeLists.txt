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
find_package(absl CONFIG REQUIRED)

# Proto files from Meshtastic protobufs submodule
# protobufs.git structure: meshtastic/*.proto at root, nanopb.proto also at root
set(MESHTASTIC_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/meshtastic")
set(MESHTASTIC_PROTO_IMPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic")

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
    "${MESHTASTIC_PROTO_DIR}/field_metadata.proto"
    "${MESHTASTIC_PROTO_DIR}/device_ui.proto"
    "${MESHTASTIC_PROTO_DIR}/module_config.proto"
    "${MESHTASTIC_PROTO_DIR}/connection_status.proto"
    "${MESHTASTIC_PROTO_DIR}/atak.proto"
    "${MESHTASTIC_PROTO_DIR}/xmodem.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/nanopb.proto"
)

# Generate protobuf files preserving directory structure under build/
# protobuf_generate with PROTOC_OUT_DIR + APPEND_PATH FALSE puts files in
# build/meshtastic/xxx.pb.cc matching the import path relative to IMPORT_DIRS
set(PROTO_SRCS)
set(PROTO_HDRS)
protobuf_generate(
    LANGUAGE cpp
    OUT_VAR PROTO_SRCS
    PROTOS ${MESHTASTIC_PROTO_FILES}
    IMPORT_DIRS ${MESHTASTIC_PROTO_IMPORT_DIR}
    PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}
    APPEND_PATH FALSE
)
# Collect the generated headers separately
foreach(_src ${PROTO_SRCS})
    string(REPLACE ".pb.cc" ".pb.h" _hdr "${_src}")
    list(APPEND PROTO_HDRS "${_hdr}")
endforeach()

# Sources - must be set before including sdrpp_module.cmake
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.h" "include/*.h")
list(APPEND SRC ${PROTO_SRCS} ${PROTO_HDRS})

# Create the module target using SDR++'s function
sdrpp_add_module(meshtastic_decoder ${SRC})

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
    ${SDRPlusPlus_INCLUDE_DIRS}
)

# Link libraries using imported targets (vcpkg provides these via toolchain)
target_link_libraries(meshtastic_decoder PRIVATE
    protobuf::libprotobuf
    cryptopp::cryptopp
    fmt::fmt
    absl::strings
    absl::memory
    absl::synchronization
    absl::time
)