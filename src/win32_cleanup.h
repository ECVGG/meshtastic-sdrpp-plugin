#pragma once
// Force-include Windows headers first, then undefine macros that clash
// with protobuf-generated enum constant names (e.g. DWORD, BYTE, ERROR).
// By including <windows.h> here ourselves, subsequent includes are no-ops
// (include guard already set), so our #undefs below persist for the
// rest of the translation unit.
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef VC_EXTRA_LEAN
#    define VC_EXTRA_LEAN
#  endif
#  include <windows.h>
// Now undefine the macros that clash with protobuf enum constant names
#  undef DWORD
#  undef BYTE
#  undef WORD
#  undef BOOL
#  undef TRUE
#  undef FALSE
#  undef ERROR
#  undef OPTIONAL
#  undef NEAR
#  undef FAR
#  undef VOID
#  undef CONST
#  undef IN
#  undef OUT
#endif
