# Meshtastic Decoder Plugin Tests

cmake_minimum_required(VERSION 3.16)

# Find GTest
find_package(GTest REQUIRED)

# Include the main CMakeLists to get access to source files and dependencies
# We'll compile the test sources directly with the plugin sources

# Test sources
set(TEST_SOURCES
    test_protobuf.cpp
    test_decryptor.cpp
    test_protocol.cpp
)

# Create test executable - link sources directly, not the MODULE library
add_executable(meshtastic_tests ${TEST_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/MeshtasticDecryptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/MeshtasticProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/LoRaDemodulator.cpp
)

# Link libraries
target_link_libraries(meshtastic_tests PRIVATE
    GTest::GTest
    GTest::Main
    ${Protobuf_LIBRARIES}
    ${CRYPTOPP_LIBRARIES}
    Threads::Threads
)

# Include directories
target_include_directories(meshtastic_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${CRYPTOPP_INCLUDE_DIRS}
)

# Add test
add_test(NAME meshtastic_tests COMMAND meshtastic_tests)

# Set properties
set_target_properties(meshtastic_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)