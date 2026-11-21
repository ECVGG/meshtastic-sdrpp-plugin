#pragma once
// Undefine Windows macros that clash with protobuf-generated enum names.
// Must be included before any protobuf headers on MSVC/Windows builds.
#ifdef _WIN32
#undef DWORD
#undef BYTE
#undef WORD
#undef BOOL
#undef TRUE
#undef FALSE
#undef ERROR
#undef OPTIONAL
#undef NEAR
#undef FAR
#endif
