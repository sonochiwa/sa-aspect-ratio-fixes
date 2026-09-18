#include "hooking.h"

#include <windows.h>

#include <cstring>

namespace hooking {
namespace {

// Another modification can repoint the same instructions after this plugin
// has already done so, which would undo a group without leaving any trace in
// the log. Every group that applies is therefore remembered and re-read by
// the watcher, so a group that stops pointing at us is reported once.
//
// A site's 32 bit operand is either an absolute address or, for a call, a
// displacement from the next instruction; both resolve to the address the
// site points at, which is what is compared.
struct PatchedGroup {
    const char* name;
    const uintptr_t* sites;
    size_t count;
    size_t operandOffset;
    bool relative;
    uintptr_t target;
    bool reported;
};

constexpr size_t kMaxGuards = 32;
constexpr size_t kCallLength = 5;

PatchedGroup g_guards[kMaxGuards] = {};
size_t g_guardCount = 0;

} // namespace

bool BytesMatch(uintptr_t address, const uint8_t* expected, size_t size) {
    return patch::IsReadable(address, size) &&
           std::memcmp(reinterpret_cast<const void*>(address), expected, size) == 0;
}

bool WriteRelativeBranch(uintptr_t site, uint8_t opcode, const void* target) {
    const intptr_t displacement = reinterpret_cast<intptr_t>(target) - static_cast<intptr_t>(site + 5);
    if (displacement < INT32_MIN || displacement > INT32_MAX)
        return false;

    uint8_t branch[5] = {opcode, 0, 0, 0, 0};
    const auto relative = static_cast<int32_t>(displacement);
    std::memcpy(branch + 1, &relative, sizeof(relative));
    return patch::WriteMemory(site, branch, sizeof(branch));
}

bool RelativeCallTargets(uintptr_t site, uintptr_t expected) {
    if (!patch::IsReadable(site, 5))
        return false;

    const auto* code = reinterpret_cast<const uint8_t*>(site);
    if (code[0] != 0xE8)
        return false;

    int32_t relative = 0;
    std::memcpy(&relative, code + 1, sizeof(relative));
    return site + 5 + relative == expected;
}

bool InstallTrampolineHook(uintptr_t address, const uint8_t* expected, size_t stolen, uintptr_t resumeAt,
                           const void* hook, void** resume) {
    auto* trampoline = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, stolen + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!trampoline)
        return false;

    std::memcpy(trampoline, expected, stolen);
    trampoline[stolen] = 0xE9;
    const auto backwards = static_cast<int32_t>(static_cast<intptr_t>(resumeAt) -
                                                reinterpret_cast<intptr_t>(trampoline + stolen + 5));
    std::memcpy(trampoline + stolen + 1, &backwards, sizeof(backwards));

    const intptr_t displacement = reinterpret_cast<intptr_t>(hook) - static_cast<intptr_t>(address + 5);
    if (displacement < INT32_MIN || displacement > INT32_MAX) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }

    // Publish the trampoline before the branch, or a frame drawn in between
    // calls through a null pointer.
    *resume = trampoline;

    uint8_t branch[16];
    std::memset(branch, 0x90, stolen);
    branch[0] = 0xE9;
    const auto relative = static_cast<int32_t>(displacement);
    std::memcpy(branch + 1, &relative, sizeof(relative));

    if (patch::WriteMemory(address, branch, stolen))
        return true;

    *resume = nullptr;
    VirtualFree(trampoline, 0, MEM_RELEASE);
    return false;
}

void RememberGuard(const char* name, const uintptr_t* sites, size_t count, size_t operandOffset, bool relative,
                   const void* target) {
    if (g_guardCount >= kMaxGuards)
        return;

    PatchedGroup& guard = g_guards[g_guardCount++];
    guard.name = name;
    guard.sites = sites;
    guard.count = count;
    guard.operandOffset = operandOffset;
    guard.relative = relative;
    guard.target = reinterpret_cast<uintptr_t>(target);
    guard.reported = false;
}

void CheckGuards() {
    for (size_t g = 0; g < g_guardCount; ++g) {
        PatchedGroup& guard = g_guards[g];
        if (guard.reported)
            continue;

        for (size_t i = 0; i < guard.count; ++i) {
            int32_t operand = 0;
            if (!patch::ReadInt32(guard.sites[i] + guard.operandOffset, operand))
                continue;

            const uintptr_t current = guard.relative
                                          ? guard.sites[i] + kCallLength + static_cast<uintptr_t>(operand)
                                          : static_cast<uintptr_t>(static_cast<uint32_t>(operand));
            if (current == guard.target)
                continue;

            logging::Write("%-22s site %08X now points at %08X instead of %08X, "
                           "another modification has overwritten it",
                           guard.name, static_cast<unsigned>(guard.sites[i]), static_cast<unsigned>(current),
                           static_cast<unsigned>(guard.target));
            guard.reported = true;
            break;
        }
    }
}

} // namespace hooking
