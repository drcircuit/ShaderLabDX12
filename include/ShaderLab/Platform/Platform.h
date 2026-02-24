#pragma once

// ---------------------------------------------------------------------------
// Platform detection
// ---------------------------------------------------------------------------
#if defined(_WIN32) || defined(_WIN64)
    #define SHADERLAB_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define SHADERLAB_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define SHADERLAB_PLATFORM_LINUX 1
#else
    #define SHADERLAB_PLATFORM_UNKNOWN 1
#endif

// ---------------------------------------------------------------------------
// Graphics backend selection
// If CMake hasn't set SHADERLAB_GFX_D3D12 / SHADERLAB_GFX_VULKAN, infer from
// the current platform so that headers compile even without the CMake build.
// ---------------------------------------------------------------------------
#if !defined(SHADERLAB_GFX_D3D12) && !defined(SHADERLAB_GFX_VULKAN)
    #if defined(SHADERLAB_PLATFORM_WINDOWS)
        #define SHADERLAB_GFX_D3D12 1
    #else
        #define SHADERLAB_GFX_VULKAN 1
    #endif
#endif

// ---------------------------------------------------------------------------
// Window backend selection
// ---------------------------------------------------------------------------
#if !defined(SHADERLAB_WINDOW_WIN32) && !defined(SHADERLAB_WINDOW_SDL2)
    #if defined(SHADERLAB_PLATFORM_WINDOWS)
        #define SHADERLAB_WINDOW_WIN32 1
    #else
        #define SHADERLAB_WINDOW_SDL2 1
    #endif
#endif

#include <cstdint>

namespace ShaderLab {

// ---------------------------------------------------------------------------
// NativeWindowHandle
//
// Opaque platform window handle.  NativeWindowHandleTag is an intentionally
// incomplete (forward-declared) struct, which makes NativeWindowHandle a
// distinct pointer type that cannot be accidentally converted to/from void* or
// other unrelated pointer types at compile time.  Implementation files cast to
// the concrete platform type at the boundary:
//   Windows  – reinterpret_cast<HWND>(handle)
//   SDL2     – reinterpret_cast<SDL_Window*>(handle)
// ---------------------------------------------------------------------------
struct NativeWindowHandleTag;
using NativeWindowHandle = NativeWindowHandleTag*;

// ---------------------------------------------------------------------------
// NativeAppHandle
//
// Opaque platform application/instance handle.  Same incomplete-type trick.
//   Windows  – reinterpret_cast<HINSTANCE>(handle)
//   POSIX    – not used (pass nullptr)
// ---------------------------------------------------------------------------
struct NativeAppHandleTag;
using NativeAppHandle = NativeAppHandleTag*;

// ---------------------------------------------------------------------------
// WindowRect
//
// Platform-neutral rectangle used to save/restore windowed bounds.
// ---------------------------------------------------------------------------
struct WindowRect {
    int left   = 0;
    int top    = 0;
    int right  = 0;
    int bottom = 0;

    int Width()  const { return right - left; }
    int Height() const { return bottom - top; }
};

} // namespace ShaderLab
