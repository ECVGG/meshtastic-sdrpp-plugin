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

# Generate protobuf files using protoc directly so output lands in build/meshtastic/
# and build/ (for nanopb), matching the #include "meshtastic/xxx.pb.h" pattern.
find_program(PROTOC_EXECUTABLE protoc REQUIRED)

set(PROTO_SRCS)
set(PROTO_HDRS)

# meshtastic/*.proto -> build/meshtastic/xxx.pb.{cc,h}
set(MESHTASTIC_PROTO_NAMES
    mesh telemetry channel portnums admin config deviceonly interdevice
    localonly mqtt remote_hardware serial_hal storeforward field_metadata
    device_ui module_config connection_status atak xmodem
)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/meshtastic)
foreach(_name ${MESHTASTIC_PROTO_NAMES})
    set(_proto "${MESHTASTIC_PROTO_DIR}/${_name}.proto")
    set(_pb_cc "${CMAKE_CURRENT_BINARY_DIR}/meshtastic/${_name}.pb.cc")
    set(_pb_h  "${CMAKE_CURRENT_BINARY_DIR}/meshtastic/${_name}.pb.h")
    add_custom_command(
        OUTPUT ${_pb_cc} ${_pb_h}
        COMMAND ${PROTOC_EXECUTABLE}
            --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
            -I${MESHTASTIC_PROTO_IMPORT_DIR}
            ${_proto}
        COMMAND ${CMAKE_COMMAND}
            -DFILE=${_pb_h}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch_pb_header.cmake
        DEPENDS ${_proto}
        COMMENT "Generating ${_name}.pb.cc/.h"
        VERBATIM
    )
    list(APPEND PROTO_SRCS ${_pb_cc})
    list(APPEND PROTO_HDRS ${_pb_h})
endforeach()

# nanopb.proto is at the root of the submodule -> build/nanopb.pb.{cc,h}
set(_nanopb_proto "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/nanopb.proto")
set(_nanopb_cc "${CMAKE_CURRENT_BINARY_DIR}/nanopb.pb.cc")
set(_nanopb_h  "${CMAKE_CURRENT_BINARY_DIR}/nanopb.pb.h")
add_custom_command(
    OUTPUT ${_nanopb_cc} ${_nanopb_h}
    COMMAND ${PROTOC_EXECUTABLE}
        --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
        -I${MESHTASTIC_PROTO_IMPORT_DIR}
        ${_nanopb_proto}
    DEPENDS ${_nanopb_proto}
    COMMENT "Generating nanopb.pb.cc/.h"
    VERBATIM
)
list(APPEND PROTO_SRCS ${_nanopb_cc})
list(APPEND PROTO_HDRS ${_nanopb_h})

# Sources - must be set before including sdrpp_module.cmake
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.h" "include/*.h")
list(APPEND SRC ${PROTO_SRCS} ${PROTO_HDRS})

# Create the module target using SDR++'s function
sdrpp_add_module(meshtastic_decoder ${SRC})

# MSVC compile options
if (MSVC)
    target_compile_options(meshtastic_decoder PRIVATE
        /utf-8
        # Force-include win32_cleanup.h as the very first header in every TU.
        # It pre-includes <windows.h> (exhausting its include guard) and then
        # undefines macros like DWORD that clash with protobuf enum constants.
        /FI${CMAKE_CURRENT_SOURCE_DIR}/src/win32_cleanup.h
    )
    target_compile_definitions(meshtastic_decoder PRIVATE
        NOMINMAX
    )
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