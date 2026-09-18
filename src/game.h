#pragma once

#include "types.h"

#include <cstdint>

// Thin calls into the game used by more than one module.
namespace game_api {

float ScreenWidth();
float ScreenHeight();

// CSprite2d::DrawRect in opaque black.
void DrawBlackRect(const Rect& rect);

// Resets RenderWare state for untextured 2D primitives.
void DefinedState2d();

// eCamMode of the active camera, or -1 when the active index is out of range.
int16_t GetCameraMode();
bool IsSniperCamera();

// CMessages::AddMessageJump. The string is copied by the game.
void AddMessage(const char* text, uint32_t milliseconds);

} // namespace game_api
