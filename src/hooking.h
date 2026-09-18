#pragma once

#include "log.h"
#include "patch.h"

#include <cstddef>
#include <cstdint>

// Everything a module needs to redirect the game at it, on top of the raw
// operand repointing in patch.h, plus the guards that notice when another
// modification has overwritten a redirected site.
namespace hooking {

bool BytesMatch(uintptr_t address, const uint8_t* expected, size_t size);

// Writes a five-byte relative branch (E8 call or E9 jump) at `site`.
bool WriteRelativeBranch(uintptr_t site, uint8_t opcode, const void* target);

// True when `site` holds an E8 call whose target is `expected`.
bool RelativeCallTargets(uintptr_t site, uintptr_t expected);

// Installs a trampoline for `stolen` bytes at `address` and points `resume`
// at it. The caller has already established that those bytes are whole
// instructions and that none of them is position dependent.
bool InstallTrampolineHook(uintptr_t address, const uint8_t* expected, size_t stolen, uintptr_t resumeAt,
                           const void* hook, void** resume);

// Where the 32 bit operand starts inside the instructions the plugin
// rewrites: two bytes into an x87 instruction with an absolute address, one
// byte into `mov eax, [disp32]` and into a relative call.
constexpr size_t kOperandOffset = 2;
constexpr size_t kMovEaxOperandOffset = 1;
constexpr size_t kCallOperandOffset = 1;

// Remembers a group of sites that now point at `target`, so the watcher can
// report once when another modification points them elsewhere.
void RememberGuard(const char* name, const uintptr_t* sites, size_t count, size_t operandOffset, bool relative,
                   const void* target);
void CheckGuards();

// Repoints every x87 operand in `sites` from `expected` at `target`, logs the
// result under `name` and guards the group.
template <size_t N>
bool ApplyGroup(const char* name, const uintptr_t (&sites)[N], uintptr_t expected, const void* target) {
    const bool applied = patch::RepointOperands(sites, expected, target);
    logging::Write("%-22s %s (%u sites)", name, applied ? "patched" : "SKIPPED, unexpected bytes",
                   static_cast<unsigned>(N));
    if (applied)
        RememberGuard(name, sites, N, kOperandOffset, false, target);
    return applied;
}

// Retargets every E8 call in `sites` that still calls `expected` at
// `wrapper`. Nothing is written unless all of them do.
template <size_t N>
void ApplyCallGroup(const char* name, const uintptr_t (&sites)[N], uintptr_t expected, const void* wrapper) {
    for (size_t i = 0; i < N; ++i) {
        if (!RelativeCallTargets(sites[i], expected)) {
            logging::Write("%-22s SKIPPED, call %08X was changed", name, static_cast<unsigned>(sites[i]));
            return;
        }
    }

    bool applied = true;
    for (size_t i = 0; i < N; ++i)
        applied &= WriteRelativeBranch(sites[i], 0xE8, wrapper);
    logging::Write("%-22s %s (%u calls)", name, applied ? "patched" : "FAILED", static_cast<unsigned>(N));
}

} // namespace hooking
