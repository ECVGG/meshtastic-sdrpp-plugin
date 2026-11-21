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

# Generate protobuf files (flat in build dir)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${MESHTASTIC_PROTO_FILES}
    IMPORT_DIRS ${MESHTASTIC_PROTO_IMPORT_DIR}
)

# Create meshtastic subdirectory and copy generated files there
# Generated headers include each other as #include "meshtastic/xxx.pb.h"
# so they need to be in build/meshtastic/
# Use individual copy commands for each known proto file (wildcards don't work in copy_if_different)
set(MESHTASTIC_PROTO_BASENAMES
    mesh
    telemetry
    channel
    portnums
    admin
    config
    deviceonly
    interdevice
    localonly
    mqtt
    remote_hardware
    serial_hal
    storeforward
    field_metadata
    device_ui
    module_config
    connection_status
    atak
    xmodem
    nanopb
)

# Build list of output files for the custom target
set(MESHTASTIC_GENERATED_HDRS)
set(MESHTASTIC_GENERATED_SRCS)
foreach(basename ${MESHTASTIC_PROTO_BASENAMES})
    list(APPEND MESHTASTIC_GENERATED_HDRS ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/${basename}.pb.h)
    list(APPEND MESHTASTIC_GENERATED_SRCS ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/${basename}.pb.cc)
endforeach()

# Custom command to copy each generated file to meshtastic subdirectory
add_custom_command(
    OUTPUT ${MESHTASTIC_GENERATED_HDRS} ${MESHTASTIC_GENERATED_SRCS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/meshtastic
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/mesh.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/mesh.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/mesh.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/mesh.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/telemetry.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/telemetry.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/telemetry.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/telemetry.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/channel.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/channel.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/channel.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/channel.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/portnums.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/portnums.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/portnums.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/portnums.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/admin.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/admin.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/admin.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/admin.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/config.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/config.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/config.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/config.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/deviceonly.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/deviceonly.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/deviceonly.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/deviceonly.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/interdevice.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/interdevice.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/interdevice.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/interdevice.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/localonly.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/localonly.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/localonly.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/localonly.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/mqtt.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/mqtt.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/mqtt.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/mqtt.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/remote_hardware.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/remote_hardware.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/remote_hardware.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/remote_hardware.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/serial_hal.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/serial_hal.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/serial_hal.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/serial_hal.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/storeforward.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/storeforward.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/storeforward.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/storeforward.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/field_metadata.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/field_metadata.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/field_metadata.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/field_metadata.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/device_ui.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/device_ui.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/device_ui.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/device_ui.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/module_config.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/module_config.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/module_config.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/module_config.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/connection_status.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/connection_status.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/connection_status.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/connection_status.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/atak.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/atak.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/atak.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/atak.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/xmodem.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/xmodem.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/xmodem.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/xmodem.pb.cc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/nanopb.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/nanopb.pb.h
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/nanopb.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/meshtastic/nanopb.pb.cc
    DEPENDS ${PROTO_HDRS} ${PROTO_SRCS}
    COMMENT "Copy generated protobuf files to meshtastic subdirectory"
    VERBATIM
)

# Custom target to ensure proto files are copied before building
add_custom_target(copy_proto_files DEPENDS ${MESHTASTIC_GENERATED_HDRS} ${MESHTASTIC_GENERATED_SRCS})

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
# SDRPlusPlus_INCLUDE_DIRS already contains imgui paths from the generated config
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