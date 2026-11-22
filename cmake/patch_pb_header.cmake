# patch_pb_header.cmake
# Called as: cmake -DFILE=path/to/xxx.pb.h -P patch_pb_header.cmake
# Inserts #undef guards for Windows macros after the first #pragma once line.

if(NOT DEFINED FILE)
    message(FATAL_ERROR "FILE variable not set")
endif()

file(READ "${FILE}" content)

# Only patch if not already patched
if(NOT content MATCHES "undef DWORD")
    set(guard
        "\n// Undefine Windows macros that clash with protobuf enum names\n"
        "#if defined(_WIN32)\n"
        "#  if defined(DWORD)\n#    undef DWORD\n#  endif\n"
        "#  if defined(BYTE)\n#    undef BYTE\n#  endif\n"
        "#  if defined(WORD)\n#    undef WORD\n#  endif\n"
        "#  if defined(BOOL)\n#    undef BOOL\n#  endif\n"
        "#  if defined(ERROR)\n#    undef ERROR\n#  endif\n"
        "#  if defined(OPTIONAL)\n#    undef OPTIONAL\n#  endif\n"
        "#  if defined(NEAR)\n#    undef NEAR\n#  endif\n"
        "#  if defined(FAR)\n#    undef FAR\n#  endif\n"
        "#endif\n"
    )
    string(JOIN "" guard ${guard})
    # Handle both Unix (LF) and Windows (CRLF) line endings
    string(REPLACE "#pragma once\r\n" "#pragma once\r\n${guard}" patched "${content}")
    if(patched STREQUAL content)
        # No CRLF match, try LF only
        string(REPLACE "#pragma once\n" "#pragma once\n${guard}" patched "${content}")
    endif()
    if(NOT patched STREQUAL content)
        file(WRITE "${FILE}" "${patched}")
        message(STATUS "Patched: ${FILE}")
    else()
        message(WARNING "Could not find '#pragma once' in ${FILE} - not patched")
    endif()
endif()
