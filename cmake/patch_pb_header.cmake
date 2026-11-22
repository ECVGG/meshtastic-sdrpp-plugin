# patch_pb_header.cmake
# Called as: cmake -DFILE=path/to/xxx.pb.h -P patch_pb_header.cmake
#
# Uses #pragma push_macro / pop_macro (MSVC + GCC/Clang support) to temporarily
# suppress Windows macros that conflict with protobuf enum constant names inside
# class bodies. This is inserted AFTER all #include directives at the top of the
# generated .pb.h, immediately before the first namespace declaration, ensuring
# it runs after all windows.h inclusions (direct or via port_def.inc).
#
# This approach is robust because push_macro/pop_macro work at the preprocessor
# level regardless of include order, and the pop at EOF restores original state.

if(NOT DEFINED FILE)
    message(FATAL_ERROR "FILE variable not set")
endif()

file(READ "${FILE}" content)

# Only patch if not already patched (check for our marker)
if(content MATCHES "PUSH_WIN32_MACROS_FOR_PROTOBUF")
    message(STATUS "Already patched: ${FILE}")
    return()
endif()

# The push block: suppress conflicting Windows macros before class definitions.
# Inserted before the first "namespace" declaration in the file.
set(push_block
    "#if defined(_WIN32) && !defined(PUSH_WIN32_MACROS_FOR_PROTOBUF)\n"
    "#define PUSH_WIN32_MACROS_FOR_PROTOBUF\n"
    "#pragma push_macro(\"DWORD\")\n"
    "#pragma push_macro(\"BYTE\")\n"
    "#pragma push_macro(\"WORD\")\n"
    "#pragma push_macro(\"BOOL\")\n"
    "#pragma push_macro(\"ERROR\")\n"
    "#pragma push_macro(\"OPTIONAL\")\n"
    "#pragma push_macro(\"NEAR\")\n"
    "#pragma push_macro(\"FAR\")\n"
    "#undef DWORD\n"
    "#undef BYTE\n"
    "#undef WORD\n"
    "#undef BOOL\n"
    "#undef ERROR\n"
    "#undef OPTIONAL\n"
    "#undef NEAR\n"
    "#undef FAR\n"
    "#endif\n\n"
)
string(JOIN "" push_block ${push_block})

# The pop block: restore macros at the very end of the file (before final #endif).
set(pop_block
    "\n#ifdef PUSH_WIN32_MACROS_FOR_PROTOBUF\n"
    "#pragma pop_macro(\"FAR\")\n"
    "#pragma pop_macro(\"NEAR\")\n"
    "#pragma pop_macro(\"OPTIONAL\")\n"
    "#pragma pop_macro(\"ERROR\")\n"
    "#pragma pop_macro(\"BOOL\")\n"
    "#pragma pop_macro(\"WORD\")\n"
    "#pragma pop_macro(\"BYTE\")\n"
    "#pragma pop_macro(\"DWORD\")\n"
    "#undef PUSH_WIN32_MACROS_FOR_PROTOBUF\n"
    "#endif\n"
)
string(JOIN "" pop_block ${pop_block})

# Find "namespace " as the insertion anchor for the push block.
# All generated .pb.h files have this before class definitions.
set(namespace_anchor "namespace ")

if(NOT content MATCHES "${namespace_anchor}")
    message(WARNING "Could not find 'namespace ' anchor in ${FILE}, skipping patch")
    return()
endif()

# Insert push_block before the first "namespace " occurrence
string(FIND "${content}" "${namespace_anchor}" first_ns_pos)
if(first_ns_pos EQUAL -1)
    message(WARNING "string FIND: 'namespace ' not found in ${FILE}, skipping patch")
    return()
endif()

string(SUBSTRING "${content}" 0 ${first_ns_pos} before_ns)
string(SUBSTRING "${content}" ${first_ns_pos} -1 from_ns)

# Insert pop_block before the final #endif (the include guard closer at EOF)
# Find last occurrence of "#endif" in from_ns (which is content from namespace onward)
string(FIND "${from_ns}" "#endif" last_endif_pos REVERSE)
if(last_endif_pos EQUAL -1)
    # No #endif found; just append pop at end
    set(patched "${before_ns}${push_block}${from_ns}${pop_block}")
else()
    string(SUBSTRING "${from_ns}" 0 ${last_endif_pos} before_endif)
    string(SUBSTRING "${from_ns}" ${last_endif_pos} -1 at_endif)
    set(patched "${before_ns}${push_block}${before_endif}${pop_block}${at_endif}")
endif()

file(WRITE "${FILE}" "${patched}")
message(STATUS "Patched (push/pop macros): ${FILE}")
