#pragma once
// MSVC's <windows.h> defines the classic min()/max() macros; mingw-w64's does
// not. The vendored game code relies on them (this build uses no std::min /
// std::max). Include this AFTER all other headers in a .cpp so the macros never
// shadow std:: internals inside system headers.
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
