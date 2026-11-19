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

# Generate protobuf files with custom command to ensure package subdirectory structure
# protobuf_generate_cpp doesn't create package subdirs with explicit file lists
set(PROTO_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/meshtastic")
file(MAKE_DIRECTORY ${PROTO_OUT_DIR})

# Use protoc from Protobuf package (vcpkg provides this)
# Protobuf_PROTOC_EXECUTABLE is set by find_package(Protobuf)
if(NOT Protobuf_PROTOC_EXECUTABLE)
    message(FATAL_ERROR "Protobuf_PROTOC_EXECUTABLE not found. Protobuf package not properly configured.")
endif()

# Custom command to generate protobuf files with proper package subdirectory
add_custom_command(
    OUTPUT ${PROTO_OUT_DIR}/mesh.pb.h ${PROTO_OUT_DIR}/mesh.pb.cc
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        --proto_path=${MESHTASTIC_PROTO_IMPORT_DIR}
        --cpp_out=${PROTO_OUT_DIR}
        ${MESHTASTIC_PROTO_FILES}
    DEPENDS ${MESHTASTIC_PROTO_FILES}
    COMMENT "Generating Meshtastic protobuf files"
    VERBATIM
)

# Collect generated sources
set(PROTO_SRCS)
set(PROTO_HDRS)
foreach(proto_file ${MESHTASTIC_PROTO_FILES})
    get_filename_component(proto_name ${proto_file} NAME_WE)
    list(APPEND PROTO_SRCS ${PROTO_OUT_DIR}/${proto_name}.pb.cc)
    list(APPEND PROTO_HDRS ${PROTO_OUT_DIR}/${proto_name}.pb.h)
endforeach()

# Custom target to ensure proto generation runs before build
add_custom_target(generate_protos DEPENDS ${PROTO_OUT_DIR}/mesh.pb.h)

# Sources - must be set before including sdrpp_module.cmake
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.h" "include/*.h")
list(APPEND SRC ${PROTO_SRCS} ${PROTO_HDRS})

# Create the module target using SDR++'s function (sdrpp_add_module from find_package)
sdrpp_add_module(meshtastic_decoder ${SRC})

# Ensure proto generation runs before building the module
add_dependencies(meshtastic_decoder generate_protos)

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