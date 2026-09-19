#pragma once

#include <cstddef>

namespace cheat_command {

enum class Command : size_t {
    Reload,
    Probe,
    Count,
};

// Hooks the keyboard of the thread that owns the game window. Returns false
// while that window does not exist yet; call again later.
bool Install();

// The word to watch for, typed in game like a single-player cheat. Letters
// and digits only, case-insensitive; an empty word disables the command.
void SetWord(Command command, const char* word);

// True once for every complete word typed since the previous call.
bool Consume(Command command);

} // namespace cheat_command
