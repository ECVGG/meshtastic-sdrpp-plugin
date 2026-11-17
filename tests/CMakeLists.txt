# Meshtastic Decoder Plugin Tests
# GTest is found in the parent CMakeLists.txt

# Test sources
set(TEST_SOURCES
    test_protobuf.cpp
    test_decryptor.cpp
    test_protocol.cpp
)

# Include generated protobuf sources from parent
# These are generated in the build root directory
set(PROTO_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/../mesh.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/../telemetry.pb.cc
)

# Create test executable - link sources directly, not the MODULE library
add_executable(meshtastic_tests ${TEST_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/MeshtasticDecryptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/MeshtasticProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/LoRaDemodulator.cpp
    ${PROTO_SRCS}
)

# Link libraries - use modern GTest targets
target_link_libraries(meshtastic_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    ${Protobuf_LIBRARIES}
    ${CRYPTOPP_LIBRARIES}
    Threads::Threads
)

# Include directories
target_include_directories(meshtastic_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}/..
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