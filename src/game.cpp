#include "game.h"

#include "addresses.h"

#include <cstring>

namespace game_api {
namespace {

using DrawRectFn = int (__cdecl*)(const Rect&, const Color&);
using DefinedState2dFn = void (__cdecl*)();
using AddMessageJumpFn = void (__cdecl*)(const char*, uint32_t, uint16_t, bool);

constexpr Color kBlack = {0, 0, 0, 255};

} // namespace

float ScreenWidth() {
    return static_cast<float>(*reinterpret_cast<const int32_t*>(game::kScreenWidth));
}

float ScreenHeight() {
    return static_cast<float>(*reinterpret_cast<const int32_t*>(game::kScreenHeight));
}

void DrawBlackRect(const Rect& rect) {
    reinterpret_cast<DrawRectFn>(game::kDrawRect)(rect, kBlack);
}

void DefinedState2d() {
    reinterpret_cast<DefinedState2dFn>(game::kDefinedState2d)();
}

int16_t GetCameraMode() {
    const auto* camera = reinterpret_cast<const uint8_t*>(game::kCamera);
    const uint8_t active = camera[game::kCameraActiveIndexOffset];
    if (active >= 3)
        return -1;

    int16_t mode = 0;
    std::memcpy(&mode,
                camera + game::kCameraArrayOffset + active * game::kCameraSize + game::kCameraModeOffset,
                sizeof(mode));
    return mode;
}

bool IsSniperCamera() {
    return GetCameraMode() == game::kCameraModeSniper;
}

void AddMessage(const char* text, uint32_t milliseconds) {
    reinterpret_cast<AddMessageJumpFn>(game::kAddMessageJump)(text, milliseconds, 0, false);
}

} // namespace game_api
