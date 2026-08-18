#pragma once

#include <cstddef>
#include <cstdint>

namespace patch {

// True when the whole range is committed and readable.
bool IsReadable(uintptr_t address, size_t size);

// Reads a float or an int32 from the game, returning false when the address is
// not mapped yet.
bool ReadFloat(uintptr_t address, float& value);
bool ReadInt32(uintptr_t address, int32_t& value);

bool WriteMemory(uintptr_t address, const void* data, size_t size);

// An x87 instruction with an absolute [disp32] operand is encoded as
// D8..DF, a ModR/M byte with mod=00 and rm=101, then the 32 bit address.
// Returns true when `instruction` is such an instruction and its operand
// currently equals `expected`.
bool IsAbsoluteX87Operand(uintptr_t instruction, uintptr_t expected);

bool VerifyOperands(const uintptr_t* sites, size_t count, uintptr_t expected);

template <size_t N>
bool VerifyOperands(const uintptr_t (&sites)[N], uintptr_t expected) {
    return VerifyOperands(sites, N, expected);
}

// Verifies every site, then repoints all of them at `target`. Nothing is
// written unless all sites pass, so a group either applies fully or not at all.
bool RepointOperands(const uintptr_t* sites, size_t count, uintptr_t expected,
                     const void* target);

template <size_t N>
bool RepointOperands(const uintptr_t (&sites)[N], uintptr_t expected,
                     const void* target) {
    return RepointOperands(sites, N, expected, target);
}

}  // namespace patch
