#pragma once
// Protobuf 7.x generates identifiers like "DWORD" as enum aliases.
// On Windows, <windows.h> (pulled in by many headers, including SDR++'s
// own core headers) defines these as macros, which breaks the generated
// protobuf code. Include this header immediately before any
// "meshtastic/*.pb.h" include to clear the conflicting macros, regardless
// of what already included <windows.h> earlier in the translation unit.
#ifdef _WIN32
#ifdef DWORD
#undef DWORD
#endif
#ifdef BYTE
#undef BYTE
#endif
#ifdef WORD
#undef WORD
#endif
#ifdef BOOL
#undef BOOL
#endif
#ifdef ERROR
#undef ERROR
#endif
#ifdef OPTIONAL
#undef OPTIONAL
#endif
#ifdef NEAR
#undef NEAR
#endif
#ifdef FAR
#undef FAR
#endif
#endif
