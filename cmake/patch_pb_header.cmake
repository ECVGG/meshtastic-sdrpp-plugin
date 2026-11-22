# patch_pb_header.cmake
# No-op: protobuf 5.27.x (shipped by vcpkg 2024.06.15) does not generate
# Windows-macro-conflicting identifiers like DWORD in class bodies.
# This file is kept as a hook in case patching is needed in the future.
if(NOT DEFINED FILE)
    message(FATAL_ERROR "FILE variable not set")
endif()
message(STATUS "proto header ok: ${FILE}")
