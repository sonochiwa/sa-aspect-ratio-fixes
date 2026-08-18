#include "patch.h"

#include <windows.h>

#include <cstring>

namespace patch {
namespace {

constexpr DWORD kReadableProtect = PAGE_READONLY | PAGE_READWRITE |
                                   PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                                   PAGE_EXECUTE_READWRITE |
                                   PAGE_EXECUTE_WRITECOPY;

// Offset of the absolute address inside an x87 [disp32] instruction.
constexpr size_t kOperandOffset = 2;

}  // namespace

bool IsReadable(uintptr_t address, size_t size) {
    MEMORY_BASIC_INFORMATION info = {};
    const auto* pointer = reinterpret_cast<const void*>(address);
    if (VirtualQuery(pointer, &info, sizeof(info)) != sizeof(info))
        return false;
    if (info.State != MEM_COMMIT || (info.Protect & kReadableProtect) == 0)
        return false;

    const auto regionEnd =
        reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return address + size <= regionEnd;
}

bool ReadFloat(uintptr_t address, float& value) {
    if (!IsReadable(address, sizeof(float)))
        return false;
    value = *reinterpret_cast<const float*>(address);
    return true;
}

bool ReadInt32(uintptr_t address, int32_t& value) {
    if (!IsReadable(address, sizeof(int32_t)))
        return false;
    value = *reinterpret_cast<const int32_t*>(address);
    return true;
}

bool WriteMemory(uintptr_t address, const void* data, size_t size) {
    auto* destination = reinterpret_cast<void*>(address);

    DWORD oldProtect = 0;
    if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    std::memcpy(destination, data, size);
    FlushInstructionCache(GetCurrentProcess(), destination, size);

    DWORD restored = 0;
    VirtualProtect(destination, size, oldProtect, &restored);
    return true;
}

bool IsAbsoluteX87Operand(uintptr_t instruction, uintptr_t expected) {
    if (!IsReadable(instruction, kOperandOffset + sizeof(uint32_t)))
        return false;

    const auto* code = reinterpret_cast<const uint8_t*>(instruction);
    if (code[0] < 0xD8 || code[0] > 0xDF)
        return false;
    if ((code[1] & 0xC7) != 0x05)
        return false;

    uint32_t operand = 0;
    std::memcpy(&operand, code + kOperandOffset, sizeof(operand));
    return operand == static_cast<uint32_t>(expected);
}

bool VerifyOperands(const uintptr_t* sites, size_t count, uintptr_t expected) {
    for (size_t i = 0; i < count; ++i) {
        if (!IsAbsoluteX87Operand(sites[i], expected))
            return false;
    }
    return true;
}

bool RepointOperands(const uintptr_t* sites, size_t count, uintptr_t expected,
                     const void* target) {
    if (!VerifyOperands(sites, count, expected))
        return false;

    const auto address = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target));
    for (size_t i = 0; i < count; ++i) {
        if (!WriteMemory(sites[i] + kOperandOffset, &address, sizeof(address)))
            return false;
    }
    return true;
}

}  // namespace patch
