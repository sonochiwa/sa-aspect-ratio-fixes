#pragma once

#include <windows.h>

namespace logging {

// Enables the log file next to the plugin and truncates it. Without this call
// every Write is a no-op.
void Enable(HMODULE module);
void Disable();

void Write(const char* format, ...);

}  // namespace logging
