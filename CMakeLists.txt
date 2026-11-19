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

# Generate protobuf files into build/meshtastic/ subdirectory
# This ensures generated headers can include each other as #include "meshtastic/xxx.pb.h"
# Source files will include them as #include <meshtastic/mesh.pb.h>
set(PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/meshtastic")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

# Use custom command to run protoc directly (more reliable than protobuf_generate_cpp)
find_program(PROTOC_EXECUTABLE protoc)
if(NOT PROTOC_EXECUTABLE)
    find_program(PROTOC_EXECUTABLE NAMES protoc.exe
        PATHS "${Protobuf_INCLUDE_DIRS}/../tools/protobuf"
        NO_DEFAULT_PATH
    )
endif()

# Generate list of output files for dependency tracking
set(PROTO_GEN_HDRS "")
set(PROTO_GEN_SRCS "")
foreach(proto_file ${MESHTASTIC_PROTO_FILES})
    get_filename_component(proto_name ${proto_file} NAME_WE)
    list(APPEND PROTO_GEN_HDRS "${PROTO_GEN_DIR}/${proto_name}.pb.h")
    list(APPEND PROTO_GEN_SRCS "${PROTO_GEN_DIR}/${proto_name}.pb.cc")
endforeach()

add_custom_command(
    OUTPUT ${PROTO_GEN_HDRS} ${PROTO_GEN_SRCS}
    COMMAND ${PROTOC_EXECUTABLE}
        --proto_path=${MESHTASTIC_PROTO_IMPORT_DIR}
        --cpp_out=${PROTO_GEN_DIR}
        ${MESHTASTIC_PROTO_FILES}
    COMMENT "Generating Meshtastic protobuf headers and sources"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
)

set(PROTO_SRCS ${PROTO_GEN_SRCS})
set(PROTO_HDRS ${PROTO_GEN_HDRS})

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
# Generated proto headers are in build/meshtastic/ (OUTPUT_DIR)
# They include each other as #include "meshtastic/xxx.pb.h" so we need build/ in path
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