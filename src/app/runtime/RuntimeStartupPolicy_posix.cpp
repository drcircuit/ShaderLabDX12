// ---------------------------------------------------------------------------
// RuntimeStartupPolicy_posix.cpp
//
// POSIX / SDL2 implementation of RuntimeStartupPolicy.
// Replaces the Win32-specific RuntimeStartupPolicy.cpp on Linux and macOS.
// ---------------------------------------------------------------------------

#include "ShaderLab/Runtime/RuntimeStartupPolicy.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#ifdef __linux__
    // Use /proc/self/exe to find the executable path on Linux.
    #include <unistd.h>
    #include <limits.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <limits.h>
#endif

#ifndef SHADERLAB_TINY_PLAYER
#define SHADERLAB_TINY_PLAYER 0
#endif

#if !SHADERLAB_TINY_PLAYER
#include <iostream>
#endif

namespace ShaderLab::RuntimeStartupPolicy {

// ---------------------------------------------------------------------------
// Helper: retrieve the running executable's path (without filename).
// ---------------------------------------------------------------------------
namespace {

std::string GetExecutablePath() {
    char buf[PATH_MAX] = {};
#ifdef __linux__
    const ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) buf[len] = '\0';
#elif defined(__APPLE__)
    uint32_t size = sizeof(buf);
    _NSGetExecutablePath(buf, &size);
#endif
    std::string path(buf);
    const size_t slash = path.find_last_of('/');
    return (slash != std::string::npos) ? path.substr(0, slash + 1) : "./";
}

} // namespace

// ---------------------------------------------------------------------------
// HandleCloser – on POSIX there is no Win32 HANDLE; the unique_ptr just
// stores nullptr and the deleter is a no-op.
// ---------------------------------------------------------------------------
void HandleCloser::operator()(void* /*handle*/) const {
    // No Win32 CloseHandle equivalent needed on POSIX for our use-cases.
}

// ---------------------------------------------------------------------------
// Single-instance enforcement via a lock file.
// ---------------------------------------------------------------------------
UniqueHandle CreateSingleInstanceMutex(bool& alreadyExists) {
    alreadyExists = false;
    // On POSIX we rely on the lock-file approach using flock() or simply
    // allow multiple instances.  For simplicity, always allow one instance.
    // A production implementation would use flock() on a pid-file.
    return UniqueHandle(reinterpret_cast<void*>(1)); // non-null sentinel = success
}

// ---------------------------------------------------------------------------
void AppendPlayerErrorLogLine(const std::string& message) {
    const std::string logPath = GetExecutablePath() + "runtime_error.log";
    std::ofstream logFile(logPath, std::ios::out | std::ios::app);
    if (logFile.is_open()) {
        logFile << message << "\n";
    }
}

#if !SHADERLAB_TINY_PLAYER
void EmitRuntimeError(const char* code, const char* shortText) {
    if (!code || !*code) return;
    std::string line(code);
    if (shortText && *shortText) { line += " "; line += shortText; }
    std::fputs(line.c_str(), stderr);
    std::fputc('\n', stderr);
    AppendPlayerErrorLogLine(line);
}
#endif

// ---------------------------------------------------------------------------
// Cursor management – delegate to SDL2.
// ---------------------------------------------------------------------------
void HideRuntimeCursor(bool& runtimeCursorHidden) {
    if (runtimeCursorHidden) return;
#ifdef SHADERLAB_WINDOW_SDL2
    // SDL_ShowCursor(SDL_DISABLE) is called from the SDL2 player app so that
    // SDL.h does not need to be included here.
#endif
    runtimeCursorHidden = true;
}

void RestoreRuntimeCursor(bool& runtimeCursorHidden) {
    if (!runtimeCursorHidden) return;
#ifdef SHADERLAB_WINDOW_SDL2
    // SDL_ShowCursor(SDL_ENABLE) is called from the SDL2 player app.
#endif
    runtimeCursorHidden = false;
}

// ---------------------------------------------------------------------------
void EnableConsoleLogging() {
#if !SHADERLAB_TINY_PLAYER
    // On POSIX, stdout/stderr are already attached to a terminal or redirected.
    std::ios::sync_with_stdio(true);
    std::cout.clear();
    std::cerr.clear();
#endif
}

} // namespace ShaderLab::RuntimeStartupPolicy
