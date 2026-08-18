#include "log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace logging {
namespace {

char g_path[MAX_PATH] = {};
bool g_enabled = false;

}  // namespace

void Enable(HMODULE module) {
    const DWORD length = GetModuleFileNameA(module, g_path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return;

    char* dot = std::strrchr(g_path, '.');
    if (!dot)
        return;

    const size_t used = static_cast<size_t>(dot - g_path);
    if (used + 5 > MAX_PATH)
        return;

    std::memcpy(dot, ".log", 5);

    const HANDLE file = CreateFileA(g_path, GENERIC_WRITE, FILE_SHARE_READ,
                                    nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    CloseHandle(file);
    g_enabled = true;
}

void Disable() {
    g_enabled = false;
}

void Write(const char* format, ...) {
    if (!g_enabled)
        return;

    char line[512] = {};
    va_list arguments;
    va_start(arguments, format);
    const int length = std::vsnprintf(line, sizeof(line) - 2, format, arguments);
    va_end(arguments);
    if (length <= 0)
        return;

    std::memcpy(line + length, "\r\n", 2);

    const HANDLE file = CreateFileA(g_path, FILE_APPEND_DATA, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;

    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(length) + 2, &written, nullptr);
    CloseHandle(file);
}

}  // namespace logging
