# patch_pb_header.cmake
# Called as: cmake -DFILE=path/to/xxx.pb.h -P patch_pb_header.cmake
# Prepends #undef guards for Windows macros at the very top of the file.
# This works regardless of protobuf version or include guard style.

if(NOT DEFINED FILE)
    message(FATAL_ERROR "FILE variable not set")
endif()

file(READ "${FILE}" content)

# Only patch if not already patched
if(NOT content MATCHES "undef DWORD")
    set(guard
        "// Undefine Windows macros that clash with protobuf enum names\n"
        "#if defined(_WIN32)\n"
        "#  if defined(DWORD)\n#    undef DWORD\n#  endif\n"
        "#  if defined(BYTE)\n#    undef BYTE\n#  endif\n"
        "#  if defined(WORD)\n#    undef WORD\n#  endif\n"
        "#  if defined(BOOL)\n#    undef BOOL\n#  endif\n"
        "#  if defined(ERROR)\n#    undef ERROR\n#  endif\n"
        "#  if defined(OPTIONAL)\n#    undef OPTIONAL\n#  endif\n"
        "#  if defined(NEAR)\n#    undef NEAR\n#  endif\n"
        "#  if defined(FAR)\n#    undef FAR\n#  endif\n"
        "#endif\n\n"
    )
    string(JOIN "" guard ${guard})
    # Prepend guard at the very top of the file unconditionally
    file(WRITE "${FILE}" "${guard}${content}")
    message(STATUS "Patched: ${FILE}")
endif()
