cmake_minimum_required(VERSION 3.13)
project(meshtastic_decoder LANGUAGES CXX)

# Use static protobuf libraries
set(Protobuf_USE_STATIC_LIBS ON)

# Find required packages via vcpkg
find_package(Protobuf REQUIRED)
find_package(Cryptopp REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(absl REQUIRED)

# Find SDR++ package (provides SDRPlusPlus_INCLUDE_DIRS, SDRPlusPlus_LIBRARIES, and sdrpp_add_module)
find_package(SDRPlusPlus REQUIRED)

# Proto files from Meshtastic firmware submodule (proto/meshtastic/protobufs/meshtastic/)
set(MESHTASTIC_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/protobufs/meshtastic")
set(MESHTASTIC_PROTO_IMPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto/meshtastic/protobufs")
# Generate proto files in a meshtastic subdirectory so generated includes like "meshtastic/xxx.pb.h" work
set(MESHTASTIC_PROTO_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/meshtastic")

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
# Generate output in meshtastic subdirectory so #include "meshtastic/xxx.pb.h" works
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${MESHTASTIC_PROTO_FILES}
    IMPORT_DIRS ${MESHTASTIC_PROTO_IMPORT_DIR}
    OUTPUT_DIR ${MESHTASTIC_PROTO_OUTPUT_DIR}
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
# Include build/meshtastic for generated proto headers (they use #include "meshtastic/xxx.pb.h")
target_include_directories(meshtastic_decoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${MESHTASTIC_PROTO_OUTPUT_DIR}
    ${Protobuf_INCLUDE_DIRS}
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