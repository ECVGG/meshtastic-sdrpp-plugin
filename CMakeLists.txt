cmake_minimum_required(VERSION 3.13)
project(meshtastic_decoder)

# Use static protobuf libraries
set(Protobuf_USE_STATIC_LIBS ON)

# Protobuf generation
find_package(Protobuf REQUIRED)

# Override to use static libraries
set(Protobuf_LIBRARIES
    "C:/vcpkg/installed/x64-windows/lib/libprotobuf.lib"
    "C:/vcpkg/installed/x64-windows/lib/libprotobuf-lite.lib"
)

set(MESHTASTIC_PROTO_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/mesh.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/telemetry.proto"
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${MESHTASTIC_PROTO_FILES})

# Sources - must be set before including sdrpp_module.cmake
file(GLOB_RECURSE SRC "src/*.cpp" "src/*.h" "include/*.h")
# Remove MeshtasticPlugin.cpp which has duplicate plugin exports
list(FILTER SRC EXCLUDE REGEX ".*MeshtasticPlugin\\.cpp$")
list(APPEND SRC ${PROTO_SRCS} ${PROTO_HDRS})

# Include SDR++ module build system - this creates the target
include(${SDRPP_MODULE_CMAKE})

# Add /utf-8 for fmt Unicode support
if (MSVC)
    target_compile_options(meshtastic_decoder PRIVATE /utf-8)
endif()

# Additional include directories
target_include_directories(meshtastic_decoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

# Add vcpkg library directory
target_link_directories(meshtastic_decoder PRIVATE "C:/vcpkg/installed/x64-windows/lib/")

# Additional link libraries
target_link_libraries(meshtastic_decoder PRIVATE
    ${Protobuf_LIBRARIES}
    "C:/vcpkg/installed/x64-windows/lib/cryptopp.lib"
    "C:/vcpkg/installed/x64-windows/lib/abseil_dll.lib"
    fmt
    spdlog
)