# Meshtastic Decoder Plugin Tests

cmake_minimum_required(VERSION 3.16)

# Find GTest
find_package(GTest REQUIRED)

# Test sources
set(TEST_SOURCES
    test_protobuf.cpp
    test_decryptor.cpp
    test_protocol.cpp
)

# Create test executable
add_executable(meshtastic_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(meshtastic_tests PRIVATE
    GTest::GTest
    GTest::Main
    MeshtasticPlugin
    ${Protobuf_LIBRARIES}
    ${CRYPTOPP_LIBRARIES}
)

# Include directories
target_include_directories(meshtastic_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${CRYPTOPP_INCLUDE_DIRS}
    ${SDRPlusPlus_INCLUDE_DIRS}
)

# Add test
add_test(NAME meshtastic_tests COMMAND meshtastic_tests)

# Set properties
set_target_properties(meshtastic_tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)