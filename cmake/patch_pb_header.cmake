# patch_pb_header.cmake
# Called as: cmake -DFILE=path/to/xxx.pb.h -P patch_pb_header.cmake
#
# Inserts #undef guards for Windows macros AFTER google/protobuf/port_def.inc
# is included. port_def.inc includes <windows.h> on MSVC which defines DWORD
# et al. as macros. Those macros then conflict with protobuf enum constant names
# inside class bodies. By undeffing right after port_def.inc we ensure the
# undefs run after windows.h but before any class definitions.

if(NOT DEFINED FILE)
    message(FATAL_ERROR "FILE variable not set")
endif()

file(READ "${FILE}" content)

# Only patch if not already patched
if(NOT content MATCHES "undef DWORD.*port_def")
    set(anchor "#include \"google/protobuf/port_def.inc\"")
    if(content MATCHES "${anchor}")
        set(undef_block
            "\n// Undefine Windows macros that clash with protobuf enum names.\n"
            "// port_def.inc above includes <windows.h> on MSVC which defines these.\n"
            "#if defined(_WIN32)\n"
            "#  if defined(DWORD)\n#    undef DWORD\n#  endif\n"
            "#  if defined(BYTE)\n#    undef BYTE\n#  endif\n"
            "#  if defined(WORD)\n#    undef WORD\n#  endif\n"
            "#  if defined(BOOL)\n#    undef BOOL\n#  endif\n"
            "#  if defined(ERROR)\n#    undef ERROR\n#  endif\n"
            "#  if defined(OPTIONAL)\n#    undef OPTIONAL\n#  endif\n"
            "#  if defined(NEAR)\n#    undef NEAR\n#  endif\n"
            "#  if defined(FAR)\n#    undef FAR\n#  endif\n"
            "// undef DWORD port_def (marker for idempotency check)\n"
            "#endif\n"
        )
        string(JOIN "" undef_block ${undef_block})
        string(REPLACE
            "${anchor}"
            "${anchor}${undef_block}"
            patched
            "${content}"
        )
        file(WRITE "${FILE}" "${patched}")
        message(STATUS "Patched (port_def undef): ${FILE}")
    else()
        message(WARNING "port_def.inc anchor not found in ${FILE}, skipping patch")
    endif()
else()
    message(STATUS "Already patched: ${FILE}")
endif()
