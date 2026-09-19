#include "cheat_command.h"

#include <windows.h>

#include <cstring>

namespace cheat_command {
namespace {

constexpr size_t kWordCapacity = 32;
constexpr size_t kCommandCount = static_cast<size_t>(Command::Count);

struct Word {
    char text[kWordCapacity] = {};
    size_t length = 0;
    volatile LONG pending = 0;
};

// The same idea as CCheat::DoCheats: the last keys typed are kept in order
// and a word matches as soon as it is the tail of that history. The history
// is filled by a WH_KEYBOARD hook on the game window's thread, so it sees
// every key in the order the game does, whether or not the SA-MP chat box
// is open.
SRWLOCK g_lock = SRWLOCK_INIT;
Word g_words[kCommandCount];
char g_history[kWordCapacity] = {};
size_t g_historyLength = 0;
HHOOK g_hook = nullptr;

char KeyToChar(WPARAM virtualKey) {
    if (virtualKey >= 'A' && virtualKey <= 'Z')
        return static_cast<char>(virtualKey);
    if (virtualKey >= '0' && virtualKey <= '9')
        return static_cast<char>(virtualKey);
    if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9)
        return static_cast<char>('0' + (virtualKey - VK_NUMPAD0));
    return 0;
}

bool HistoryEndsWith(const Word& word) {
    return word.length != 0 && g_historyLength >= word.length &&
           std::memcmp(g_history + g_historyLength - word.length, word.text, word.length) == 0;
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    // Bit 31 is the transition state (set on release), bit 30 the previous
    // state (set on auto-repeat). Only fresh presses count.
    if (code == HC_ACTION && !(lParam & 0xC0000000)) {
        const char typed = KeyToChar(wParam);
        if (typed) {
            AcquireSRWLockExclusive(&g_lock);
            if (g_historyLength == kWordCapacity - 1) {
                std::memmove(g_history, g_history + 1, kWordCapacity - 2);
                --g_historyLength;
            }
            g_history[g_historyLength++] = typed;
            for (Word& word : g_words) {
                if (HistoryEndsWith(word)) {
                    g_historyLength = 0;
                    InterlockedExchange(&word.pending, 1);
                }
            }
            ReleaseSRWLockExclusive(&g_lock);
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

BOOL CALLBACK FindGameWindow(HWND window, LPARAM result) {
    DWORD processId = 0;
    const DWORD threadId = GetWindowThreadProcessId(window, &processId);
    if (processId != GetCurrentProcessId() || !IsWindowVisible(window) ||
        GetWindow(window, GW_OWNER) != nullptr)
        return TRUE;
    *reinterpret_cast<DWORD*>(result) = threadId;
    return FALSE;
}

} // namespace

bool Install() {
    if (g_hook)
        return true;

    DWORD threadId = 0;
    EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&threadId));
    if (threadId == 0)
        return false;

    // A thread hook inside the calling process takes no module handle.
    g_hook = SetWindowsHookExW(WH_KEYBOARD, KeyboardProc, nullptr, threadId);
    return g_hook != nullptr;
}

void SetWord(Command command, const char* word) {
    char normalized[kWordCapacity] = {};
    size_t length = 0;
    for (const char* c = word ? word : ""; *c && length < kWordCapacity - 1; ++c) {
        if (*c >= 'a' && *c <= 'z')
            normalized[length++] = static_cast<char>(*c - 'a' + 'A');
        else if ((*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9'))
            normalized[length++] = *c;
    }

    Word& slot = g_words[static_cast<size_t>(command)];
    AcquireSRWLockExclusive(&g_lock);
    std::memcpy(slot.text, normalized, kWordCapacity);
    slot.length = length;
    g_historyLength = 0;
    ReleaseSRWLockExclusive(&g_lock);
}

bool Consume(Command command) {
    return InterlockedExchange(&g_words[static_cast<size_t>(command)].pending, 0) != 0;
}

} // namespace cheat_command
